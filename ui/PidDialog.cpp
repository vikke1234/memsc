#include "ui/PidDialog.h"

#include <qcoreapplication.h>
#include <QVBoxLayout>
#include <QKeyEvent>
#include <QDir>
#include <QVector>
#include <QTextStream>
#include <QDebug>
#include <qnamespace.h>

PidDialog::PidDialog(QWidget *parent) : QDialog(parent),
	search{new QLineEdit(this)}, listWidget{new QListWidget(this)} {

	setWindowTitle("Select a PID");
	auto *layout = new QVBoxLayout(this);

	search->setPlaceholderText(tr("Search..."));
	layout->addWidget(search);
	layout->addWidget(listWidget);

	populateList();

	// Filter as the user types
	connect(search, &QLineEdit::textChanged,
		 this, &PidDialog::filterItems);

	// Selection by double-click or Enter key
	connect(listWidget, &QListWidget::itemActivated,
		 this, &PidDialog::onItemActivated);

	connect(search, &QLineEdit::returnPressed, this, [this]() {
		if (auto *it = listWidget->currentItem())
			onItemActivated(it);
	});
	search->installEventFilter(this);
	listWidget->installEventFilter(this);
	search->setFocus();

}

void PidDialog::filterItems(const QString &text) {
	for (int i = 0; i < listWidget->count(); ++i) {
		auto *item = listWidget->item(i);
		bool match = item->text().contains(text, Qt::CaseInsensitive);
		item->setHidden(!match);
	}
	// select first visible
	for (int i = 0; i < listWidget->count(); ++i) {
		if (!listWidget->item(i)->isHidden()) {
			listWidget->setCurrentRow(i);
			break;
		}
	}
}

bool PidDialog::eventFilter(QObject *watched, QEvent *event) {
	if (event->type() != QEvent::KeyPress)
		return QDialog::eventFilter(watched, event);

	auto *ke = static_cast<QKeyEvent*>(event);

	// 1) In the search box: Up/Down → move focus into the list and replay the arrow
	if (watched == search &&
		(ke->key() == Qt::Key_Down || ke->key() == Qt::Key_Up))
	{
		listWidget->setFocus();
		// repost the same arrow key event so the list will act on it
		auto *clone = ke->clone();
		QCoreApplication::postEvent(listWidget, clone);
		return true;
	}

	// 2) In the list: Up/Down → custom nav that skips hidden items
	if (watched == listWidget &&
		(ke->key() == Qt::Key_Down || ke->key() == Qt::Key_Up))
	{
		const int step  = (ke->key() == Qt::Key_Down) ? 1 : -1;
		const int count = listWidget->count();
		int row         = listWidget->currentRow();

		for (int i = row + step; i >= 0 && i < count; i += step) {
			if (!listWidget->item(i)->isHidden()) {
				listWidget->setCurrentRow(i);
				listWidget->scrollToItem(listWidget->currentItem(), QAbstractItemView::PositionAtCenter);
				break;
			}
		}
		return true;
	}

	// 3) In the list: any other key → send back to search so they can keep typing
	if (watched == listWidget) {
		search->setFocus();
		auto *clone = ke->clone();
		QCoreApplication::postEvent(search, clone);
		return true;
	}

	// 4) Enter/Return anywhere → activate the current item
	if (ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) {
		if (auto *it = listWidget->currentItem())
			onItemActivated(it);
		return true;
	}

	return QDialog::eventFilter(watched, event);
}

void PidDialog::onItemActivated(QListWidgetItem *item) {
	int pid = item->data(Qt::UserRole).toInt();
	QString displayText = item->text();
	emit pidSelected(pid, displayText);
	accept();
}

void PidDialog::populateList() {
	QDir procDir("/proc");
	QStringList entries = procDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
	int maxPidDigits = 0;
	struct Entry { int pid; QString name; };
	QVector<Entry> entriesData;

	for (const QString &entry : entries) {
		bool ok = false;
		int pid = entry.toInt(&ok);
		if (!ok) continue;

		QString commPath = QString("/proc/%1/comm").arg(pid);
		QFile commFile(commPath);
		QString procName = tr("<unknown>");
		if (commFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
			QTextStream in(&commFile);
			procName = in.readLine().trimmed();
		}

		entriesData.append({pid, procName});
		maxPidDigits = qMax(maxPidDigits, QString::number(pid).length());
	}

	for (const auto &e : entriesData) {
		QString itemText = QString("%1: %2")
			.arg(e.pid, -maxPidDigits)
			.arg(e.name);
		auto *item = new QListWidgetItem(itemText, listWidget);
		item->setData(Qt::UserRole, e.pid);
	}
	if (listWidget->count() > 0)
		listWidget->setCurrentRow(0);
}
