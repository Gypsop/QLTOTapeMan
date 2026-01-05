#include "DeviceStatusWidget.h"

DeviceStatusWidget::DeviceStatusWidget(QWidget *parent) : QWidget(parent)
{
    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(2, 2, 2, 2);
    layout->setSpacing(4);

    m_ledOpStatus = new QLabel(this);
    setupLed(m_ledOpStatus, "OP");

    m_ledEncryption = new QLabel(this);
    setupLed(m_ledEncryption, "ENC");

    m_ledCleaning = new QLabel(this);
    setupLed(m_ledCleaning, "CLN");

    m_ledTapeStatus = new QLabel(this);
    setupLed(m_ledTapeStatus, "TAPE");

    m_ledDriveStatus = new QLabel(this);
    setupLed(m_ledDriveStatus, "DRV");

    m_ledActivity = new QLabel(this);
    setupLed(m_ledActivity, "ACT");

    layout->addWidget(m_ledOpStatus);
    layout->addWidget(m_ledEncryption);
    layout->addWidget(m_ledCleaning);
    layout->addWidget(m_ledTapeStatus);
    layout->addWidget(m_ledDriveStatus);
    layout->addWidget(m_ledActivity);
    layout->addStretch(); // Push LEDs to the left, or remove if we want them centered/right
    
    // Actually, user said "at the end of the row". 
    // If we put this widget in a column, it will fill the column.
    // Let's remove stretch so they are compact.
    layout->removeItem(layout->itemAt(layout->count() - 1));
    
    reset();
}

void DeviceStatusWidget::setupLed(QLabel *label, const QString &text)
{
    label->setText(text);
    label->setAlignment(Qt::AlignCenter);
    label->setMinimumWidth(40);
}

void DeviceStatusWidget::setLedStyle(QLabel *label, const QString &color, const QString &text, const QString &textColor)
{
    QString style = QString("QLabel { background-color: %1; color: %2; padding: 2px; border-radius: 4px; min-width: 40px; qproperty-alignment: AlignCenter; }")
                        .arg(color, textColor);
    label->setStyleSheet(style);
    if (!text.isEmpty()) {
        label->setText(text);
    }
}

void DeviceStatusWidget::setStatus(const QString &statusName, const QString &color, const QString &text, const QString &textColor)
{
    if (statusName == "OP") setLedStyle(m_ledOpStatus, color, text, textColor);
    else if (statusName == "ENC") setLedStyle(m_ledEncryption, color, text, textColor);
    else if (statusName == "CLN") setLedStyle(m_ledCleaning, color, text, textColor);
    else if (statusName == "TAPE") setLedStyle(m_ledTapeStatus, color, text, textColor);
    else if (statusName == "DRV") setLedStyle(m_ledDriveStatus, color, text, textColor);
    else if (statusName == "ACT") setLedStyle(m_ledActivity, color, text, textColor);
}

void DeviceStatusWidget::reset()
{
    setStatus("OP", "gray");
    setStatus("ENC", "gray");
    setStatus("CLN", "gray");
    setStatus("TAPE", "gray");
    setStatus("DRV", "gray");
    setStatus("ACT", "gray");
}
