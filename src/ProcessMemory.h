#pragma once
#include "maps.h"
#include "perf.h"

#include <cstddef>
#include <filesystem>
#include <immintrin.h>
#include <cerrno>
#include <cstring>
#include <algorithm>
#include <sys/mman.h>
#include <type_traits>
#include <variant>
#include <memory>
#include <unistd.h>
#include <sys/types.h>
#include <cstdio>
#include <vector>
#include <atomic>
#include <cassert>
#include <chrono>
#include <functional>
#include <iostream>


using Match = std::variant<std::uint8_t *, std::uint16_t *, std::uint32_t *, std::uint64_t *, float *, double *>;

/*
 * maybe make this into a class? you could place the pid into
 * it which would be nice so you don't have a global variable
 */
class ProcessMemory {
    std::size_t max_read_size_ = 0x10000000;
    double epsilon_ = 0.000000001;
    std::vector<void *> matches{};
    std::function<void(size_t, size_t)> progressCallback_{};
    std::atomic<bool> scanning_{};

    pid_t pid_{-1};

public:
    ssize_t write_process_memory(void *address, void *buffer,
                                 const ssize_t n) const;

    static void prefetch_area(pid_t pid, void *address, size_t size);
    ssize_t read_process_memory(void *address, void *buffer, size_t n) const;
	static ssize_t read_process_memory_nosplit(pid_t pid, void *address,
                                            void *buffer, size_t n);

    decltype(matches) &get_matches() {
        return matches;
    }

    bool scanning() const {
        return scanning_.load();
    }

    void setProgressCallback(std::function<void(size_t, size_t)> cb) {
        progressCallback_ = cb;
    }

    void max_read_size(std::size_t size) {
        // TODO: fix race condition
        max_read_size_ = size;
    }

    std::size_t max_read_size() const {
        return max_read_size_;
    }

    void epsilon(double e) {
        epsilon_ = e;
    }

    bool pid(const pid_t p) {
        if (!std::filesystem::exists("/proc/" + std::to_string(p))) {
            std::cout << "Could not attach to: " << p << "\n";
            return false;
        }

        pid_ = p;
        return true;
    }

    pid_t pid() const {
        return pid_;
    }

    /**
     * @brief ProcessMemory::scan_range scans a section of a process
     * @param range Memory range information
     * @param value value to look for
     * @return
     */
    template<typename T>
    void scan_range(std::vector<void *>& found, const address_range &range,
                    const T value) {
        // We're largely dependent on how large the page size is on how much we can actually read.
        const std::size_t count = max_read_size_ / sizeof(T);

        size_t bufsize = std::min(range.length / sizeof(T), count);
        alignas(64) std::unique_ptr<T[]> buf = std::make_unique<T[]>(bufsize);
        if (range.length > 4096) {
            prefetch_area(pid_, range.start, range.length);
        }
        char *end = static_cast<char *>(range.start) + range.length;

        for (char *start = reinterpret_cast<char *>(range.start);
             start < end;
             start += max_read_size_) {
            std::uintptr_t ptrdiff = end - start;
            ssize_t size = std::min(max_read_size_, ptrdiff);

            ssize_t nread = read_process_memory_nosplit(pid_, start, buf.get(), size);
            if (nread < 0 || nread != size) {
                std::fprintf(
                    stderr,
                    "ProcessMemory: Error partial read at %p: %s\n",
                    static_cast<void *>(start),
                    std::strerror(errno)
                );
                return;
            }
            filter_results(found, value, start, buf.get(), nread / sizeof(T));
        }
    }

    /**
     * @brief ProcessMemory::scan scans the memory for a certain value
     * @param value
     * @return returns the matches it finds
     */
    template<typename T>
    void scan(T value) {
        scanning_ = true;
        using Clock = std::chrono::high_resolution_clock;
        std::cout << "epsilon: " << epsilon_ << "\n";
        perf_event_attr  pe {
            .type = PERF_TYPE_HARDWARE,
            .size = sizeof(perf_event_attr),
            .config = PERF_COUNT_HW_INSTRUCTIONS,
            .disabled = 1,
            .exclude_kernel = 1,
            .exclude_hv = 1
        };
        int fd = perf_event_open(&pe, 0, -1, -1, 0);
        if (fd == -1) {
            std::cout << "Error could not open perf event";
        }
        ioctl(fd, PERF_EVENT_IOC_RESET, 0);
        ioctl(fd, PERF_EVENT_IOC_ENABLE, 0);

        auto overall_t0 = Clock::now();

        if (matches.size() == 0) {
            matches = initial_scan<T>(value);
        } else {
            scan_found<T>(value);
        }

        ioctl(fd, PERF_EVENT_IOC_DISABLE, 0);
        long long cycles{};

        read(fd, &cycles, sizeof(long long));
        auto overall_t1 = Clock::now();
        std::chrono::duration<double> total_elapsed = overall_t1 - overall_t0;
        double total_seconds = total_elapsed.count();
        fprintf(stdout, "Total scan time: %.3f s\n", total_seconds);
        fprintf(stdout, "Total cycles spent %lld\n", cycles);
        fprintf(stdout, "matches: %lu\n", matches.size());
        scanning_ = false;
    }

private:
    template<typename T>
    void scan_found(T value) {
        std::size_t found = 0;
        for (std::size_t i = 0; i < matches.size(); ++i) {
            void *addr = matches[i];
            if (addr == nullptr) {
                return;
            }
            T new_value{};
            [[maybe_unused]] ssize_t n =
                read_process_memory_nosplit(pid_, addr, &new_value,
                                            sizeof(new_value));
            assert(n == sizeof(T));
            if (new_value != value) {
                matches[i] = static_cast<T *>(nullptr);
            } else {
                matches[found++] = addr;
            }
        }
        matches.resize(found);
    }

    template<typename T>
    std::vector<void *> initial_scan(T value) {
        std::vector<address_range> list = get_memory_ranges(pid_, false);
        std::vector<void *> found;
        size_t total_size = get_address_range_list_size(list, false);
        size_t scanned_size = 0;
        for (address_range &current : list) {
            if (current.perms & PERM_READ) {
                scan_range<T>(found, current, value);

                scanned_size += current.length;
                progressCallback_(scanned_size, total_size);
            }
        }
        printf("Scanned %'lu bytes (%.02f GB)\n", total_size, double(total_size) / 1e9);
        return found;
    }

    /**
     * @brief Finds addresses containing the value given by `value`
     *
     * TODO: this could probably be an actual pattern matching library.
     *
     * @tparam T Type of value to look for
     * @param found vector of currently found results
     * @param value value to look for
     * @param start start of the memory range
     * @param buf buffer containing read values
     * @param count elements in the buffer
     * @return vector containing addresses that match the value
     */
    template<typename T>
    void filter_results(std::vector<void*> &found, T value,
                        char *start, T *buf, size_t count) {
        size_t i = 0;

#ifdef __AVX2__
        if constexpr (!std::is_floating_point_v<T> && sizeof(T) == 4) {
            constexpr size_t lanes = 8;
            size_t aligned_end = (count / lanes) * lanes; // 8 lanes of int32_t
            const __m256i val_vec = _mm256_set1_epi32(static_cast<int32_t>(value));

            for (; i < aligned_end; i += lanes) {
                if (i % (4096 / sizeof(T)) == 0) {
                    _mm_prefetch(&buf[i + (4096 / sizeof(T))], _MM_HINT_NTA);
                }
                __m256i chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(&buf[i]));
                __m256i cmp = _mm256_cmpeq_epi32(chunk, val_vec);
                int mask = _mm256_movemask_ps(_mm256_castsi256_ps(cmp));
                char* base = start + i * sizeof(T);
                while (mask) {
                    unsigned int lane = __builtin_ctz(mask); // [0..7]
                    found.push_back(
                        reinterpret_cast<T *>(base + lane * sizeof(T)));
                    mask &= (mask - 1);
                }
            }

            i = aligned_end;
        }
#endif // __AVX2__

        // "Default implementation" as well as cleanup for AVX implementation
        for (; i < count; i++) {
            if constexpr (std::is_floating_point_v<T>) {
                double val = std::abs(buf[i] - value);
                if ((start + i * sizeof(T)) == (char*)0x7ffd3a28cdf0) {
                    std::cout << "val: " << val << " buf[i] " << buf[i]<< "\n";
                }
                if (val < epsilon_) {
                    found.push_back(
                        reinterpret_cast<T *>(start + i * sizeof(T)));
                }
            } else {
                if (buf[i] == value) {
                    found.push_back(
                        reinterpret_cast<T *>(start + i * sizeof(T)));
                }
            }
        }
    }
};
