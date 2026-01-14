/*
 * QLTOTapeMan - Qt-based LTO Tape Manager
 *
 * Copyright (c) 2026 Jeffrey ZHU (zhxsh1225@gmail.com)
 * https://github.com/Gypsop/QLTOTapeMan
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "DeviceSelectWidget.h"

#include <QHBoxLayout>

namespace qltfs {
namespace app {

DeviceSelectWidget::DeviceSelectWidget(QWidget *parent)
    : QWidget(parent)
    , m_deviceCombo(nullptr)
    , m_refreshButton(nullptr)
    , m_connectButton(nullptr)
    , m_disconnectButton(nullptr)
    , m_statusLabel(nullptr)
    , m_autoRefreshTimer(nullptr)
    , m_connected(false)
    , m_autoRefresh(false)
    , m_autoRefreshInterval(30000)  // 30 seconds
{
    setupUi();

    // Setup auto-refresh timer
    m_autoRefreshTimer = new QTimer(this);
    connect(m_autoRefreshTimer, &QTimer::timeout, this, &DeviceSelectWidget::onAutoRefreshTimer);

    // Initial device scan
    refreshDevices();
}

void DeviceSelectWidget::setupUi()
{
    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(5);

    // Device label
    QLabel *label = new QLabel(tr("Tape Device:"), this);
    layout->addWidget(label);

    // Device combo box
    m_deviceCombo = new QComboBox(this);
    m_deviceCombo->setMinimumWidth(300);
    m_deviceCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    connect(m_deviceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &DeviceSelectWidget::onDeviceSelected);
    layout->addWidget(m_deviceCombo);

    // Refresh button
    m_refreshButton = new QPushButton(tr("Refresh"), this);
    m_refreshButton->setIcon(QIcon::fromTheme("view-refresh"));
    connect(m_refreshButton, &QPushButton::clicked, this, &DeviceSelectWidget::refreshDevices);
    layout->addWidget(m_refreshButton);

    // Connect button
    m_connectButton = new QPushButton(tr("Connect"), this);
    m_connectButton->setIcon(QIcon::fromTheme("network-connect"));
    connect(m_connectButton, &QPushButton::clicked, this, &DeviceSelectWidget::onConnectClicked);
    layout->addWidget(m_connectButton);

    // Disconnect button
    m_disconnectButton = new QPushButton(tr("Disconnect"), this);
    m_disconnectButton->setIcon(QIcon::fromTheme("network-disconnect"));
    m_disconnectButton->setEnabled(false);
    connect(m_disconnectButton, &QPushButton::clicked, this, &DeviceSelectWidget::onDisconnectClicked);
    layout->addWidget(m_disconnectButton);

    // Status indicator
    m_statusLabel = new QLabel("●", this);
    m_statusLabel->setStyleSheet("color: gray;");
    m_statusLabel->setToolTip(tr("Not connected"));
    layout->addWidget(m_statusLabel);

    setLayout(layout);
}

QString DeviceSelectWidget::selectedDevice() const
{
    int index = m_deviceCombo->currentIndex();
    if (index >= 0 && index < m_devices.size()) {
        return m_devices[index].devicePath;
    }
    return QString();
}

void DeviceSelectWidget::setSelectedDevice(const QString &devicePath)
{
    for (int i = 0; i < m_devices.size(); ++i) {
        if (m_devices[i].devicePath == devicePath) {
            m_deviceCombo->setCurrentIndex(i);
            return;
        }
    }
}

void DeviceSelectWidget::setAutoRefresh(bool enable)
{
    m_autoRefresh = enable;
    if (enable && !m_connected) {
        m_autoRefreshTimer->start(m_autoRefreshInterval);
    } else {
        m_autoRefreshTimer->stop();
    }
}

void DeviceSelectWidget::refreshDevices()
{
    QString currentDevice = selectedDevice();

    m_devices = device::DeviceEnumerator::enumerate();

    m_deviceCombo->clear();
    for (const device::DeviceInfo &dev : m_devices) {
        QString displayText = QString("%1 - %2 %3")
            .arg(dev.devicePath)
            .arg(dev.vendor)
            .arg(dev.model);
        m_deviceCombo->addItem(displayText, dev.devicePath);
    }

    // Try to restore selection
    if (!currentDevice.isEmpty()) {
        setSelectedDevice(currentDevice);
    }

    emit devicesRefreshed();
}

void DeviceSelectWidget::setConnected(bool connected)
{
    m_connected = connected;
    updateButtonStates();

    if (connected) {
        m_statusLabel->setStyleSheet("color: green;");
        m_statusLabel->setToolTip(tr("Connected"));
        if (m_autoRefreshTimer->isActive()) {
            m_autoRefreshTimer->stop();
        }
    } else {
        m_statusLabel->setStyleSheet("color: gray;");
        m_statusLabel->setToolTip(tr("Not connected"));
        if (m_autoRefresh) {
            m_autoRefreshTimer->start(m_autoRefreshInterval);
        }
    }

    emit connectionStateChanged(connected);
}

void DeviceSelectWidget::onDeviceSelected(int index)
{
    Q_UNUSED(index)
    emit deviceChanged(selectedDevice());
}

void DeviceSelectWidget::onConnectClicked()
{
    QString device = selectedDevice();
    if (!device.isEmpty()) {
        emit connectRequested(device);
    }
}

void DeviceSelectWidget::onDisconnectClicked()
{
    emit disconnectRequested();
}

void DeviceSelectWidget::onAutoRefreshTimer()
{
    if (!m_connected) {
        refreshDevices();
    }
}

void DeviceSelectWidget::updateButtonStates()
{
    m_deviceCombo->setEnabled(!m_connected);
    m_refreshButton->setEnabled(!m_connected);
    m_connectButton->setEnabled(!m_connected && !selectedDevice().isEmpty());
    m_disconnectButton->setEnabled(m_connected);
}

} // namespace app
} // namespace qltfs
