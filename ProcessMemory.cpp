#include <stdlib.h>
#include <sys/mman.h>
#include <sys/uio.h>
#include <unistd.h>
#include <sys/types.h>

#include <QDebug>

#include "ProcessMemory.h"


    /**
     * @brief read_process_memory reads `n` amount of bytes from a process
     * @param address starting address
     * @param buffer to store read items into
     * @param n amount to read
     * @return bytes read
     */
    ssize_t ProcessMemory::read_process_memory(void *address, void *buffer, size_t n) const {
        const size_t page_size = static_cast<size_t>(sysconf(_SC_PAGESIZE));
        size_t num_vecs = (n + page_size - 1) / page_size;
        auto max_iov = static_cast<unsigned long>(sysconf(_SC_IOV_MAX));
        if (num_vecs > max_iov) {
            fprintf(stderr, "ProcessMemory: Error could not read process memory, too many iovecs %zu > %lu\n",
                    num_vecs, max_iov);
            return 0;
        }

        std::unique_ptr<iovec[]> remote = std::make_unique<iovec[]>(num_vecs);
        size_t remaining = n;
        size_t offset = 0;
        for (size_t i = 0; i < num_vecs; ++i) {
            size_t chunk = remaining < page_size ? remaining : page_size;
            remote[i].iov_base = static_cast<char *>(address) + offset;
            remote[i].iov_len  = chunk;
            offset    += chunk;
            remaining -= chunk;
        }

        iovec local { .iov_base = buffer, .iov_len = n };
        ssize_t amount_read = process_vm_readv(_pid, &local, 1,
            remote.get(), num_vecs, 0);

        if (amount_read == -1) {
            char error[512] = {};
            snprintf(error, sizeof(error),
                     "ProcessMemory: process_vm_readv error %p - %p",
                     address, static_cast<char *>(address) + n);
            perror(error);
        }

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
    ssize_t ProcessMemory::write_process_memory(void *address, void *buffer, const ssize_t n) const {
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
