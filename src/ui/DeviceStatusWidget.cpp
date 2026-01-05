#include "DeviceStatusWidget.h"

DeviceStatusWidget::DeviceStatusWidget(QWidget *parent) : QWidget(parent)
{
    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(4, 2, 4, 2);
    layout->setSpacing(6);

    // Create LEDs
    m_ledOpStatus = new QLabel(this);
    setupLed(m_ledOpStatus, "RDY"); // Ready

    m_ledEncryption = new QLabel(this);
    setupLed(m_ledEncryption, "ENC"); // Encryption

    m_ledCleaning = new QLabel(this);
    setupLed(m_ledCleaning, "CLN"); // Cleaning

    m_ledTapeStatus = new QLabel(this);
    setupLed(m_ledTapeStatus, "TP"); // Tape

    m_ledDriveStatus = new QLabel(this);
    setupLed(m_ledDriveStatus, "DRV"); // Drive

    m_ledActivity = new QLabel(this);
    setupLed(m_ledActivity, "ACT"); // Activity

    layout->addWidget(m_ledOpStatus);
    layout->addWidget(m_ledEncryption);
    layout->addWidget(m_ledCleaning);
    layout->addWidget(m_ledTapeStatus);
    layout->addWidget(m_ledDriveStatus);
    layout->addWidget(m_ledActivity);
    
    // Info Label
    m_infoLabel = new QLabel(this);
    m_infoLabel->setStyleSheet("color: #333; font-size: 11px; margin-left: 4px;");
    layout->addWidget(m_infoLabel);
    
    layout->addStretch(); // Push everything to the left
    
    reset();
}

void DeviceStatusWidget::setupLed(QLabel *label, const QString &text)
{
    label->setText(text);
    label->setAlignment(Qt::AlignCenter);
    label->setFixedSize(36, 20); // Fixed size to look like a small indicator
    // Initial style
    label->setStyleSheet("QLabel { background-color: gray; color: white; border-radius: 4px; font-weight: bold; font-size: 10px; }");
}

void DeviceStatusWidget::setLedStyle(QLabel *label, const QString &color, const QString &text, const QString &textColor)
{
    // We do NOT change the text of the LED label itself (it keeps "RDY", "ENC" etc.)
    // We only change the color.
    // The 'text' passed here is used for the tooltip and the info label.
    
    QString style = QString("QLabel { background-color: %1; color: %2; border-radius: 4px; font-weight: bold; font-size: 10px; }")
                        .arg(color, textColor);
    label->setStyleSheet(style);
    
    if (!text.isEmpty()) {
        label->setToolTip(text);
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
    
    // Update Info Label
    if (!text.isEmpty()) {
        m_statusMessages[statusName] = text;
    }
    updateInfoLabel();
}

void DeviceStatusWidget::updateInfoLabel()
{
    QStringList parts;
    
    // Define priority order for display
    QStringList keys = {"OP", "DRV", "TAPE", "CLN", "ACT"};
    
    for (const QString &key : keys) {
        if (m_statusMessages.contains(key)) {
            QString msg = m_statusMessages[key];
            // Filter out common "normal" states to reduce clutter, unless it's the only thing
            if (msg == "IDLE" || msg == "OFF" || msg == "OK" || msg == "RW") continue;
            parts << msg;
        }
    }
    
    // If everything is normal, show "Ready" or "Idle"
    if (parts.isEmpty()) {
        if (m_statusMessages.value("OP") == "READY") parts << "Ready";
        else if (m_statusMessages.value("OP") == "RW") parts << "Ready";
        else parts << "Idle";
    }
    
    m_infoLabel->setText(parts.join(" | "));
}

void DeviceStatusWidget::reset()
{
    m_statusMessages.clear();
    setStatus("OP", "gray", "IDLE");
    setStatus("ENC", "gray", "OFF");
    setStatus("CLN", "gray", "OK");
    setStatus("TAPE", "gray", "EMPTY");
    setStatus("DRV", "gray", "IDLE");
    setStatus("ACT", "gray", "IDLE");
}
