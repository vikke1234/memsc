#pragma once
#include <QDialog>
#include <QLineEdit>
#include <QListWidget>

class PidDialog : public QDialog {
	Q_OBJECT;

	QLineEdit *search;
	QListWidget *listWidget;

public:
	explicit PidDialog(QWidget *parent = nullptr);


signals:
	void pidSelected(int pid, const QString &displayText);

protected:
	bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
	void filterItems(const QString &text);
	void onItemActivated(QListWidgetItem *item);

private:
	void populateList();

};
