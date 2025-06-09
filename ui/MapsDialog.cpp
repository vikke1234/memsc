#include "MapsDialog.h"
#include <QHeaderView>
#include <QMessageBox>
#include <QLabel>
#include <cstdio>
#include <QFontDatabase>
#include <QStyledItemDelegate>
#include <inttypes.h>    // for PRIuPTR
#include <QApplication>
#include <qmetaobject.h>

/**
 * Delegate to elide the start of a filename instead of the end.
 */
class LeftElideDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    virtual void paint(QPainter *painter, const QStyleOptionViewItem &option,
      const QModelIndex &index) const
    {
        QStyleOptionViewItem opt = option;
        opt.textElideMode=Qt::ElideLeft;
        QStyledItemDelegate::paint(painter, opt, index);
    }
};

MapsDialog::MapsDialog(pid_t pid, QWidget *parent)
    : QDialog(parent),
      searchEdit_(new QLineEdit(this)),
      searchButton_(new QPushButton("Go", this)),
      table_(new QTableWidget(this)),
      mainLayout_(new QVBoxLayout(this))
{
    setWindowTitle(QString("Memory Map for PID %1").arg(pid));
    resize(900, 500);

    ranges = get_memory_ranges(pid);

    auto *searchLayout = new QHBoxLayout();
    searchLayout->addWidget(new QLabel("Address:", this));
    searchEdit_->setPlaceholderText("e.g. 0x7f3e2a500000");
    searchLayout->addWidget(searchEdit_);
    searchLayout->addWidget(searchButton_);

    // Make pressing Enter in the QLineEdit trigger the same slot as clicking "Go"
    connect(searchEdit_, &QLineEdit::returnPressed, this, &MapsDialog::searchAddress);
    connect(searchButton_, &QPushButton::clicked, this, &MapsDialog::searchAddress);

    table_->setColumnCount(8);
    QStringList headers;
    headers << "Start"
            << "End"
            << "Length"
            << "Perms"
            << "Offset"
            << "Device"
            << "Inode"
            << "Name";
    table_->setHorizontalHeaderLabels(headers);
    const QFont mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    table_->setFont(mono);
    searchEdit_->setFont(mono);

    table_->setWordWrap(false);
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setStretchLastSection(true);
    table_->horizontalHeader()->resizeSection(7, 300);
    table_->setItemDelegateForColumn(7, new LeftElideDelegate());


    table_->setEditTriggers(QTableWidget::NoEditTriggers);
    table_->setSelectionBehavior(QTableWidget::SelectRows);
    table_->setSelectionMode(QTableWidget::SingleSelection);

    mainLayout_->addLayout(searchLayout);
    mainLayout_->addWidget(table_);
    setLayout(mainLayout_);

    populateTable();
}

QString MapsDialog::permsToString(unsigned char perms)
{
    QString s;
    s.reserve(4);
    s += (perms & PERM_READ    ? 'r' : '-');
    s += (perms & PERM_WRITE   ? 'w' : '-');
    s += (perms & PERM_EXECUTE ? 'x' : '-');
    if (perms & PERM_SHARED)
        s += 's';
    else if (perms & PERM_PRIVATE)
        s += 'p';
    else
        s += '-';
    return s;
}

void MapsDialog::populateTable()
{
    size_t count = get_address_range_list_size(ranges, false);
    table_->setRowCount(int(ranges.size()));

    int row = 0;
    for (address_range &current : ranges) {
        // Column 0: Start address (hex)
        char bufStart[32], bufEnd[32];
        uintptr_t startAddr = reinterpret_cast<uintptr_t>(current.start);
        sprintf(bufStart, "0x%0*" PRIxPTR,
                (int)(sizeof(void*)*2),
                startAddr);

        // Column 1: End address = start + length
        uintptr_t endAddr = startAddr + current.length;
        sprintf(bufEnd, "0x%0*" PRIxPTR,
                (int)(sizeof(void*)*2),
                endAddr);

        // Column 2: Length (decimal)
        QString lengthStr = QString::number(current.length);

        // Column 3: Permissions string
        QString permsStr = permsToString(current.perms);

        // Column 4: Offset (hex)
        char bufOffset[32];
        sprintf(bufOffset, "0x%zx", current.offset);

        // Column 5: Device = major:minor
        unsigned int major = (current.device >> 8) & 0xff;
        unsigned int minor = (current.device & 0xff) |
                             ((current.device >> 12) & 0xfff00);
        QString deviceStr = QString("%1:%2").arg(major).arg(minor);

        // Column 6: inode (if any)
        QString inode;
        if (current.inode != 0) {
            inode = QString::number(current.inode);
        }

        // Column 7: name (if any)
        QString name;
        if (current.name[0] != '\0') {
            name = QString::fromUtf8(current.name);
        }

        QTableWidgetItem *startItem  = new QTableWidgetItem(QString::fromUtf8(bufStart));
        QTableWidgetItem *endItem    = new QTableWidgetItem(QString::fromUtf8(bufEnd));
        QTableWidgetItem *lengthItem = new QTableWidgetItem(lengthStr);
        QTableWidgetItem *permsItem  = new QTableWidgetItem(permsStr);
        QTableWidgetItem *offsetItem = new QTableWidgetItem(QString::fromUtf8(bufOffset));
        QTableWidgetItem *deviceItem = new QTableWidgetItem(deviceStr);
        QTableWidgetItem *inodeItem  = new QTableWidgetItem(inode);
        QTableWidgetItem *nameItem   = new QTableWidgetItem(name);
        nameItem->setToolTip(name);

        // --- if this range is executable, give a very light background to all columns ---
        if (current.perms & PERM_EXECUTE) {
            // e.g. a pale red tint; tweak RGB if you prefer something lighter/darker
            QColor highlightColor(255, 230, 230);
            QBrush  brush(highlightColor);

            startItem->setBackground(brush);
            endItem->setBackground(brush);
            lengthItem->setBackground(brush);
            permsItem->setBackground(brush);
            offsetItem->setBackground(brush);
            deviceItem->setBackground(brush);
            inodeItem->setBackground(brush);
            nameItem->setBackground(brush);
        }

        // --- finally insert each item into the table, column by column ---
        table_->setItem(row, 0, startItem);
        table_->setItem(row, 1, endItem);
        table_->setItem(row, 2, lengthItem);
        table_->setItem(row, 3, permsItem);
        table_->setItem(row, 4, offsetItem);
        table_->setItem(row, 5, deviceItem);
        table_->setItem(row, 6, inodeItem);
        table_->setItem(row, 7, nameItem);
        ++row;
    }
}

void MapsDialog::gotoAddress(uintptr_t addr) const {
	QMetaObject::invokeMethod(table_, [&] {
		int targetRow = findRowForAddress(addr);
		if (targetRow < 0) {
			QMessageBox::information(table_,
									 "Not Found",
									 QString("Address %1 is not contained in any mapped region.")
										 .arg(addr));
			return;
		}

		table_->selectRow(targetRow);
		table_->scrollToItem(table_->item(targetRow, 0), QAbstractItemView::PositionAtCenter);
	}, Qt::QueuedConnection);
}

int MapsDialog::findRowForAddress(uintptr_t addr) const
{
    int row = 0;
    for (const address_range &cur : ranges) {
        uintptr_t startAddr = reinterpret_cast<uintptr_t>(cur.start);
        uintptr_t endAddr = startAddr + cur.length;
        if (addr >= startAddr && addr < endAddr) {
            return row;
        }
        ++row;
    }
    return -1;
}

void MapsDialog::searchAddress()
{
    QString text = searchEdit_->text().trimmed();
    if (text.isEmpty()) {
        QMessageBox::warning(this,
                             "Empty Address",
                             "Please enter a valid address (e.g. 0x7f3e2a500000).");
        return;
    }

    bool ok = false;
    uintptr_t value = text.toULongLong(&ok, 16);
    if (!ok) {
        QMessageBox::warning(this,
                             "Invalid Address",
                             "Could not parse the given address. "
                             "Make sure it’s a hex or decimal number (e.g. 0x7f3e2a500000).");
        return;
    }
	gotoAddress(value);
}
