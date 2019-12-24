#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/uio.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>

#include <QDebug>
#include <iterator>

#include "ProcessMemory.h"
#include "maps.h"

static pid_t g_pid = 50482;

void ProcessMemory::attach(pid_t pid) {
       g_pid = pid;
}

/**
 * @brief read_process_memory reads `n` amount of bytes from a process
 * @param address starting address
 * @param buffer to store read items into
 * @param n amount to read
 * @return bytes read
 */
ssize_t read_process_memory(pid_t pid, void *address, void *buffer, size_t n) {
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
std::unordered_set<void*> *scan_range(char *start, unsigned long length, T value) {
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
        fprintf(stderr, "Error: %s\n", strerror(errno));
        delete matches;
        return nullptr;
    }
    ssize_t n = read_process_memory(g_pid, start, buffer, length);
    if(n == -1) {
        fprintf(stderr, "Error: %s, %p - %p\n", strerror(errno), (void *)start, (void *)(start + length));

        delete matches;
        delete[] buffer;
        return nullptr;
    }

    for(int i = 0; i < n; i++) {
        if(buffer[i] == value) {
            matches->insert(start + (unsigned long)i * sizeof(T));
        }
    }

    delete[] buffer;
    return matches;
}


/* this will probably have to return some sort of unordered_set or something for it to compare older results with */
/**
 * @brief ProcessMemory::scan scans the memory for a certain value
 * @param previous_matches if equal to nullptr it will allocate a new one and return that
 * @param value
 * @return returns the matches it finds
 */
std::unordered_set<void *> *ProcessMemory::scan(std:: unordered_set<void *> *previous_matches,  int value) {
    address_range *list = get_memory_ranges(g_pid);
    address_range *current = list;
    if (previous_matches == nullptr) {
        previous_matches = new std::unordered_set<void *>;
    }
    /*
     * eliminate previous matches, check what the pointer in previous_matches points to and make sure it points to `value`
     * and that it's in current matches
     * TODO:
     * figure out how to quickly check the memory addresses that are in previous matches, the fastest way can't be to use process_vm_readv
     * */
    fprintf(stderr, "Scanning for: %d\n", value);
    while(current != nullptr) {
        if (!(current->perms & PERM_EXECUTE)) { /* look for data sections */
            fprintf(stderr, "Scanning %s: %p - %p\n", current->name, current->start, (char*)current->start+current->length);
            std::unordered_set<void *> *matches = scan_range<int>((char*) current->start, current->length, value);
            if(matches != nullptr) {
                previous_matches->insert(matches->begin(), matches->end());
                delete matches;
            }
        }

        current = current->next;
    }
    fprintf(stderr, "matches: %lu\n", previous_matches->size());
    free_address_range(list);
    return previous_matches;
}
