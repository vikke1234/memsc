#ifndef MEMORYWIDGET_H
#define MEMORYWIDGET_H

#include <QWidget>
#include <QTableWidget>
#include <QKeyEvent>

class MemoryWidget : public QTableWidget
{
    Q_OBJECT
public:
    MemoryWidget(QWidget *parent=nullptr);

protected:
    void keyPressEvent(QKeyEvent *event);

signals:
    void delete_pressed(int row, int column);
};

#endif // MEMORYWIDGET_H
