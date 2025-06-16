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
    epsilonLineEdit(new QLineEdit(this)),
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
    formLayout->addRow(tr("Epsilon"), epsilonLineEdit);

    QRegularExpression size_regex("^(0[xX][0-9a-fA-F]+|\\d+)$");
    QRegularExpressionValidator *size_validator = new QRegularExpressionValidator(size_regex, scanBlockSizeEdit);
    scanBlockSizeEdit->setValidator(size_validator);
    scanBlockSizeEdit->setToolTip("C language convention used, no prefix = dec, 0x = hex, 0b = binary, 0 = octal");

    QRegularExpression epsilonRegex("^\\d+([,.]\\d*)?");

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
    connect(buttonBox, &QDialogButtonBox::rejected, this,
            &SettingsDialog::reject);
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

    double epsilon = settings.value("epsilon", 0.00000001).toDouble();
    epsilonLineEdit->setText(QString::number(epsilon, 'f', 9));
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
    double epsilon = epsilonLineEdit->text().toDouble(&ok);
    if (!ok) {
        return false;
    }

    settings.setValue("update-interval", updateIntervalSpin->value());
    settings.setValue("auto-attach", autoAttachPid->value());
    settings.setValue("scan-block-size", size);
    settings.setValue("epsilon", epsilon);

    settings.endGroup();
    settings.sync();
    emit settings_changed();
    return true;
}
