#include "mainwindow.h"

#include <QApplication>

Q_DECLARE_METATYPE(Match);
int main(int argc, char *argv[])
{
    qRegisterMetaType<Match>("Match");
    QApplication a(argc, argv);
    MainWindow w;
    w.show();
    return a.exec();
}
