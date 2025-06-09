#include "ui_mainwindow.h"
#include "mainwindow.h"

#include <QtDebug>
#include <QShortcut>
#include <QComboBox>
#include <QTableWidgetItem>
#include <QInputDialog>
#include <QScrollBar>
#include <QKeyEvent>
#include <QMenuBar>
#include <QAction>
#include <QListWidget>
#include <iostream>

#include <assert.h>
#include <unistd.h>
#include <QMessageBox>

#include "ui/MapsDialog.h"
#include "ui/MatchTableItem.h"

static void toggleLayoutItems(QLayout *layout, bool enable) {
    for (int i = 0; i < layout->count(); ++i) {
        QLayoutItem *item = layout->itemAt(i);
        if (QWidget *widget = item->widget()) {
            widget->setEnabled(enable);
        } else if (QLayout *childLayout = item->layout()) {
            toggleLayoutItems(childLayout, enable);
        }
    }
}

/**
 * @brief parse_cstr places the value correctly from the table into dest, cuts off necessary bits
 * @param dest
 * @param src
 * @param size
 * @param type
 */
static void parse_cstr(void *dest, const char *src, size_t size, int type) {
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

    ui->progressBar->setRange(0, 100);
    scanner.setProgressCallback(
    [this] (size_t current, size_t total) {
        QMetaObject::invokeMethod(ui->progressBar, [this, current, total]() {
            ui->progressBar->setValue((int) ((double) current / total * 100));
        }, Qt::QueuedConnection);
    });
    ui->next_scan->setEnabled(false);
    //MemoryWidget *memory_addresses = new MemoryWidget(this);
    ui->memory_addresses->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->memory_addresses->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->memory_addresses->setEditTriggers(QAbstractItemView::NoEditTriggers);
    create_menu();
    create_connections();
    ui->search_bar->setValidator(this->pos_only);
    ui->saved_addresses->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    QtConcurrent::run(this, &MainWindow::saved_address_thread);
    toggleLayoutItems(ui->memorySearchLayout, false);
    ui->memory_addresses->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    ui->memory_addresses->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    ui->saved_addresses->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
}


void MainWindow::show_pid_window() {
	PidDialog *dialog = new PidDialog(this);
	connect(dialog, &PidDialog::pidSelected, this, [this] (pid_t pid, const QString &name) {
		scanner.pid(pid);
		setWindowTitle(QString("MemSC - ") + QString::number(pid) + name);
		toggleLayoutItems(ui->memorySearchLayout, true);
		ui->next_scan->setEnabled(false);
		ui->search_bar->setFocus();
		ui->memory_addresses->clearContents();
		ui->saved_addresses->clearContents();
	});
	dialog->exec();
}

MainWindow::~MainWindow() {
    this->quit = true;
    delete ui;
}

void MainWindow::create_menu() {
    menubar = new QMenuBar(this);
    filemenu = new QMenu("File", menubar);
    QMenu *tools = new QMenu("Tools", menubar);
    QAction *maps = new QAction("Maps", tools);
    maps->setShortcut(QKeySequence(Qt::Key_F12));
    connect(maps, &QAction::triggered, [this]() {
        pid_t pid = scanner.pid();
        if (pid == 0) {
            QMessageBox::warning(
                this,
                tr("Invalid PID"),
                tr("Please attach to a process")
            );
            return;
        }
        MapsDialog *dialog = new MapsDialog(pid, this);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->setWindowModality(Qt::NonModal);
        dialog->show();
    });
    tools->addAction(maps);
    menubar->addMenu(filemenu);
    menubar->addMenu(tools);
    this->layout()->setMenuBar(menubar);
}

void MainWindow::create_connections() {
    ui->attachButton->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_A));
    connect(ui->attachButton, &QPushButton::clicked, this,
                          &MainWindow::show_pid_window);
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
    connect(ui->search_bar, &QLineEdit::returnPressed, this, [this]() {
    if (ui->next_scan->isEnabled())
        handle_next_scan();
    else
        handle_new_scan();
});
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
    QTableWidgetItem *address = ui->memory_addresses->item(row, 0)->clone();
    QString value = ui->memory_addresses->item(row, 2)->text();
    int type = ui->value_type->currentIndex();

    int current_rows = ui->saved_addresses->rowCount();
    ui->saved_addresses->setRowCount(current_rows + 1);
    QTableWidgetItem *checkbox = new QTableWidgetItem();
    checkbox->setCheckState(Qt::Unchecked);
    ui->saved_addresses->setItem(current_rows, 0, checkbox); /* checkbox */
    /*description inserted automatically; when saving is added will probably be included here as well or something */
    ui->saved_addresses->setItem(current_rows, 2, address); /* address */
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
        sscanf(address->text().toStdString().c_str(), "%p", &_address);
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
        ui->value_type->setEnabled(true);
    } else {
        if(ui->search_bar->text().isEmpty() || scanner.scanning()) {
            return;
        }
        ui->value_type->setEnabled(false);
        QtConcurrent::run([this](){
            handle_next_scan();
        });
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
        assert(false);
        return;
    }

    QMetaObject::invokeMethod(this, [this]() {
        constexpr int max_rows = 10000;
        auto &matches = scanner.get_matches();
        /* start thread here somewhere to monitor the values later if they change?
         * it's probably very cpu expensive though */
        ui->memory_addresses->clearContents();
        ui->memory_addresses->setRowCount(0);
        int row = 0;

        for(size_t i = 0; i < matches.size(); i++) {
            char str_address[64] = {};
            Match &match = matches[i];

            std::visit([&](auto *ptr) {
                if (ptr == nullptr) {
                    return;
                }
                matches[row] = ptr;

                if (row >= max_rows) {
                    row++;
                    return;
                }

                snprintf(str_address, 64, "%p", ptr);
                std::remove_pointer_t<decltype(ptr)> val{};
                [[maybe_unused]] ssize_t n = scanner.read_process_memory(ptr, &val, sizeof(val));
                assert(n == sizeof(val));
                ui->memory_addresses->insertRow(row);
                ui->memory_addresses->setItem(row, 0, new MatchTableItem(str_address, match));
                ui->memory_addresses->setItem(row, 1, new QTableWidgetItem(QString::number(val)));
                ui->memory_addresses->setItem(row, 2, new QTableWidgetItem(ui->search_bar->text()));
                // Restructure the array to not have a bunch of "dead" slots
                row++;
            }, match);
        }
        matches.resize(row);

        char found[32] = {0};
        snprintf(found, 32, "Found: %d", row);
        ui->amount_found->setText(found);
        ui->next_scan->setEnabled(true);
    }, Qt::QueuedConnection);

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
        //uint32_t value =(uint32_t) QInputDialog::getInt(this, tr("QInputDialog::getInt()"), tr("Set Value"));
        QString str_addr = ui->saved_addresses->item(row, 2)->text();
        void *address = nullptr;
        sscanf(str_addr.toStdString().c_str(), "%p", &address);
        // TODO: scanner.write(address, &value, sizeof(value));
        break;
    }
}

void MainWindow::delete_window() {

}

void MainWindow::update_table(QTableWidget *widget, int addr_col, int value_col) {
    // Scans forward this many rows, unnecessary to go further down
    constexpr int scan_forward = 50;
    // Look around "current row", i.e. highest one
    for (int row = std::max(widget->verticalScrollBar()->value() - scan_forward, 0); row < widget->verticalScrollBar()->value() + scan_forward; row++) {
        QTableWidgetItem *addr_item = widget->item(row, addr_col);
        if (addr_item == nullptr) {
            continue;
        }
        QTableWidgetItem *current_value = widget->item(row, value_col);
        if (current_value == nullptr) {
            continue;
        }
        if (auto *mti = dynamic_cast<MatchTableItem*>(addr_item)) {
            const Match &storedMatch = mti->match();
            std::visit([&](auto *ptr) {
                using U = std::remove_pointer_t<decltype(ptr)>;
                if (!ptr) return;

                U newVal{};
                [[maybe_unused]] ssize_t n = scanner.read_process_memory(ptr, &newVal, sizeof(newVal));
                assert(n == sizeof(newVal));

                current_value->setText(QString::number(newVal));
            }, storedMatch);
        }
        else {
            // Not a MatchTableItem
        }
    }
}

/* https://forum.qt.io/topic/88895/refreshing-content-of-qtablewidget/5 */
void MainWindow::saved_address_thread() {
    std::atomic<bool> has_run = true;
    while (!this->quit) {
        while (!has_run.load() && !this->quit) {
            usleep(100);
        }
        has_run = false;
        QMetaObject::invokeMethod(this, [this, &has_run]() {
            update_table(ui->saved_addresses, 0, 1);
            update_table(ui->memory_addresses, 0, 1);
            has_run = true;
        }, Qt::QueuedConnection);
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
