#pragma once

#include <QMainWindow>
#include <QWidget>
#include <cstddef>
#include <sys/types.h>
#include <capstone/capstone.h>

class QTableWidget;
class Disassembly : public QMainWindow {
    QTableWidget *disassembly{};
    pid_t pid{};
    csh handle;

public:
    Disassembly(pid_t pid, QWidget *parent = nullptr);
    ~Disassembly();

protected:
    void disassemble(std::size_t offset);
};
