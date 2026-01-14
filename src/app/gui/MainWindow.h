/*
 * QLTOTapeMan - Qt-based LTO Tape Manager
 *
 * Copyright (c) 2026 Jeffrey ZHU (zhxsh1225@gmail.com)
 * https://github.com/Gypsop/QLTOTapeMan
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <QMainWindow>
#include <QSettings>
#include <QSharedPointer>

#include "device/DeviceEnumerator.h"
#include "device/TapeDevice.h"
#include "core/LtfsIndex.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

/**
 * @brief Main application window
 *
 * Provides the primary user interface for tape management operations
 * including device selection, file browsing, and data transfer.
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent *event) override;
    void showEvent(QShowEvent *event) override;

private slots:
    // Menu actions
    void onActionNewSession();
    void onActionOpenIndex();
    void onActionSaveIndex();
    void onActionSaveIndexAs();
    void onActionExit();

    void onActionRefreshDevices();
    void onActionDeviceProperties();
    void onActionFormatTape();
    void onActionEjectTape();

    void onActionDirectReadWrite();
    void onActionVerifyFiles();
    void onActionSettings();

    void onActionAbout();
    void onActionAboutQt();

    // Device operations
    void onDeviceChanged(int index);
    void onDeviceStatusChanged();
    void onConnectDevice();
    void onDisconnectDevice();

    // Tape operations
    void onMountTape();
    void onUnmountTape();
    void onTapeStatusRefresh();

    // Transfer operations
    void onWriteFiles();
    void onReadFiles();
    void onTransferProgress(int percent, qint64 bytesTransferred, qint64 totalBytes);
    void onTransferComplete(bool success, const QString &message);

private:
    void setupUi();
    void setupMenus();
    void setupToolBar();
    void setupStatusBar();
    void setupConnections();

    void loadSettings();
    void saveSettings();

    void refreshDeviceList();
    void updateDeviceStatus();
    void updateTapeInfo();
    void updateUiState();

    void setConnected(bool connected);
    void showError(const QString &title, const QString &message);
    void showInfo(const QString &title, const QString &message);
    bool askConfirmation(const QString &title, const QString &message);

private:
    Ui::MainWindow *ui;
    QSettings m_settings;

    qltfs::DeviceEnumerator m_enumerator;
    QSharedPointer<qltfs::TapeDevice> m_device;
    QSharedPointer<qltfs::LtfsIndex> m_currentIndex;

    bool m_connected = false;
    bool m_firstShow = true;
    QString m_currentIndexPath;
};
