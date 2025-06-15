#pragma once

#include <QDialog>
#include <QWidget>
#include <QVariant>

class QSpinBox;
class QCheckBox;
class QLineEdit;
class QFormLayout;
class QDialogButtonBox;

class SettingsDialog : public QDialog {
    Q_OBJECT

signals:
    void settings_changed();

public:
    explicit SettingsDialog(QWidget *parent = nullptr);
    ~SettingsDialog() override = default;

    void loadSettings();
    bool saveSettings();

private:
    // General settings provided by user
    QSpinBox  *updateIntervalSpin;     // "update-interval"
    QSpinBox  *autoAttachPid;       // "auto-attach"
    QLineEdit *scanBlockSizeEdit;     // "scan-block-size"

    QFormLayout *formLayout;
    QDialogButtonBox *buttonBox;
};
