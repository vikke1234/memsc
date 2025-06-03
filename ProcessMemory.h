#pragma once
#include <cerrno>
#include <cstring>
#include <algorithm>
#include <variant>
#include <memory>
#include <unordered_set>
#include <set>
#include <sys/mman.h>
#include <sys/uio.h>
#include <unistd.h>
#include <sys/types.h>
#include <limits>
#include <cstdio>
#include <iostream>

#include "maps.h"


using Match = std::variant<std::uint8_t*, std::uint16_t*, std::uint32_t*, std::int32_t*>;
using MatchSet = std::set<Match>;

/*
 * maybe make this into a class? you could place the pid into
 * it which would be nice so you don't have a global variable
 */
class ProcessMemory {
    pid_t _pid{};
    MatchSet matches;

public:
    ssize_t write_process_memory(void *address, void *buffer, const ssize_t n) const;
    ssize_t read_process_memory(void *address, void *buffer, size_t n) const;

    MatchSet &get_matches() {
        return matches;
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
    template <typename T>
    MatchSet scan_range(address_range &range, T value) {
        // We're largely dependent on how large the page size is on how much we can actually read.
        const std::size_t max_size = IOV_MAX * sysconf(_SC_PAGESIZE);
        const std::size_t increments = max_size / sizeof(T);

        MatchSet found;

        std::unique_ptr<T[]> buf{std::make_unique<T[]>(increments)};

        for (char  *start = static_cast<char *>(range.start);
                    start < static_cast<char *>(range.start) + range.length;
                    start += increments) {
            std::uintptr_t ptrdiff = (static_cast<char*>(range.start) + range.length) - start;
            std::size_t size = std::min(increments, ptrdiff);

            ssize_t nread = read_process_memory(start, buf.get(), size);
            if (nread < 0 && nread == size) {
                std::fprintf(
                    stderr,
                    "ProcessMemory: Error reading memory at %p: %s\n",
                    static_cast<void*>(start),
                    std::strerror(errno)
                );
                return found;
            }

            for (int i = 0, j = 0; i < increments && (start + i) < (range.start + range.length); i += sizeof(T), j++) {
                if (buf[j] == value) {
                    assert(start + i < (range.start + range.length));
                    found.insert(std::bit_cast<T*>(start + i));
                }
            }
        }

        return found;
    }

    /**
     * @brief ProcessMemory::scan scans the memory for a certain value
     * @param value
     * @return returns the matches it finds
     */
    template<typename T>
    MatchSet &scan(T value) {

        std::unique_ptr<address_range> list = get_memory_ranges(_pid);
        address_range *current = list.get();

        MatchSet found;
        /*
         * eliminate previous matches, check what the pointer in previous_matches points to and make sure it points to `value`
         * and that it's in current matches
         * TODO:
         * figure out how to quickly check the memory addresses that are in previous matches, the fastest way can't be to use process_vm_readv
         * */
        using Clock = std::chrono::high_resolution_clock;

        auto overall_t0 = Clock::now();

        while (current != nullptr) {
            if (!(current->perms & PERM_EXECUTE) && current->perms & PERM_READ) {
                fprintf(stdout,
                        "Scanning %s: %p - %p (size: %zx)\n",
                        current->name,
                        current->start,
                        static_cast<char*>(current->start) + current->length,
                        current->length);

                const size_t region_bytes = current->length;
                const size_t num_addresses = region_bytes / sizeof(T);

                auto t0 = Clock::now();
                MatchSet current_matches = scan_range<T>(*current, value);
                found.insert(current_matches.begin(), current_matches.end());

                auto t1 = Clock::now();

                std::chrono::duration<double> elapsed = t1 - t0;
                double seconds = elapsed.count();

                double rate = 0.0;
                if (seconds > 0.0) {
                    rate = static_cast<double>(num_addresses) / seconds;
                }

                fprintf(stdout,
                        "    → Scanned %zu addresses in %.5f s (%.0f addresses/s)\n",
                        num_addresses,
                        seconds,
                        rate);

            }

            current = current->next.get();
        }

        if (!matches.empty()) {
            std::erase_if(matches, [&](auto const &x) { return found.find(x) == found.end(); });
        } else {
            matches = found;
        }
        auto overall_t1 = Clock::now();
        std::chrono::duration<double> total_elapsed = overall_t1 - overall_t0;
        double total_seconds = total_elapsed.count();
        fprintf(stdout, "Total scan time: %.3f s\n", total_seconds);
        fprintf(stdout, "matches: %lu\n", matches.size());
        return matches;
    }
};
