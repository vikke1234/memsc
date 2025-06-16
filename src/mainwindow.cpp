#include "mainwindow.h"
#include "ProcessMemory.h"
#include "ui/MapsDialog.h"
#include "ui/PidDialog.h"
#include "ui/MatchTableItem.h"
#include "ui/Settings.h"
#include "ui_mainwindow.h"

#include <QAction>
#include <QComboBox>
#include <QListWidget>
#include <QMenuBar>
#include <QMessageBox>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QScrollBar>
#include <QShortcut>
#include <QTableWidgetItem>
#include <QTimer>
#include <QtConcurrent/QtConcurrent>
#include <QtDebug>

#include <assert.h>
#include <QDebug>
#include <QSettings>
#include <unistd.h>

template<typename T>
void populate_table_after_scan(ProcessMemory &scanner,
                               QTableWidget *memory_addresses,
                               const QString &searchText,
                               QLabel *amount_found_label,
                               QPushButton *next_scan_button)
{
    constexpr int max_rows = 10000;
    auto &matches = scanner.get_matches();
    memory_addresses->clearContents();
    memory_addresses->setRowCount(0);
    int row = 0;
    for (size_t i = 0; i < matches.size(); ++i) {
        void *match = matches[i];
        if (!match) continue;
        if (row >= max_rows) { row++; continue; }
        char str_address[64];
        snprintf(str_address, sizeof(str_address), "%p", match);

        T val{};
        [[maybe_unused]]
        ssize_t n = scanner.read_process_memory(match, &val, sizeof(val));
        assert(n == sizeof(val));

        memory_addresses->insertRow(row);
        memory_addresses->setItem(row, 0, new MatchTableItem(str_address, reinterpret_cast<T*>(match)));
        memory_addresses->setItem(row, 1, new QTableWidgetItem(QString::number(val)));
        memory_addresses->setItem(row, 2, new QTableWidgetItem(searchText));

        row++;
    }
    matches.resize(row);
    amount_found_label->setText(QString("Found: %1").arg(row));
    next_scan_button->setEnabled(true);
}

template<typename T>
void start_scan_and_populate(MainWindow *self, ProcessMemory *scanner,
                             QTableWidget *memory_addresses,
                             const QString &searchText,
                             QLabel *amount_found_label,
                             QPushButton *next_scan_button,
                             T parsedValue)
{
    auto future = QtConcurrent::run([scanner, parsedValue] {
        scanner->scan(parsedValue);
    });

    auto *watcher = new QFutureWatcher<void>(self);
    QObject::connect(watcher, &QFutureWatcher<void>::finished, self, [self, scanner, memory_addresses, searchText, amount_found_label, next_scan_button, watcher]() {
            populate_table_after_scan<T>(*scanner, memory_addresses, searchText,
                                         amount_found_label, next_scan_button);
            watcher->deleteLater();
        });
    watcher->setFuture(future);
}

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
    : QWidget(parent), ui(new Ui::MainWindow),
      value_update_timer(new QTimer(this)),
      pos_only{new QRegularExpressionValidator(QRegularExpression("\\d*"), this)},
      pos_neg{new QRegularExpressionValidator(QRegularExpression("[+-]?\\d*"),
                                              this)},
    floating_point(new QRegularExpressionValidator(QRegularExpression("\\d+([,.]+\\d*)?"), this)),
    scanner(new ProcessMemory())
{
    ui->setupUi(this);

    ui->progressBar->setRange(0, 100);
    scanner->setProgressCallback(
    [this] (size_t current, size_t total) {
        QMetaObject::invokeMethod(ui->progressBar, [this, current, total]() {
            ui->progressBar->setValue((int) ((double) current / total * 100));
        }, Qt::QueuedConnection);
    });
    ui->next_scan->setEnabled(false);
    ui->value_type->setEnabled(false);
    //MemoryWidget *memory_addresses = new MemoryWidget(this);
    ui->memory_addresses->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->memory_addresses->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->memory_addresses->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->memory_addresses->setEditTriggers(QAbstractItemView::NoEditTriggers);
	ui->memory_addresses->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->memory_addresses, &QTableWidget::customContextMenuRequested, this,
        [this](const QPoint &pos) {
        // figure out which row was clicked
        QModelIndex idx = ui->memory_addresses->indexAt(pos);
        if (!idx.isValid())
            return;

        int row = idx.row();

        // build & exec the menu
        QMenu menu(this);
        QAction *showInMaps = menu.addAction(tr("Show in maps"));
        connect(showInMaps, &QAction::triggered, this, [this, row]() {
            // grab the address string from column 0
            auto *item = ui->memory_addresses->item(row, 0);
            if (!item) return;
            QString addr = item->text();
            // do your mapping logic here, e.g. emit a signal:
            locate_in_maps(addr.toULongLong(nullptr, 16));
        });

        menu.exec(ui->memory_addresses->viewport()->mapToGlobal(pos));
    });
    create_menu();
    create_connections();
    ui->search_bar->setValidator(this->pos_only);
    ui->saved_addresses->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    toggleLayoutItems(ui->memorySearchLayout, false);
    ui->memory_addresses->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    ui->memory_addresses->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    ui->saved_addresses->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);

    load_settings();

    value_update_timer->start();
}

void MainWindow::load_settings() {
    // TODO: integrate failures
    QSettings settings{"Memory Scanner", "Memsc"};
    bool ok = false;
    qDebug() << "Updating settings\n";
    value_update_timer->setInterval(
        settings.value("General/update-interval", 100).toInt());
    std::size_t size =
        settings.value("General/scan-block-size").toULongLong(&ok);
    scanner->max_read_size(size);
    double epsilon = settings.value("General/epsilon").toDouble(&ok);
    scanner->epsilon(epsilon);

    pid_t pid = pid_t{settings.value("General/auto-attach", -1).toInt(&ok)};
    if (scanner->pid() < 0 && ok && pid >= 0) {
        QString commPath = QString("/proc/%1/comm").arg(pid);
        QFile commFile(commPath);
        QString procName = tr("<unknown>");
        if (commFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&commFile);
            procName = in.readLine().trimmed();
        }

        attach_to_process(pid, QString("%1: %2").arg(pid).arg(procName));
    }
}

void MainWindow::attach_to_process(pid_t pid, const QString name) {
    if (!scanner->pid(pid)) {
        return;
    }
    qDebug() << "Attaching to: " << pid << "\n";
    setWindowTitle(QString("MemSC - ") + name);
    toggleLayoutItems(ui->memorySearchLayout, true);
    ui->next_scan->setEnabled(false);
    ui->search_bar->setFocus();
    ui->memory_addresses->clearContents();
    ui->saved_addresses->clearContents();
    scanner->get_matches().clear();
}

void MainWindow::show_pid_window() {
    PidDialog *dialog = new PidDialog(this);
    connect(dialog, &PidDialog::pidSelected, this,
            &MainWindow::attach_to_process);
    dialog->exec();
}

MainWindow::~MainWindow() {
    this->quit = true;
    delete ui;
    delete scanner;
}

void MainWindow::createMapsDialog(pid_t pid) {
    if (mapsDialog == nullptr) {
        mapsDialog = new MapsDialog(pid, this);
        mapsDialog->setAttribute(Qt::WA_DeleteOnClose);
        mapsDialog->setWindowModality(Qt::NonModal);
        connect(mapsDialog, &QObject::destroyed, this, [this]() {
            mapsDialog = nullptr;
        });
        mapsDialog->show();
    }
}

void MainWindow::locate_in_maps(uintptr_t ptr) {
	createMapsDialog(scanner->pid());
    mapsDialog->gotoAddress(reinterpret_cast<uintptr_t>(ptr));
}

void MainWindow::create_menu() {
    menubar = new QMenuBar(this);
    filemenu = new QMenu("File", menubar);
    QMenu *tools = new QMenu("Tools", menubar);
    QAction *maps = new QAction("Maps", tools);
    maps->setShortcut(QKeySequence(Qt::Key_F12));
    connect(maps, &QAction::triggered, [this]() {
        pid_t pid = scanner->pid();
        if (pid == 0) {
            QMessageBox::warning(
                this,
                tr("Invalid PID"),
                tr("Please attach to a process")
            );
            return;
        }

		createMapsDialog(pid);
    });

    QAction *settings = new QAction("Settings", filemenu);
    connect(settings, &QAction::triggered, [this] {
        SettingsDialog *diag = new SettingsDialog(this);
        connect(diag, &SettingsDialog::settings_changed, this, &MainWindow::load_settings);
        diag->exec();
    });
    filemenu->addAction(settings);
    tools->addAction(maps);
    menubar->addMenu(filemenu);
    menubar->addMenu(tools);
    this->layout()->setMenuBar(menubar);
}

void MainWindow::create_connections() {
    QSettings settings{"Memsc"};
    ui->attachButton->setShortcut(QKeySequence(Qt::Key_F2));
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
    // TODO integrate settings
    connect(value_update_timer, &QTimer::timeout, [this] {
        update_table(ui->saved_addresses, 0, 1);
        update_table(ui->memory_addresses, 0, 1);
    });


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
    type_combobox->addItems({"Byte", "2 Bytes", "4 Bytes", "8 Bytes", "Float", "Double", "String", "Array of Bytes"});
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
        /* Int */
        case 0 ... 3:
            ui->search_bar->setValidator(pos_only);
            break;
        case 4:
        case 5:
            ui->search_bar->setValidator(floating_point);
        default:
            ui->search_bar->setValidator(nullptr);
    }
}

void MainWindow::handle_new_scan() {
    if(ui->next_scan->isEnabled()) {
        scanner->get_matches().clear();
        ui->memory_addresses->clearContents();
        ui->amount_found->setText("Found: 0");
        ui->next_scan->setEnabled(false);
        ui->value_type->setEnabled(true);
    } else {
        if(ui->search_bar->text().isEmpty() || scanner->scanning()) {
            return;
        }
        ui->value_type->setEnabled(false);
        handle_next_scan();
    }
}

void MainWindow::handle_next_scan() {
    if(ui->search_bar->text().isEmpty()) {
        return;
    }

    int idx = ui->value_type->currentIndex();
    const QString searchText = ui->search_bar->text();
    switch (idx) {
        case 0: { // Byte -> uint8_t
            bool ok = false;
            uint8_t v = static_cast<uint8_t>(searchText.toUInt(&ok));
            if (!ok) return;
            start_scan_and_populate<uint8_t>(this, scanner,
                                             ui->memory_addresses, searchText,
                                             ui->amount_found, ui->next_scan, v
                                             );
            break;
        }
        case 1: { // 2 Bytes -> uint16_t
            bool ok = false;
            uint16_t v = static_cast<uint16_t>(searchText.toUInt(&ok));
            if (!ok) return;
            start_scan_and_populate<uint16_t>(this, scanner,
                                              ui->memory_addresses, searchText,
                                              ui->amount_found, ui->next_scan, v);
            break;
        }
        case 2: { // 4 Bytes -> uint32_t
            bool ok = false;
            uint32_t v = searchText.toUInt(&ok);
            if (!ok) return;
            start_scan_and_populate<uint32_t>(this, scanner,
                                              ui->memory_addresses, searchText,
                                              ui->amount_found, ui->next_scan, v);
            break;
        }
        case 3: { // 8 Bytes -> uint64_t
            bool ok = false;
            uint64_t v = searchText.toULongLong(&ok);
            if (!ok) return;
            start_scan_and_populate<uint64_t>(this, scanner,
                                              ui->memory_addresses, searchText,
                                              ui->amount_found, ui->next_scan, v);
            break;
        }

        case 4: { // Float
            bool ok = false;
            float v = searchText.toFloat(&ok);
            if (!ok) return;
            start_scan_and_populate<float>(this, scanner,
                                              ui->memory_addresses, searchText,
                                              ui->amount_found, ui->next_scan, v);
            break;
        }

        case 5: { // Double
            bool ok = false;
            double v = searchText.toDouble(&ok);
            if (!ok) return;
            start_scan_and_populate<double>(this, scanner,
                                              ui->memory_addresses, searchText,
                                              ui->amount_found, ui->next_scan, v);
            break;
        }
        default:
            /* should never get here... */
            assert(false);
            return;
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
        //uint32_t value =(uint32_t) QInputDialog::getInt(this, tr("QInputDialog::getInt()"), tr("Set Value"));
        QString str_addr = ui->saved_addresses->item(row, 2)->text();
        void *address = nullptr;
        sscanf(str_addr.toStdString().c_str(), "%p", &address);
        // TODO: scanner->write(address, &value, sizeof(value));
        break;
    }
}

void MainWindow::delete_window() {

}

void MainWindow::update_table(QTableWidget *widget, int addr_col, int value_col) {
    // Scans forward this many rows, unnecessary to go further down
    constexpr int scan_forward = 50;
    int row = std::max(widget->verticalScrollBar()->value() - scan_forward, 0);
    int end = std::min(widget->verticalScrollBar()->value() + scan_forward,
                       widget->rowCount());
    // Look around "current row", i.e. highest one
    for (; row < end; row++) {
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
                [[maybe_unused]] ssize_t n = scanner->read_process_memory(ptr, &newVal, sizeof(newVal));
                assert(n == sizeof(newVal));

                current_value->setText(QString::number(newVal));
            }, storedMatch);
        }
        else {
            // Not a MatchTableItem
        }
    }
}

/**
 * @brief MainWindow::handle_value_changed
 * @param segment the piece in memory that has changed value, it should also
 * have been updated so you don't need to re-read the memory
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
