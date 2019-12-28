#include "search_functions.h"
#include <ProcessMemory.h>
#include <chrono>
#include <QDebug>

search_functions::search_functions() {
    this->matches = new std::unordered_set<void *>;
}

search_functions::~search_functions() {
    delete matches;
    matches = nullptr;
}

void search_functions::scan_for(uint32_t value) {
    std::unordered_set<void *> *results = ProcessMemory::scan(this->matches, value);
    auto start = std::chrono::high_resolution_clock::now();
    if(this->matches->empty()) {
        this->matches->insert(results->begin(), results->end());
    }
    std::unordered_set<void *> *intersection = compare_results(this->matches, results);
    this->matches->clear();
    this->matches->insert(intersection->begin(), intersection->end());
    auto finish = std::chrono::high_resolution_clock::now();
    qDebug() << "time: " << (finish - start).count();
}

/**
 * @brief search_functions::compare_results gets the intersection of both sets
 * @param previous
 * @param current
 */
std::unordered_set<void *> *search_functions::get_matches() const
{
    return matches;
}

std::unordered_set<void *> *search_functions::compare_results(std::unordered_set<void *> *previous, std::unordered_set<void *> *current) {
    std::unordered_set<void *> *intersection = new std::unordered_set<void *>;
    for (auto it = previous->begin(); it != previous->end(); it++) {
        if(current->find(*it) != current->end()) {
            intersection->insert(*it);
        }
    }
    return intersection;
}

void search_functions::clear_results(void) {
    this->matches->clear();
}
