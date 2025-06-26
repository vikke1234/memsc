#include "ui/Disassembly.h"
#include "ProcessMemory.h"
#include "capstone/capstone.h"
#include "maps.h"

#include <algorithm>

#include <QTableWidget>
#include <QVBoxLayout>
#include <QWidget>
#include <QHeaderView>
#include <memory>

Disassembly::Disassembly(pid_t pid, QWidget *parent) : QMainWindow(parent),
    disassembly(new QTableWidget(this)),
    pid(pid)
{
    if (cs_open(CS_ARCH_X86, CS_MODE_64, &handle) != CS_ERR_OK) {
        close();
    }
    QFont monoFont("Courier New");  // Or use "Monospace", "Courier", etc.
    monoFont.setStyleHint(QFont::Monospace);
    disassembly->setFont(monoFont);

    setWindowTitle("Disassembler");
    disassembly->setColumnCount(3);
    disassembly->setHorizontalHeaderLabels(
        {"Address", "Disassembly", "Comments"});
    disassembly->horizontalHeader()->
        setSectionResizeMode(QHeaderView::Stretch);
    disassembly->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    disassembly->verticalHeader()->setVisible(false);
    disassembly->setShowGrid(false);

    setCentralWidget(disassembly);
    resize(1024, 800);
    disassemble(0);
}

Disassembly::~Disassembly() {
    cs_close(&handle);
}

void Disassembly::disassemble(std::size_t offset) {
    auto maps = get_memory_ranges(pid, true);
    // Remove non-executable sections
    maps.erase(std::remove_if(maps.begin(), maps.end(), [](auto &entry)
                   { return !(entry.perms & PERM_EXECUTE); }));

    std::uint64_t row = 0;
    for (auto &map : maps) {
        std::unique_ptr<std::uint8_t[]> buf =
            std::make_unique<std::uint8_t[]>(map.length);
        ProcessMemory::read_process_memory_nosplit(pid, map.start,
                                                   buf.get(), map.length);
        cs_insn *ins;
        std::size_t count =
            cs_disasm(handle, buf.get(), map.length, (uint64_t)map.start, 0, &ins);

        disassembly->setRowCount(count + row);
        if (count > 0) {
            for (std::size_t i = 0; i < count; i++) {
                QTableWidgetItem *addr = new QTableWidgetItem(
                    "0x" + QString::number(ins[i].address, 16));
                QString str = QString(ins[i].mnemonic) % QString(" ") %
                    QString(ins[i].op_str);
                QTableWidgetItem *op = new QTableWidgetItem(str);
                disassembly->setItem(row, 0, addr);
                disassembly->setItem(row, 1, op);
                row++;
            }
            free(ins);
        };
    }
}
