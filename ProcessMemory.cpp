#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/uio.h>
#include <stdint.h>
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
ssize_t ProcessMemory::read_process_memory(pid_t pid, void *address, void *buffer, size_t n) {
    static const size_t page_size = (size_t) sysconf(_SC_PAGESIZE);

    unsigned long amount_of_iovecs = 1;
    if (page_size < n) {
        amount_of_iovecs = n / page_size;
        if(amount_of_iovecs > (unsigned long) sysconf(_SC_IOV_MAX)) {
            fprintf(stderr, "Error could not read process memory, too much to read\n");
            return 0;
        }
    }

    iovec *remote = new iovec[amount_of_iovecs];
    iovec local;
    local.iov_len = n;
    local.iov_base = buffer;
    /* bytes remaining after filled all of the remotes except last */
    size_t remeinder_of_bytes = n % page_size;
    unsigned long i = 0;

    if(n > page_size) {
        do {
            remote[i].iov_base = (char *)address + (i * page_size);
            remote[i].iov_len = page_size;
            n -= page_size;
        } while (i++ < amount_of_iovecs - 1);

        if (remeinder_of_bytes > 0 && amount_of_iovecs > 1) {
            remote[i].iov_base = (char *)address + (i * page_size);
            remote[i].iov_len = n;
        }
    } else {
        remote[i].iov_len = n;
        remote[i].iov_base = address;

    }
    ssize_t amount_read = process_vm_readv(pid, &local, 1, remote, amount_of_iovecs, 0);
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
ssize_t ProcessMemory::write_process_memory(pid_t pid, void *address, void *buffer, ssize_t n) {
    iovec local, remote;
    /* this might have to be made so that if n > _SC_PAGESIZE
     * local would be split into multiple locals, similar to how
     * read_process_memory works, no fucking clue though if it's necessary */
    remote.iov_base = address;
    remote.iov_len = n;
    local.iov_base = buffer;
    local.iov_len = n;
    ssize_t amount_read = process_vm_writev(pid, &local, 1, &remote, 1, 0);
    return amount_read;
}
/**
 * @brief ProcessMemory::scan_range scans a section of a process memory
 * @param start pointer to where it should start
 * @param length length of the section it should scan
 * @param value value to look for
 * @return
 */
template <typename T>
std::unordered_set<void*> *scan_range(pid_t pid, char *start, unsigned long length, T value) {
    std::unordered_set<void *> *matches = new std::unordered_set<void*>;
    if (matches == nullptr) {
        fprintf(stderr, "Error could not allocate set for matches: %s\n", strerror(errno));
        return nullptr;
    }

    /* remote: where to read from and how much
     * local: buf to store data and size of each buf */

    /* TODO:
     * split remote into smaller pieces so it doesn't span a page */
    T *buffer = nullptr;
    buffer =  new T[length];
    if (buffer == nullptr) {
        perror("scan_range");
        delete matches;
        delete[] buffer;
        return nullptr;
    }
    ssize_t n = ProcessMemory::read_process_memory(pid, start, buffer, length);
    if(n == -1) {
        fprintf(stderr, "Error: %s, %p - %p\n", strerror(errno), (void *)start, (void *)(start + length));

        delete matches;
        delete[] buffer;
        return nullptr;
    }

    for(unsigned long i = 0; (unsigned long) i < n; i++) {
        if(buffer[i] == value) {
            matches->insert(start + i * sizeof(T));
        }
    }

    delete[] buffer;
    return matches;
}

/* I shouldn't write to previous matches, instead return a new one and then compare in search_functions */
/**
 * @brief ProcessMemory::scan scans the memory for a certain value
 * @param previous_matches if equal to nullptr it will allocate a new one and return that
 * @param value
 * @return returns the matches it finds
 */
std::unordered_set<void *> *ProcessMemory::scan(pid_t pid, std::unordered_set<void *> *previous_matches,  int value) {
    if (previous_matches == nullptr) {
        return nullptr;
    }
    address_range *list = get_memory_ranges(pid);
    address_range *current = list;
    if(list == nullptr) {
        return nullptr;
    }
    std::unordered_set<void *> *matches = new std::unordered_set<void *>;
    /*
     * eliminate previous matches, check what the pointer in previous_matches points to and make sure it points to `value`
     * and that it's in current matches
     * TODO:
     * figure out how to quickly check the memory addresses that are in previous matches, the fastest way can't be to use process_vm_readv
     * */
    fprintf(stdout, "Scanning for: %d\n", value);
    while(current != nullptr) {
        if (!(current->perms & PERM_EXECUTE)) { /* look for data sections */
            fprintf(stdout, "Scanning %s: %p - %p\n", current->name, current->start, (char*)current->start+current->length);
            std::unordered_set<void *> *current_matches = scan_range<int>(pid, (char*) current->start, current->length, value);
            if(current_matches != nullptr) {
                matches->insert(current_matches->begin(), current_matches->end());
                delete current_matches;
            }
        }

        current = current->next;
    }
    fprintf(stdout, "matches: %lu\n", previous_matches->size());
    free_address_range(list);
    return matches;
}
