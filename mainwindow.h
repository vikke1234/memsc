#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMenuBar>
#include <QMenu>
#include <QRegExpValidator>

#include "search_functions.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QWidget
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
signals:

public slots:
    void handle_next_scan(void);
    void handle_new_scan(void);
    void change_validator(int index);
    void save_row(int row, int column);
    void handle_double_click_saved(int row, int column);
private:
    QMenuBar    *menubar;
    QMenu       *filemenu;
    QRegExpValidator *pos_only = new QRegExpValidator(QRegExp("\\d*"));
    QRegExpValidator *pos_neg = new QRegExpValidator(QRegExp("[+-]?\\d*"));

    search_functions *search = new search_functions;

    Ui::MainWindow *ui;
    void create_menu(void);
    void create_connections(void);

};
#endif // MAINWINDOW_H
