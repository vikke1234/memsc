#ifndef SEARCH_FUNCTIONS_H
#define SEARCH_FUNCTIONS_H

#include <unordered_set>
#include <ProcessMemory.h>
#include <stdio.h>
#include <string.h>
#include <cstdint>

class search_functions {
private:
    pid_t pid = 60924;
    /* this might be made into a unordered_map you can keep track of the values in them, <void *, std::variant<...>
     * similar to how it's done in the mainwindow.cpp currently, but probably will never happen, dunno */
    std::unordered_set<void *> *matches = nullptr;

    std::unordered_set<void *> *compare_results(std::unordered_set<void *> *previous, std::unordered_set<void *> *current);
public:
    search_functions();
    ~search_functions();
    void clear_results(void);
    void scan_for(uint8_t value);
    void scan_for(uint16_t value);
    void scan_for(uint32_t value);
    void scan_for(uint8_t *byte_array);
    bool write(void *address, void *value, ssize_t size);

    std::unordered_set<void *> *get_matches() const;
    pid_t get_pid() const;
};

#endif // SEARCH_FUNCTIONS_H
