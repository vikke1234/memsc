#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMenuBar>
#include <QMenu>
#include <QRegExpValidator>
#include <QtConcurrent/QtConcurrent>

#include <unordered_map>
#include <variant>
#include <thread>


#include "ProcessMemory.h"

struct address_t {
    /* fuck this shit, the ui can get to decide what the data is currently..
     * I give up trying to do some bullshit with variants etc */
    /* TODO: change to std::variant */
    void *value;
    /* size to read */
    size_t size;
};


QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QWidget
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;
signals:
    void value_changed(address_t *segment, int row);

public slots:
    void handle_next_scan();
    void handle_new_scan();
    void change_validator(int index);
    void save_row(int row, int column);
    void handle_double_click_saved(int row, int column);
    void delete_window();
    void saved_address_change(address_t *segment, int row);

private:
    QMenuBar    *menubar;
    QMenu       *filemenu;
    QRegExpValidator *pos_only = new QRegExpValidator(QRegExp("\\d*"), this);
    QRegExpValidator *pos_neg = new QRegExpValidator(QRegExp("[+-]?\\d*"), this);

    bool quit = false;
    std::unordered_map<void *, struct address_t*> saved_address_values;
    std::thread saved_address_scanner;
    ProcessMemory scanner;
    Ui::MainWindow *ui;

    void create_menu(void);
    void create_connections(void);
    void saved_address_thread();
    void show_pid_window();

    enum saved_address_cells {
        SAVED_ADDRESS_CHECKBOX = 0,
        SAVED_ADDRESS_DESCRIPTION,
        SAVED_ADDRESS_ADDRESS,
        SAVED_ADDRESS_TYPE,
        SAVED_ADDRESS_VALUE
    };

};
#endif // MAINWINDOW_H
