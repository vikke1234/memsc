#ifndef PROCESSMEMORY_H
#define PROCESSMEMORY_H
#include <stdlib.h>
#include <unordered_set>

/*
 * maybe make this into a class? you could place the pid into
 * it which would be nice so you don't have a global variable
 */
namespace ProcessMemory {
    //std::unordered_set<void *> *scan_range(char *start, unsigned long length, int32_t value);
    /* TODO make this into a template so you can look for any type */
    std::unordered_set<void *> *scan(std::unordered_set<void *> *previous_matches, int value);
    ssize_t write_process_memory(pid_t pid, void *address, void *buffer, ssize_t n);
    void attach(pid_t pid);
}

#endif // PROCESSMEMORY_H
