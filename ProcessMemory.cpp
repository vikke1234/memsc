#include <stdlib.h>
#include <sys/mman.h>
#include <sys/uio.h>
#include <unistd.h>
#include <sys/types.h>

#include <QDebug>

#include "ProcessMemory.h"
#include "maps.h"


    /**
     * @brief read_process_memory reads `n` amount of bytes from a process
     * @param address starting address
     * @param buffer to store read items into
     * @param n amount to read
     * @return bytes read
     */
    ssize_t ProcessMemory::read_process_memory(void *address, void *buffer, size_t n) const {
        const size_t page_size = static_cast<size_t>(sysconf(_SC_PAGESIZE));

        unsigned long amount_of_iovecs = 1;
        if (page_size < n) {
            amount_of_iovecs = n / page_size;
            auto max_iov = static_cast<unsigned long>(sysconf(_SC_IOV_MAX));
            if(amount_of_iovecs > max_iov) {
                fprintf(stderr, "ProcessMemory: Error could not read process memory, too much to read %lu > %lu\n", amount_of_iovecs, max_iov);
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
        if (amount_read == -1) {
            perror("ProcessMemory: process_vm_readv error");
        }
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
