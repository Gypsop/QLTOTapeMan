/*
 * QLTOTapeMan - Qt-based LTO Tape Manager
 *
 * Copyright (c) 2026 Jeffrey ZHU (zhxsh1225@gmail.com)
 * https://github.com/Gypsop/QLTOTapeMan
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef DEVICESELECTWIDGET_H
#define DEVICESELECTWIDGET_H

#include <QWidget>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QHBoxLayout>
#include <QTimer>

#include "DeviceEnumerator.h"

namespace qltfs {
namespace app {

/**
 * @brief DeviceSelectWidget - Tape device selection widget
 *
 * A reusable widget for selecting tape devices, featuring:
 * - Dropdown list of available devices
 * - Refresh button
 * - Connect/Disconnect buttons
 * - Device status indicator
 */
class DeviceSelectWidget : public QWidget
{
    Q_OBJECT

    Q_PROPERTY(QString selectedDevice READ selectedDevice WRITE setSelectedDevice NOTIFY deviceChanged)
    Q_PROPERTY(bool connected READ isConnected NOTIFY connectionStateChanged)
    Q_PROPERTY(bool autoRefresh READ autoRefresh WRITE setAutoRefresh)

public:
    explicit DeviceSelectWidget(QWidget *parent = nullptr);
    virtual ~DeviceSelectWidget() = default;

    /**
     * @brief Get currently selected device path
     */
    QString selectedDevice() const;

    /**
     * @brief Set the selected device
     */
    void setSelectedDevice(const QString &devicePath);

    /**
     * @brief Check if currently connected
     */
    bool isConnected() const { return m_connected; }

    /**
     * @brief Get/Set auto-refresh enabled
     */
    bool autoRefresh() const { return m_autoRefresh; }
    void setAutoRefresh(bool enable);

    /**
     * @brief Get the list of available devices
     */
    QList<device::DeviceInfo> availableDevices() const { return m_devices; }

signals:
    /**
     * @brief Emitted when selected device changes
     */
    void deviceChanged(const QString &devicePath);

    /**
     * @brief Emitted when connection state changes
     */
    void connectionStateChanged(bool connected);

    /**
     * @brief Emitted when connect is requested
     */
    void connectRequested(const QString &devicePath);

    /**
     * @brief Emitted when disconnect is requested
     */
    void disconnectRequested();

    /**
     * @brief Emitted when device list is refreshed
     */
    void devicesRefreshed();

public slots:
    /**
     * @brief Refresh the device list
     */
    void refreshDevices();

    /**
     * @brief Set connection state (called by parent after connect/disconnect)
     */
    void setConnected(bool connected);

private slots:
    void onDeviceSelected(int index);
    void onConnectClicked();
    void onDisconnectClicked();
    void onAutoRefreshTimer();

private:
    void setupUi();
    void updateButtonStates();

    QComboBox *m_deviceCombo;
    QPushButton *m_refreshButton;
    QPushButton *m_connectButton;
    QPushButton *m_disconnectButton;
    QLabel *m_statusLabel;
    QTimer *m_autoRefreshTimer;

    QList<device::DeviceInfo> m_devices;
    bool m_connected;
    bool m_autoRefresh;
    int m_autoRefreshInterval;
};

} // namespace app
} // namespace qltfs

#endif // DEVICESELECTWIDGET_H
