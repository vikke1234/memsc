#pragma once
#include "maps.h"

#include <immintrin.h>
#include <cerrno>
#include <cstring>
#include <algorithm>
#include <variant>
#include <memory>
#include <unordered_set>
#include <unistd.h>
#include <sys/types.h>
#include <cstdio>
#include <iostream>
#include <vector>
#include <atomic>
#include <cassert>
#include <chrono>
#include <functional>


using Match = std::variant<std::uint8_t *, std::uint16_t *, std::uint32_t *>;
using MatchSet = std::unordered_set<Match>;

/*
 * maybe make this into a class? you could place the pid into
 * it which would be nice so you don't have a global variable
 */
class ProcessMemory {
    pid_t _pid{};
    std::vector<Match> matches;
    std::atomic<bool> scanning_;
    std::function<void(size_t, size_t)> progressCallback_;

public:
    ssize_t write_process_memory(void *address, void *buffer, const ssize_t n) const;

    ssize_t read_process_memory(void *address, void *buffer, size_t n) const;

    decltype(matches) &get_matches() {
        return matches;
    }

    bool scanning() {
        return scanning_.load();
    }

    void setProgressCallback(std::function<void(size_t, size_t)> cb) {
        progressCallback_ = cb;
    }

    void pid(const pid_t p) {
        _pid = p;
    }

    pid_t pid() const {
        return _pid;
    }

    /**
     * @brief ProcessMemory::scan_range scans a section of a process
     * @param range Memory range information
     * @param value value to look for
     * @return
     */
    template<typename T>
    std::vector<T *> scan_range(const address_range &range, const T value) {
        // We're largely dependent on how large the page size is on how much we can actually read.
        constexpr std::size_t max_size = IOV_MAX * 4096;
        constexpr std::size_t increments = max_size / sizeof(T);

        std::vector<T *> found;

        std::unique_ptr<T[]> buf(new(std::align_val_t(64)) T[increments]);
        char *end = static_cast<char *>(range.start) + range.length;

        for (char *start = static_cast<char *>(range.start);
             start < end;
             start += increments) {
            std::uintptr_t ptrdiff = end - start;
            ssize_t size = std::min(max_size, ptrdiff);

            ssize_t nread = read_process_memory(start, buf.get(), size);
            if (nread < 0 && nread == size) {
                std::fprintf(
                    stderr,
                    "ProcessMemory: Error reading memory at %p: %s\n",
                    static_cast<void *>(start),
                    std::strerror(errno)
                );
                return found;
            }
            auto addresses = filter_results(value, start, buf.get(), increments);
            found.insert(found.end(), addresses.begin(), addresses.end());
        }

        return found;
    }

    /**
     * @brief ProcessMemory::scan scans the memory for a certain value
     * @param value
     * @return returns the matches it finds
     */
    template<typename T>
    decltype(matches) &scan(T value) {
        scanning_ = true;
        using Clock = std::chrono::high_resolution_clock;

        auto overall_t0 = Clock::now();

        if (matches.size() == 0) {
            matches = initial_scan<T>(value);
        } else {
            scan_found<T>(value);
        }

        auto overall_t1 = Clock::now();
        std::chrono::duration<double> total_elapsed = overall_t1 - overall_t0;
        double total_seconds = total_elapsed.count();
        fprintf(stdout, "Total scan time: %.3f s\n", total_seconds);
        fprintf(stdout, "matches: %lu\n", matches.size());
        scanning_ = false;
        return matches;
    }

private:
    template<typename T>
    void scan_found(T value) {
        std::size_t found = 0;
        for (std::size_t i = 0; i < matches.size(); ++i) {
            auto &match = matches[i];
            std::visit([&]<typename U>(U *addr) {
                if constexpr (std::is_same_v<T, U>) {
                    if (addr == nullptr) {
                        return;
                    }
                    U new_value{};
                    [[maybe_unused]] ssize_t n = read_process_memory(addr, &new_value, sizeof(new_value));
                    assert(n == sizeof(T));
                    if (new_value != value) {
                        matches[i] = static_cast<T *>(nullptr);
                    } else {
                        matches[found++] = addr;
                    }
                }
            }, match);
        }
        matches.resize(found);
    }

    template<typename T>
    std::vector<Match> initial_scan(T value) {
        using Clock = std::chrono::high_resolution_clock;
        std::unique_ptr<address_range> list = get_memory_ranges(_pid);
        address_range *current = list.get();
        std::vector<Match> found;
        size_t total_size = get_address_range_list_size(current, false);
        size_t scanned_size = 0;
        while (current != nullptr) {
            // Do not scan vvar & vvar_clock, not permitted.
            if (!(current->perms & PERM_EXECUTE) && current->perms & PERM_READ &&
                std::strncmp(current->name, "[vvar]", 4096) && std::strncmp(current->name, "[vvar_vclock]", 4096)) {
                fprintf(stdout,
                        "Scanning %s: %p - %p (size: %zx)\n",
                        current->name,
                        current->start,
                        static_cast<char *>(current->start) + current->length,
                        current->length);

                const size_t region_bytes = current->length;
                const size_t num_addresses = region_bytes / sizeof(T);

                auto t0 = Clock::now();
                auto current_matches = scan_range<T>(*current, value);
                found.insert(found.end(), current_matches.begin(), current_matches.end());

                auto t1 = Clock::now();

                std::chrono::duration<double> elapsed = t1 - t0;
                double seconds = elapsed.count();
                fprintf(stdout,
                        "    → Scanned %zu addresses in %.5f s\n",
                        num_addresses,
                        seconds);
                scanned_size += current->length;
                progressCallback_(scanned_size, total_size);
            }

            current = current->next.get();
        }
        return found;
    }

    /**
     * @brief Finds addresses containing the value given by `value`
     *
     * TODO: this could probably be an actual pattern matching library.
     *
     * @tparam T Type of value to look for
     * @param value value to look for
     * @param start start of the memory range
     * @param buf buffer containing read values
     * @param count elements in the buffer
     * @return vector containing addresses that match the value
     */
    template<typename T>
    std::vector<T *> filter_results(T value, char *start, T *buf, size_t count) {
        std::vector<T *> found{};
        // Expect ~33% hit-rate
        found.reserve(count / 33);

        if constexpr (__AVX2__) {
            size_t i = 0;
            size_t aligned_end = (count / 8) * 8; // 8 lanes of int32_t
            const __m256i val_vec = _mm256_set1_epi32(static_cast<int32_t>(value));

            for (; i < aligned_end; i += 8) {
                if (i % 4096 == 0) {
                    __builtin_prefetch(buf + i + 4096);
                }

                __m256i chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(&buf[i]));
                __m256i cmp = _mm256_cmpeq_epi32(chunk, val_vec);
                int mask = _mm256_movemask_ps(_mm256_castsi256_ps(cmp));
                while (mask) {
                    int lane = __builtin_ctz(mask); // [0..7]
                    found.push_back(reinterpret_cast<T *>(start) + i + static_cast<size_t>(lane));
                    mask &= (mask - 1);
                }
            }

            // Clean up if not aligned
            for (size_t i = aligned_end; i < count; ++i) {
                if (buf[i] == value) {
                    found.push_back(
                        reinterpret_cast<T *>(start) + i
                    );
                }
            }
        } else {
            for (size_t i = 0; i < count; i++) {
                if (buf[i] == value) {
                    assert(start + i < (static_cast<char*>(range.start) + range.length));
                    found.push_back(reinterpret_cast<T *>(start + i * sizeof(T)));
                }
            }
        }
        return found;
    }
};
