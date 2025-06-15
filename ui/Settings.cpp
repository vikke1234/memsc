#include "ui/Settings.h"
#include <QSettings>
#include <QLineEdit>
#include <QSpinBox>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QCheckBox>


#include <QSpinBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QVBoxLayout>
#include <QSettings>
#include <QRegularExpressionValidator>
#include <QRegularExpression>
#include <limits>

SettingsDialog::SettingsDialog(QWidget *parent)
    : QDialog(parent),
      updateIntervalSpin(new QSpinBox(this)),
      autoAttachPid(new QSpinBox(this)),
      scanBlockSizeEdit(new QLineEdit(this)),
      formLayout(new QFormLayout),
      buttonBox(new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this))
{
    setWindowTitle("Settings");

    updateIntervalSpin->setRange(1, 60000);
    updateIntervalSpin->setSuffix("ms");
    autoAttachPid->setMaximum(std::numeric_limits<int>::max());
    autoAttachPid->setMinimum(-1);

    formLayout->addRow(tr("Update Interval:"), updateIntervalSpin);
    formLayout->addRow(tr("Auto-Attach:"), autoAttachPid);
    formLayout->addRow(tr("Scan Block Size:"), scanBlockSizeEdit);

    QRegularExpression regex("^(0[xX][0-9a-fA-F]+|\\d+)$");
    QRegularExpressionValidator *validator = new QRegularExpressionValidator(regex, scanBlockSizeEdit);
    scanBlockSizeEdit->setValidator(validator);
    scanBlockSizeEdit->setToolTip("C language convention used, no prefix = dec, 0x = hex, 0b = binary, 0 = octal");


    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->addLayout(formLayout);
    mainLayout->addWidget(buttonBox);
    setLayout(mainLayout);

    loadSettings();

    connect(buttonBox, &QDialogButtonBox::accepted, this, [this]() {
        if (saveSettings()) {
            accept();
        } else {
            reject();
        }
    });
    connect(buttonBox, &QDialogButtonBox::rejected, this, &SettingsDialog::reject);
}

void SettingsDialog::loadSettings()
{
    QSettings settings{"Memory Scanner", "Memsc"};
    settings.beginGroup("General");
    updateIntervalSpin->setValue(
        settings.value("update-interval", 100).toInt());
    autoAttachPid->setValue(settings.value("auto-attach", -1).toInt());
    QString hexstr = QString("0x%1").arg(
        settings.value("scan-block-size", 0x1000000).toULongLong(), 0, 16);
    scanBlockSizeEdit->setText(hexstr);
    settings.endGroup();
}

bool SettingsDialog::saveSettings()
{
    bool ok = false;
    QSettings settings{"Memory Scanner", "Memsc"};

    settings.beginGroup("General");
    qulonglong size = scanBlockSizeEdit->text().toULongLong(&ok, 0);

    if (!ok) {
        return false;
    }
    settings.setValue("update-interval", updateIntervalSpin->value());
    settings.setValue("auto-attach", autoAttachPid->value());
    settings.setValue("scan-block-size", size);

    settings.endGroup();
    settings.sync();
    emit settings_changed();
    return true;
}
