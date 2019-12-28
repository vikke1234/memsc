#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "maps.h"

#include <QtDebug>
#include <QMainWindow>
#include <QComboBox>
#include <QTableWidgetItem>

#include <thread>

MainWindow::MainWindow(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    QObject::connect(MainWindow::ui->new_scan, &QPushButton::pressed, this, &MainWindow::handle_new_scan);
    QObject::connect(MainWindow::ui->next_scan, &QPushButton::pressed, this, &MainWindow::handle_next_scan);
    MainWindow::ui->next_scan->setEnabled(false);
    QObject::connect(MainWindow::ui->value_type, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::change_validator);

    MainWindow::create_menu();
    ui->search_bar->setValidator(this->pos_only);
    ui->memory_addresses->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    this->search = new search_functions;
}

MainWindow::~MainWindow()
{
    delete ui;
    delete pos_only;
    delete pos_neg;
    delete search;
    delete filemenu;
    delete menubar;
}

void MainWindow::create_menu(void) {
    menubar = new QMenuBar(this);
    filemenu = new QMenu("File");
    QAction *open = new QAction("Open", filemenu);
    filemenu->addAction(open);
    menubar->addMenu(filemenu);
    this->layout()->setMenuBar(menubar);
}

void MainWindow::change_validator(int index) {

    switch (index) {
        case 0: /* Int */
            MainWindow::ui->search_bar->setValidator(pos_only);
            break;
        default:
            MainWindow::ui->search_bar->setValidator(nullptr);
    }
}

void MainWindow::handle_new_scan() {
    if(ui->next_scan->isEnabled()) {
        ui->memory_addresses->clearContents();
        ui->amount_found->setText("Found: 0");
        search->get_matches()->clear();
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
        search->scan_for((uint32_t) MainWindow::ui->search_bar->text().toInt());
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
    /* start thread here somewhere to monitor the values later if they change?
     * it's probably very cpu expensive though */
    ui->memory_addresses->setRowCount((int) search->get_matches()->size());
    char found[32] = {0};
    sprintf(found, "Found: %d", search->get_matches()->size());
    ui->amount_found->setText(found);
    int row = 0;
    char str_address[64] = {0};
    for(auto it = search->get_matches()->begin(); it != search->get_matches()->end();it++) {
        sprintf(str_address, "%p", *it);

        ui->memory_addresses->setItem(row, 0, new QTableWidgetItem(str_address));
        ui->memory_addresses->setItem(row, 2, new QTableWidgetItem(ui->search_bar->text()));
        row++;
    }
}

