#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "maps.h"

#include <QtDebug>
#include <QShortcut>
#include <QComboBox>
#include <QTableWidgetItem>
#include <QInputDialog>
#include <QScrollBar>
#include <QKeyEvent>
#include <QListWidget>
#include <iostream>

#include <assert.h>
#include <unistd.h>
#include <limits.h>

/*
 * PLAN:
 * 1. create a function to automatically update the value in the variables DONE!!
 * 2. create ability to freeze a memory range(value in memory more likely) (mmap)
 * 3. create more types to read
 * 4. create more types to write
 * 5. add pointer dereferencing
 * 6. a better ui for pointers, like cheat engine
 * */
MainWindow::MainWindow(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    MainWindow::ui->next_scan->setEnabled(false);
    //MemoryWidget *memory_addresses = new MemoryWidget(this);
    ui->memory_addresses->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->memory_addresses->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->memory_addresses->setEditTriggers(QAbstractItemView::NoEditTriggers);
    MainWindow::create_menu();
    MainWindow::create_connections();
    ui->search_bar->setValidator(this->pos_only);
    ui->saved_addresses->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    connect(ui->attachButton, &QPushButton::clicked, this,
                              &MainWindow::show_pid_window);
    QtConcurrent::run(this, &MainWindow::saved_address_thread);
}

void MainWindow::show_pid_window() {
    QDir procDir("/proc");
    QStringList entries = procDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    QList<QPair<int, QString>> pidInfoList;
    int maxPidDigits = 0;

    for (const QString &entry : entries) {
        bool ok = false;
        int pid = entry.toInt(&ok);
        if (!ok) continue;

        QString commPath = QString("/proc/%1/comm").arg(pid);
        QFile commFile(commPath);
        QString procName = "<unknown>";

        if (commFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&commFile);
            procName = in.readLine().trimmed();
            commFile.close();
        }

        pidInfoList.append(qMakePair(pid, procName));

        int pidDigits = QString::number(pid).length();
        if (pidDigits > maxPidDigits) {
            maxPidDigits = pidDigits;
        }
    }

    QDialog dialog(this);
    dialog.setWindowTitle(tr("Select a PID"));
    QVBoxLayout *layout = new QVBoxLayout(&dialog);

    QLineEdit *searchEdit = new QLineEdit(&dialog);
    searchEdit->setPlaceholderText(tr("Search..."));
    layout->addWidget(searchEdit);

    QListWidget *listWidget = new QListWidget(&dialog);
    for (const auto &pidInfo : pidInfoList) {
        int pid = pidInfo.first;
        const QString &name = pidInfo.second;
        QString itemText = QString("%1 : %2")
                               .arg(pid, -maxPidDigits)
                               .arg(name);

        QListWidgetItem *item = new QListWidgetItem(itemText, listWidget);
        item->setData(Qt::UserRole, pid);
    }
    layout->addWidget(listWidget);

    connect(searchEdit, &QLineEdit::textChanged, [listWidget](const QString &text) {
        for (int i = 0; i < listWidget->count(); ++i) {
            QListWidgetItem *item = listWidget->item(i);
            bool match = item->text().contains(text, Qt::CaseInsensitive);
            item->setHidden(!match);
        }
    });

    connect(listWidget, &QListWidget::itemDoubleClicked, &dialog,
            [this, &dialog](QListWidgetItem *item){
                int pid = item->data(Qt::UserRole).toInt();
                QString displayText = item->text();
                qDebug() << "User clicked PID:" << pid << "(" << displayText << ")";
                scanner.pid(pid);
                setWindowTitle("MemSC - " + displayText);
                dialog.close();
            });

    QShortcut *enterShortcut = new QShortcut(QKeySequence(Qt::Key_Return), &dialog);
    connect(enterShortcut, &QShortcut::activated, [this, listWidget, &dialog]() {
        for (int i = 0; i < listWidget->count(); ++i) {
            QListWidgetItem *item = listWidget->item(i);
            if (!item->isHidden()) {
                int pid = item->data(Qt::UserRole).toInt();
                QString displayText = item->text();
                qDebug() << "User pressed Enter, selected PID:" << pid << "(" << displayText << ")";
                scanner.pid(pid);
                setWindowTitle("MemSC - " + displayText);
                dialog.close();
                break;
            }
        }
    });

    dialog.exec();
}

MainWindow::~MainWindow() {
    this->quit = true;
}

void MainWindow::create_menu() {
    menubar = new QMenuBar(this);
    filemenu = new QMenu("File", this);
    menubar->addMenu(filemenu);
    this->layout()->setMenuBar(menubar);
}

void MainWindow::create_connections() {
    connect(ui->memory_addresses, &QTableWidget::cellDoubleClicked,
                     this, &MainWindow::save_row);
    connect(ui->value_type,
                     QOverload<int>::of(&QComboBox::currentIndexChanged),
                     this, &MainWindow::change_validator);
    connect(ui->new_scan, &QPushButton::pressed,
                     this, &MainWindow::handle_new_scan);
    connect(ui->next_scan, &QPushButton::pressed,
                     this, &MainWindow::handle_next_scan);
    connect(ui->saved_addresses, &QTableWidget::cellDoubleClicked, this, &MainWindow::handle_double_click_saved);
    connect(this, &MainWindow::value_changed, this, &MainWindow::saved_address_change);
}

/**
 * @brief parse_cstr places the value correctly from the table into dest, cuts off necessary bits
 * @param dest
 * @param src
 * @param size
 * @param type
 */
void parse_cstr(void *dest, const char *src, size_t size, int type) {
    if(dest == nullptr || src == nullptr) {
        return;
    }
    /* it's simply the largest possible; so that everything will fit */
    uint64_t num = 0;
    /* to reduce code repetitions */
    size_t max_values[] = {
        0,
        UINT8_MAX,
        UINT16_MAX,
        UINT32_MAX,
        UINT64_MAX,
    };
    size_t sizes[] = {
        0,
        sizeof(uint8_t),
        sizeof(uint16_t),
        sizeof(uint32_t),
        sizeof(uint64_t)
    };

    switch(type) {
    case 0: /* binary */
        break;
    case 1: /* uint8 */
    case 2: /* uint16 */
    case 3: /* uint32 */
    case 4: /* uint64 */
        num = atoll(src) & max_values[type];
        memcpy(dest, &num, size);
        break;
    default:
        break;
    }
}
/**
 * @brief MainWindow::save_row saves the row to saved_addresses, memory monitoring is currently only
 * supported for the "basic" types
 * @param row
 * @param column
 */
void MainWindow::save_row(int row, int column) {
    /* sizes for the specified type */
    static const ssize_t size_array[] = {
        -1,
        1,
        2,
        4,
        8,
        sizeof(float),
        sizeof(double),
        -1,
        -1,
        -1
    };
    QString address = ui->memory_addresses->item(row, 0)->text();
    QString value = ui->memory_addresses->item(row, 2)->text();
    int type = ui->value_type->currentIndex();

    int current_rows = ui->saved_addresses->rowCount();
    ui->saved_addresses->setRowCount(current_rows + 1);
    QTableWidgetItem *checkbox = new QTableWidgetItem();
    checkbox->setCheckState(Qt::Unchecked);
    ui->saved_addresses->setItem(current_rows, 0, checkbox); /* checkbox */
    /*description inserted automatically; when saving is added will probably be included here as well or something */
    ui->saved_addresses->setItem(current_rows, 2, new QTableWidgetItem(address)); /* address */
    QComboBox *type_combobox = new QComboBox(this);
    type_combobox->addItems({"Binary", "Byte", "2 Bytes", "4 Bytes", "8 Bytes", "Float", "Double", "String", "Array of Bytes"});
    type_combobox->setCurrentIndex(type);
    ui->saved_addresses->setCellWidget(current_rows, 3, type_combobox);
    QTableWidgetItem *value_cell = new QTableWidgetItem(value);
    std::string str = value_cell->text().toStdString();
    value_cell->setFlags(value_cell->flags() & (~Qt::ItemFlag::ItemIsEditable));
    /* TODO change this to get from the column that contains the actual value when done */
    ssize_t size = size_array[type];
    if (size > 0) {
        void *_address = nullptr;
        sscanf(address.toStdString().c_str(), "%p", &_address);
        if(saved_address_values.find(_address) == saved_address_values.end()) {
            address_t *entry = new address_t;
            entry->size=size;
            if(size != -1) {
                entry->value = new char[size];
                parse_cstr((void *)entry->value, value.toStdString().c_str(), entry->size, type);
                ui->saved_addresses->setItem(current_rows, 4, value_cell); /* value */

            } else {
                /* TODO add an allocation system for byte arrays etc,
                 * will probably include asking user how large it is
                 */
            }
            saved_address_values[_address] = entry;
            printf("saving: %p\n", _address);

        }
    }
}

void MainWindow::change_validator(int index) {

    switch (index) {
        case 0: /* Int */
            ui->search_bar->setValidator(pos_only);
            break;
        default:
            ui->search_bar->setValidator(nullptr);
    }
}

void MainWindow::handle_new_scan() {
    if(ui->next_scan->isEnabled()) {
        scanner.get_matches().clear();
        ui->memory_addresses->clearContents();
        ui->amount_found->setText("Found: 0");
        ui->next_scan->setEnabled(false);
    } else {
        if(ui->search_bar->text().isEmpty()) {
            return;
        }
        ui->next_scan->setEnabled(true);
        handle_next_scan();
    }
}

void MainWindow::handle_next_scan() {
    if(ui->search_bar->text().isEmpty()) {
        return;
    }

    switch(ui->value_type->currentIndex()) {
    case 0: /* Binary */
        break;
    case 1: /* Byte */
        break;
    case 2: /* 2 Bytes */
        break;
    case 3: /* 4 Bytes */
        scanner.scan<std::uint32_t>(ui->search_bar->text().toInt());
        break;
    case 4: /* 8 Bytes */
        break;
    case 5: /* Float */
        break;
    case 6: /* Double */
        break;
    case 7: /* String */
        break;
    case 9: /* Array of Byte */
        break;
    default:
        /* should never get here... */
        return;
    }

    constexpr int max_rows = 10000;
    MatchSet &matches = scanner.get_matches();
    /* start thread here somewhere to monitor the values later if they change?
     * it's probably very cpu expensive though */
    ui->memory_addresses->setRowCount(std::min(static_cast<int>(matches.size()), max_rows));
    char found[32] = {0};
    snprintf(found, 32, "Found: %ld", matches.size());
    ui->amount_found->setText(found);
    int row = 0;

    /* loop to add the found addresses to the non saved memory address table */
    for(const auto &match : matches) {
        char str_address[64] = {};

        std::visit([&](auto *ptr) {
            snprintf(str_address, 64, "%p", ptr);
            ui->memory_addresses->setItem(row, 0, new QTableWidgetItem(str_address));
            if (ptr != nullptr) {
                std::remove_pointer_t<decltype(ptr)> val{};
                ssize_t n = scanner.read_process_memory(ptr, &val, sizeof(val));
                assert(n == sizeof(val));
                ui->memory_addresses->setItem(row, 2, new QTableWidgetItem(QString::number(val)));
            }
        }, match);

        row++;
        if (row >= max_rows) {
            break;
        }
    }
}

void MainWindow::handle_double_click_saved(int row,int column) {
    switch (column) {
    case 0: /* freeze value, probably somehow with mmap to change region to read only*/
        break;
    case 1: /* change description */
        break;
    case 2: /* change address */
        break;
    case 3: /* change type */
        break;
    case 4: /* change value */
        uint32_t value =(uint32_t) QInputDialog::getInt(this, tr("QInputDialog::getInt()"), tr("Set Value"));
        QString str_addr = ui->saved_addresses->item(row, 2)->text();
        void *address = nullptr;
        sscanf(str_addr.toStdString().c_str(), "%p", &address);
        // TODO: scanner.write(address, &value, sizeof(value));
        break;
    }
}

void MainWindow::delete_window() {

}

/* https://forum.qt.io/topic/88895/refreshing-content-of-qtablewidget/5 */
void MainWindow::saved_address_thread() {
    while (!this->quit) {
        usleep(100);
        /* this can probably be optimized somehow so it doesn't allocate so much constantly
           could probably be assumed to be 4 bytes and then check if
           it's not that and only then allocate more*/
        for(int row = ui->saved_addresses->verticalScrollBar()->value();
            (row < ui->saved_addresses->verticalScrollBar()->value() + 20) &&
            (row < ui->saved_addresses->rowCount());
            row++) {
            QTableWidgetItem *address_string = ui->saved_addresses->item(row, 2);
            if(address_string == nullptr) {
                continue;
            }

            QTableWidgetItem *current_value = ui->saved_addresses->item(row, 4);
            if(current_value == nullptr) {
                continue;
            }
            void *address = nullptr;
            sscanf(address_string->text().toStdString().c_str(), "%p", &address);
            if(this->saved_address_values.find(address) == this->saved_address_values.end()) {
                fprintf(stderr, "Error could not find address: %p\n", address);
                continue;
            }
            struct address_t *segment = this->saved_address_values[address];
            uint8_t *buffer = new uint8_t[segment->size];

            ssize_t amount_read = scanner.read_process_memory(address, buffer,
                                                              segment->size);

            bool changed = false; /* makes the text red in the value box if the value has changed */
            printf("reading: %p\n", address);
            if((size_t) amount_read != segment->size) {
                perror("saved_address_thread");
            } else {
                if(memcmp(segment->value, buffer, segment->size) != 0) {
                    memcpy(segment->value, buffer, segment->size);
                    changed = true;
                }

            }
            delete[] buffer;
            if(changed) {
                /* should change the text to red if it has changed */
                emit value_changed(segment, row);
            }
        }
    }
}

/**
 * @brief MainWindow::handle_value_changed
 * @param segment the piece in memory that has changed value, it should also have been updated so you don't need to re-read the memory
 * @param row which row it's on
 */
void MainWindow::saved_address_change(address_t *segment, int row) {
    QTableWidgetItem *value_cell = this->ui->saved_addresses->item(row, SAVED_ADDRESS_VALUE);

    value_cell->setForeground(QColor::fromRgb(200, 0, 0));
    QComboBox *combo = qobject_cast<QComboBox *>(this->ui->saved_addresses->cellWidget(row, SAVED_ADDRESS_TYPE));

    uint64_t num = 0;
    switch(combo->currentIndex()) {
    case 0:
        break;
    case 1:
    case 2:
    case 3:
    case 4:
        for (size_t i = 0; i < segment->size; i++) {
            num |= ((char *)segment->value)[i] << i;
        }
        value_cell->setText(QString::number(num));
        break;
    }
}
