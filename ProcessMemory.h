#pragma once
#include <cerrno>
#include <cstring>
#include <memory>
#include <unordered_set>
#include <sys/mman.h>
#include <sys/uio.h>
#include <unistd.h>
#include <sys/types.h>

#include <cstdio>

#include "maps.h"

/*
 * maybe make this into a class? you could place the pid into
 * it which would be nice so you don't have a global variable
 */
class ProcessMemory {
    pid_t _pid{};
    std::unordered_set<void *> matches;

public:
    std::unordered_set<void *> &get_matches() {
        return matches;
    }

    void pid(const pid_t p) {
        _pid = p;
    }

    pid_t pid() const {
        return _pid;
    }

    /**
     * @brief ProcessMemory::scan_range scans a section of a process memory
     * @param start pointer to where it should start
     * @param length length of the section it should scan
     * @param value value to look for
     * @return
     */
    template <typename T>
    std::unordered_set<void*> scan_range(char *start, const unsigned long length, T value) {
        // We limit each read to at most 4096 MiB (1 << 32 bytes) per chunk.
        constexpr std::size_t max_chunk_bytes = 4096;

        std::unordered_set<void*> found;

        // Number of bytes remaining to scan
        unsigned long bytes_remaining = length;
        // Offset (in bytes) from `start` where the next chunk begins
        unsigned long offset = 0;

        while (bytes_remaining > 0) {
            // Determine how many bytes we'll read in this chunk
            auto chunk_bytes = static_cast<std::size_t>(std::min<unsigned long>(bytes_remaining, max_chunk_bytes));

            // Compute how many T‐elements fit in this chunk.
            // If chunk_bytes isn't a multiple of sizeof(T), we round down,
            // but still read the full chunk_bytes from the remote process.
            std::size_t chunk_elems = chunk_bytes / sizeof(T);
            if (chunk_elems == 0) {
                // If sizeof(T) > chunk_bytes, we still want to read at least
                // one T’s worth of data (so that we don't skip a potential match).
                // In that case, bump chunk_elems to 1 and adjust chunk_bytes accordingly.
                chunk_elems = 1;
                chunk_bytes = sizeof(T);
            } else {
                // Ensure we don't read past the intended range:
                // round chunk_bytes down to a multiple of sizeof(T).
                chunk_bytes = chunk_elems * sizeof(T);
            }

            // Allocate a local buffer to hold chunk_elems elements of type T
            std::unique_ptr<T[]> buffer = std::make_unique<T[]>(chunk_elems);

            // Attempt to read from the remote process:
            ssize_t nread = read_process_memory(start + offset, buffer.get(), chunk_bytes);
            if (nread < 0) {
                std::fprintf(
                    stderr,
                    "Error reading memory at %p: %s\n",
                    static_cast<void*>(start + offset),
                    std::strerror(errno)
                );
                // In case of an error, stop scanning and return what we have so far.
                return found;
            }

            // Number of complete T‐elements that we actually read:
            std::size_t elems_read = static_cast<std::size_t>(nread) / sizeof(T);

            // Scan the local buffer for matches
            for (std::size_t i = 0; i < elems_read; ++i) {
                if (buffer[i] == value) {
                    // The address of this match in the remote process:
                    void* match_addr = start + offset + i * sizeof(T);
                    found.insert(match_addr);
                }
            }

            // Advance our window
            offset += chunk_bytes;
            bytes_remaining -= std::min<unsigned long>(bytes_remaining, static_cast<unsigned long>(chunk_bytes));
        }
        return found;
    }
    /**
     * @brief ProcessMemory::scan scans the memory for a certain value
     * @param value
     * @return returns the matches it finds
     */
    template<typename T>
    std::unordered_set<void *> &scan(T value) {

        std::unique_ptr<address_range> list = get_memory_ranges(_pid);
        address_range *current = list.get();

        std::unordered_set<void *> found;
        /*
         * eliminate previous matches, check what the pointer in previous_matches points to and make sure it points to `value`
         * and that it's in current matches
         * TODO:
         * figure out how to quickly check the memory addresses that are in previous matches, the fastest way can't be to use process_vm_readv
         * */
        while(current != nullptr) {
            if (!(current->perms & PERM_EXECUTE)) { /* look for data sections */
                fprintf(stdout, "Scanning %s: %p - %p (size: %zx)\n", current->name, current->start,
                        (char *) current->start + current->length, current->length);
                std::unordered_set<void *> current_matches = scan_range<int>(static_cast<char *>(current->start), current->length, value);
                found.insert(current_matches.begin(), current_matches.end());
            }

            current = current->next.get();
        }
        matches.merge(found);
        fprintf(stdout, "matches: %lu\n", matches.size());
        return matches;
    }

    /**
     * @brief read_process_memory reads `n` amount of bytes from a process
     * @param address starting address
     * @param buffer to store read items into
     * @param n amount to read
     * @return bytes read
     */
    ssize_t read_process_memory(void *address, void *buffer, size_t n) const {
        const size_t page_size = static_cast<size_t>(sysconf(_SC_PAGESIZE));

        unsigned long amount_of_iovecs = 1;
        if (page_size < n) {
            amount_of_iovecs = n / page_size;
            auto max_iov = static_cast<unsigned long>(sysconf(_SC_IOV_MAX));
            if(amount_of_iovecs > max_iov) {
                fprintf(stderr, "Error could not read process memory, too much to read %lu > %lu\n", amount_of_iovecs, max_iov);
                return 0;
            }
        }

        iovec *remote = new iovec[amount_of_iovecs];
        iovec local{.iov_base = buffer, .iov_len = n};
        /* bytes remaining after filled all of the remotes except last */
        size_t remeinder_of_bytes = n % page_size;
        unsigned long i = 0;

        if(n > page_size) {
            do {
                remote[i].iov_base = static_cast<char *>(address) + (i * page_size);
                remote[i].iov_len = page_size;
                n -= page_size;
            } while (i++ < amount_of_iovecs - 1);

            if (remeinder_of_bytes > 0 && amount_of_iovecs > 1) {
                remote[i].iov_base = static_cast<char *>(address) + (i * page_size);
                remote[i].iov_len = n;
            }
        } else {
            remote[i].iov_len = n;
            remote[i].iov_base = address;

        }
        const ssize_t amount_read = process_vm_readv(_pid, &local, 1, remote, amount_of_iovecs, 0);
        delete[] remote;
        return amount_read;
    }

    /**
     * @brief write_process_memory Writes to the memory of a given process
     * @param pid                  Program pid
     * @param address              The base memory address
     * @param buffer               Buffer to write
     * @param n                    How many bytes to write
     * @return                     Returns bytes written
     */
    ssize_t write_process_memory(void *address, void *buffer, const ssize_t n) const {
        iovec local, remote;
        /* this might have to be made so that if n > _SC_PAGESIZE
         * local would be split into multiple locals, similar to how
         * read_process_memory works, no fucking clue though if it's necessary */
        remote.iov_base = address;
        remote.iov_len = n;
        local.iov_base = buffer;
        local.iov_len = n;
        ssize_t amount_read = process_vm_writev(_pid, &local, 1, &remote, 1, 0);
        return amount_read;
    }
};
