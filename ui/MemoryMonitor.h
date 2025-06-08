//
// Created by viktorh on 5.6.2025.
//

#pragma once
#include <QObject>
#include <variant>

#include "ProcessMemory.h"

class MemoryMonitor : public QObject {
    Q_OBJECT
    const ProcessMemory &scanner;
public:
    explicit MemoryMonitor(const ProcessMemory &scanner, QObject *parent = nullptr) : QObject(parent), scanner(scanner) {}
    ~MemoryMonitor() override {}

public slots:
    // Call this whenever you want the worker to do “one pass” of updating.
    // You pass in the current scroll position, and the list of Matches (so
    // that the worker can loop over them and read memory).
    void doOneUpdate(int firstRow, int lastRow, const std::vector<Match> &matches) {
        // For each row in [firstRow..lastRow), read the pointer and produce (row, newValue) pairs.
        // We'll accumulate them in a temporary list and then emit them all at once.

        std::vector<std::pair<int, QString>> updates;
        updates.reserve(lastRow - firstRow);

        for (const auto &m : matches) {
            // We want only those whose “row index” is between firstRow and lastRow.
            // Let’s say you also store somewhere a vector<int> that maps each match → its row index.
            // For simplicity, I’ll assume “matches” is in exactly the same order as your table rows,
            // so row = index. If you had a more complicated mapping, adjust accordingly.

            // In practice, the worker doesn’t know “widget->verticalScrollBar()->value()”. The
            // main thread will pass in firstRow/lastRow from the current scrollbar. So here:
            int row = 0;
            if (row < firstRow || row >= lastRow)
                continue;

            // Now actually do the read‐memory:
            std::visit([&](auto *ptr) {
                using U = std::remove_pointer_t<decltype(ptr)>;
                if (!ptr) return;

                U val{};
                [[maybe_unused]] ssize_t n = scanner.read_process_memory(ptr, &val, sizeof(val));
                assert(n == sizeof(val));

                updates.push_back({ row, QString::number(val) });
            }, m);
        }

        // Once we’ve collected all the (row,newText) that need repainting, emit one signal.
        emit valuesReady(updates);
    }

signals:
    // This signal carries a list of (row, new‐text). It will be queued to the main thread.
    void valuesReady(std::vector<std::pair<int, QString>> updates);
};
