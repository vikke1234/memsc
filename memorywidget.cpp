#include "memorywidget.h"

MemoryWidget::MemoryWidget(QWidget *parent) : QTableWidget(parent)
{

}

void MemoryWidget::keyPressEvent(QKeyEvent *event) {
    if(event->key() == Qt::Key_Delete) {
        emit delete_pressed(this->currentRow(), this->currentColumn());
    } else {
        QTableWidget::keyPressEvent(event);
    }
}
