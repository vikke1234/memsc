#pragma once

#include <QDialog>
#include <QTableWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QString>
#include <QHBoxLayout>
#include <sys/types.h>
#include "maps.h"  // your existing header

/**
 * A QDialog that shows the memory mappings of a process (PID).
 * It calls get_memory_ranges(pid) internally and populates a table
 * with each address_range. It also provides a text input for looking
 * up a specific address.
 */
class MapsDialog : public QDialog
{
    Q_OBJECT

public:
    /**
     * @param pid
     *   The target process ID whose /proc/<pid>/maps we will display.
     *   Should be non‐zero and refer to a live process.
     */
    explicit MapsDialog(pid_t pid, QWidget *parent = nullptr);

    ~MapsDialog() override = default;

private slots:
    /// Called when the user presses "Go" or hits Enter in the QLineEdit.
    void searchAddress();

private:
    QLineEdit                      *searchEdit_;
    QPushButton                    *searchButton_;
    QTableWidget                   *table_;
    QVBoxLayout                    *mainLayout_;
    std::vector<address_range>  ranges;  ///< keep the head pointer around

    /// Helper: Convert the perms bitmask into a human‐readable "rwxp" string.
    static QString permsToString(unsigned char perms);

    /// Populate the table from rangesHead_.
    void populateTable();

    /**
     * Given a raw hex/decimal address, scan through rangesHead_ to find which
     * address_range (if any) contains it. Returns the zero‐based row index or -1 if not found.
     */
    int findRowForAddress(uintptr_t addr) const;
};