#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTableWidget>

#include <unordered_map>
#include <thread>

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

class ProcessMemory;
class QRegularExpressionValidator;
class MapsDialog;
class QMenuBar;
class QMenu;

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
    void load_settings();
    void attach_to_process(pid_t pid, const QString name);
private:
    enum saved_address_cells {
        SAVED_ADDRESS_CHECKBOX = 0,
        SAVED_ADDRESS_DESCRIPTION,
        SAVED_ADDRESS_ADDRESS,
        SAVED_ADDRESS_TYPE,
        SAVED_ADDRESS_VALUE
    };

    Ui::MainWindow *ui;
    QMenuBar    *menubar{};
    QMenu       *filemenu{};
	QTimer      *value_update_timer{};
	MapsDialog  *mapsDialog{};
    QRegularExpressionValidator *pos_only;
    QRegularExpressionValidator *pos_neg;
    QRegularExpressionValidator *floating_point;

    bool quit = false;
    std::unordered_map<void *, address_t*> saved_address_values;
    std::thread saved_address_scanner;
    ProcessMemory *scanner;

    void create_menu();
    void create_connections();
    void show_pid_window();
    void update_table(QTableWidget *widget, int addr_row, int value_row);
    void createMapsDialog(pid_t pid);
    void locate_in_maps(uintptr_t ptr);
};
#endif // MAINWINDOW_H
