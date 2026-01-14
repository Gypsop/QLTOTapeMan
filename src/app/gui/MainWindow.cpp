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

#include "MainWindow.h"
#include "ui_MainWindow.h"

#include "FileBrowserDialog.h"
#include "LtfsWriterWindow.h"
#include "ProgressDialog.h"
#include "SettingsDialog.h"
#include "AboutDialog.h"

#include "xml/IndexParser.h"
#include "xml/IndexWriter.h"

#include <QCloseEvent>
#include <QMessageBox>
#include <QFileDialog>
#include <QStatusBar>
#include <QLabel>
#include <QProgressBar>
#include <QTimer>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_settings(QStringLiteral("JeffreyZHU"), QStringLiteral("QLTOTapeMan"))
{
    ui->setupUi(this);

    setupMenus();
    setupToolBar();
    setupStatusBar();
    setupConnections();
    loadSettings();
    updateUiState();
}

MainWindow::~MainWindow()
{
    saveSettings();

    if (m_device && m_device->isOpen()) {
        m_device->close();
    }

    delete ui;
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (m_connected) {
        if (askConfirmation(tr("Disconnect"),
                           tr("A tape device is connected. Do you want to disconnect and exit?"))) {
            onDisconnectDevice();
        } else {
            event->ignore();
            return;
        }
    }

    saveSettings();
    event->accept();
}

void MainWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);

    if (m_firstShow) {
        m_firstShow = false;

        // Auto-refresh devices on startup
        QTimer::singleShot(100, this, &MainWindow::onActionRefreshDevices);
    }
}

void MainWindow::setupUi()
{
    // Additional UI setup if needed
}

void MainWindow::setupMenus()
{
    // File menu
    connect(ui->actionNew, &QAction::triggered, this, &MainWindow::onActionNewSession);
    connect(ui->actionOpenIndex, &QAction::triggered, this, &MainWindow::onActionOpenIndex);
    connect(ui->actionSaveIndex, &QAction::triggered, this, &MainWindow::onActionSaveIndex);
    connect(ui->actionSaveIndexAs, &QAction::triggered, this, &MainWindow::onActionSaveIndexAs);
    connect(ui->actionExit, &QAction::triggered, this, &MainWindow::onActionExit);

    // Device menu
    connect(ui->actionRefreshDevices, &QAction::triggered, this, &MainWindow::onActionRefreshDevices);
    connect(ui->actionDeviceProperties, &QAction::triggered, this, &MainWindow::onActionDeviceProperties);
    connect(ui->actionFormatTape, &QAction::triggered, this, &MainWindow::onActionFormatTape);
    connect(ui->actionEjectTape, &QAction::triggered, this, &MainWindow::onActionEjectTape);

    // Tools menu
    connect(ui->actionDirectReadWrite, &QAction::triggered, this, &MainWindow::onActionDirectReadWrite);
    connect(ui->actionVerifyFiles, &QAction::triggered, this, &MainWindow::onActionVerifyFiles);
    connect(ui->actionSettings, &QAction::triggered, this, &MainWindow::onActionSettings);

    // Help menu
    connect(ui->actionAbout, &QAction::triggered, this, &MainWindow::onActionAbout);
    connect(ui->actionAboutQt, &QAction::triggered, this, &MainWindow::onActionAboutQt);
}

void MainWindow::setupToolBar()
{
    // Connect toolbar buttons
    connect(ui->btnRefresh, &QPushButton::clicked, this, &MainWindow::onActionRefreshDevices);
    connect(ui->btnConnect, &QPushButton::clicked, this, &MainWindow::onConnectDevice);
    connect(ui->btnDisconnect, &QPushButton::clicked, this, &MainWindow::onDisconnectDevice);
    connect(ui->btnMount, &QPushButton::clicked, this, &MainWindow::onMountTape);
    connect(ui->btnEject, &QPushButton::clicked, this, &MainWindow::onActionEjectTape);
    connect(ui->btnWrite, &QPushButton::clicked, this, &MainWindow::onWriteFiles);
    connect(ui->btnRead, &QPushButton::clicked, this, &MainWindow::onReadFiles);
}

void MainWindow::setupStatusBar()
{
    // Device status label
    auto *deviceStatusLabel = new QLabel(tr("No device connected"));
    deviceStatusLabel->setObjectName(QStringLiteral("deviceStatusLabel"));
    statusBar()->addWidget(deviceStatusLabel);

    // Spacer
    auto *spacer = new QWidget();
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    statusBar()->addWidget(spacer);

    // Transfer progress (hidden by default)
    auto *progressBar = new QProgressBar();
    progressBar->setObjectName(QStringLiteral("transferProgress"));
    progressBar->setMaximumWidth(200);
    progressBar->setVisible(false);
    statusBar()->addPermanentWidget(progressBar);
}

void MainWindow::setupConnections()
{
    // Device combo box
    connect(ui->comboDevice, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onDeviceChanged);
}

void MainWindow::loadSettings()
{
    // Restore window geometry
    restoreGeometry(m_settings.value(QStringLiteral("window/geometry")).toByteArray());
    restoreState(m_settings.value(QStringLiteral("window/state")).toByteArray());

    // Restore last used device
    QString lastDevice = m_settings.value(QStringLiteral("device/last")).toString();
    if (!lastDevice.isEmpty()) {
        int index = ui->comboDevice->findData(lastDevice);
        if (index >= 0) {
            ui->comboDevice->setCurrentIndex(index);
        }
    }
}

void MainWindow::saveSettings()
{
    // Save window geometry
    m_settings.setValue(QStringLiteral("window/geometry"), saveGeometry());
    m_settings.setValue(QStringLiteral("window/state"), saveState());

    // Save current device
    if (ui->comboDevice->currentIndex() >= 0) {
        m_settings.setValue(QStringLiteral("device/last"),
                           ui->comboDevice->currentData());
    }
}

void MainWindow::refreshDeviceList()
{
    ui->comboDevice->clear();
    ui->comboDevice->addItem(tr("-- Select Device --"), QString());

    if (!m_enumerator.refresh()) {
        showError(tr("Device Enumeration"),
                 tr("Failed to enumerate tape devices: %1").arg(m_enumerator.lastError()));
        return;
    }

    const auto &devices = m_enumerator.tapeDevices();
    for (const auto &device : devices) {
        ui->comboDevice->addItem(device.displayName(), device.devicePath);
    }

    statusBar()->showMessage(tr("Found %1 tape device(s)").arg(devices.size()), 3000);
}

void MainWindow::updateDeviceStatus()
{
    auto *statusLabel = statusBar()->findChild<QLabel *>(QStringLiteral("deviceStatusLabel"));
    if (!statusLabel) return;

    if (!m_connected || !m_device) {
        statusLabel->setText(tr("No device connected"));
        return;
    }

    QString status;
    switch (m_device->status()) {
    case qltfs::TapeStatus::Ready:
        status = tr("Ready");
        break;
    case qltfs::TapeStatus::NoMedia:
        status = tr("No Media");
        break;
    case qltfs::TapeStatus::Loading:
        status = tr("Loading...");
        break;
    case qltfs::TapeStatus::Rewinding:
        status = tr("Rewinding...");
        break;
    case qltfs::TapeStatus::WriteProtected:
        status = tr("Write Protected");
        break;
    case qltfs::TapeStatus::Error:
        status = tr("Error");
        break;
    default:
        status = tr("Unknown");
        break;
    }

    statusLabel->setText(tr("Device: %1 - %2")
                        .arg(m_device->deviceInfo().displayName(), status));
}

void MainWindow::updateTapeInfo()
{
    if (!m_device || !m_device->isOpen()) {
        ui->lblTapeStatus->setText(tr("Not connected"));
        ui->lblMediaType->setText(QStringLiteral("-"));
        ui->lblCapacity->setText(QStringLiteral("-"));
        ui->lblVolumeId->setText(QStringLiteral("-"));
        return;
    }

    // Refresh media info
    m_device->refreshMediaInfo();
    const auto &mediaInfo = m_device->mediaInfo();

    ui->lblTapeStatus->setText(m_device->status() == qltfs::TapeStatus::Ready ?
                               tr("Ready") : tr("Not Ready"));
    ui->lblMediaType->setText(mediaInfo.mediaType.isEmpty() ?
                              tr("Unknown") : mediaInfo.mediaType);
    ui->lblCapacity->setText(QStringLiteral("%1 / %2")
                            .arg(qltfs::formatSize(mediaInfo.remainingCapacity),
                                 qltfs::formatSize(mediaInfo.totalCapacity)));

    if (m_currentIndex) {
        ui->lblVolumeId->setText(m_currentIndex->volumeUuid().toString(QUuid::WithoutBraces).left(8));
    } else {
        ui->lblVolumeId->setText(tr("No index loaded"));
    }
}

void MainWindow::updateUiState()
{
    bool hasDevice = (ui->comboDevice->currentIndex() > 0);

    // Connection buttons
    ui->btnConnect->setEnabled(hasDevice && !m_connected);
    ui->btnDisconnect->setEnabled(m_connected);

    // Tape operations
    ui->btnMount->setEnabled(m_connected);
    ui->btnEject->setEnabled(m_connected);

    // File operations
    bool ready = m_connected && m_device && m_device->status() == qltfs::TapeStatus::Ready;
    ui->btnWrite->setEnabled(ready);
    ui->btnRead->setEnabled(ready && m_currentIndex);

    // Menu actions
    ui->actionFormatTape->setEnabled(ready);
    ui->actionEjectTape->setEnabled(m_connected);
    ui->actionDeviceProperties->setEnabled(m_connected);
    ui->actionSaveIndex->setEnabled(m_currentIndex != nullptr);
    ui->actionSaveIndexAs->setEnabled(m_currentIndex != nullptr);
}

void MainWindow::setConnected(bool connected)
{
    m_connected = connected;
    updateDeviceStatus();
    updateTapeInfo();
    updateUiState();
}

void MainWindow::showError(const QString &title, const QString &message)
{
    QMessageBox::critical(this, title, message);
}

void MainWindow::showInfo(const QString &title, const QString &message)
{
    QMessageBox::information(this, title, message);
}

bool MainWindow::askConfirmation(const QString &title, const QString &message)
{
    return QMessageBox::question(this, title, message,
                                QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes;
}

// ============================================================================
// Menu Action Slots
// ============================================================================

void MainWindow::onActionNewSession()
{
    if (m_currentIndex) {
        if (!askConfirmation(tr("New Session"),
                            tr("Current session will be closed. Continue?"))) {
            return;
        }
    }

    m_currentIndex.reset();
    m_currentIndexPath.clear();
    updateTapeInfo();
    updateUiState();
}

void MainWindow::onActionOpenIndex()
{
    QString fileName = QFileDialog::getOpenFileName(this,
        tr("Open LTFS Index"),
        m_settings.value(QStringLiteral("paths/lastIndex")).toString(),
        tr("LTFS Index Files (*.xml);;All Files (*)"));

    if (fileName.isEmpty()) {
        return;
    }

    m_settings.setValue(QStringLiteral("paths/lastIndex"), QFileInfo(fileName).absolutePath());

    qltfs::IndexParser parser;
    m_currentIndex = parser.parse(QFile(fileName).readAll());

    if (!m_currentIndex) {
        showError(tr("Open Index"),
                 tr("Failed to parse index file: %1").arg(parser.errorMessage()));
        return;
    }

    m_currentIndexPath = fileName;
    updateTapeInfo();
    updateUiState();

    statusBar()->showMessage(tr("Loaded index with %1 files")
                            .arg(m_currentIndex->rootDirectory().totalFileCount()), 3000);
}

void MainWindow::onActionSaveIndex()
{
    if (!m_currentIndex) {
        return;
    }

    if (m_currentIndexPath.isEmpty()) {
        onActionSaveIndexAs();
        return;
    }

    qltfs::IndexWriter writer;
    if (!writer.writeFile(*m_currentIndex, m_currentIndexPath)) {
        showError(tr("Save Index"),
                 tr("Failed to save index: %1").arg(writer.errorMessage()));
        return;
    }

    statusBar()->showMessage(tr("Index saved"), 3000);
}

void MainWindow::onActionSaveIndexAs()
{
    if (!m_currentIndex) {
        return;
    }

    QString fileName = QFileDialog::getSaveFileName(this,
        tr("Save LTFS Index"),
        m_currentIndexPath.isEmpty() ?
            m_settings.value(QStringLiteral("paths/lastIndex")).toString() :
            m_currentIndexPath,
        tr("LTFS Index Files (*.xml);;All Files (*)"));

    if (fileName.isEmpty()) {
        return;
    }

    qltfs::IndexWriter writer;
    if (!writer.writeFile(*m_currentIndex, fileName)) {
        showError(tr("Save Index"),
                 tr("Failed to save index: %1").arg(writer.errorMessage()));
        return;
    }

    m_currentIndexPath = fileName;
    m_settings.setValue(QStringLiteral("paths/lastIndex"), QFileInfo(fileName).absolutePath());
    statusBar()->showMessage(tr("Index saved"), 3000);
}

void MainWindow::onActionExit()
{
    close();
}

void MainWindow::onActionRefreshDevices()
{
    refreshDeviceList();
}

void MainWindow::onActionDeviceProperties()
{
    if (!m_device) {
        return;
    }

    // Refresh all device info
    m_device->refreshStatus();
    m_device->refreshMediaInfo();
    m_device->refreshLogData();

    QString info;
    const auto &deviceInfo = m_device->deviceInfo();
    const auto &mediaInfo = m_device->mediaInfo();
    const auto &logData = m_device->logData();

    info += tr("Device Information\n");
    info += QStringLiteral("==================\n\n");
    info += tr("Path: %1\n").arg(deviceInfo.devicePath);
    info += tr("Vendor: %1\n").arg(deviceInfo.vendor);
    info += tr("Product: %1\n").arg(deviceInfo.product);
    info += tr("Revision: %1\n").arg(deviceInfo.revision);
    info += tr("Serial: %1\n").arg(deviceInfo.serialNumber);
    info += QStringLiteral("\n");

    info += tr("Media Information\n");
    info += QStringLiteral("==================\n\n");
    info += tr("Type: %1\n").arg(mediaInfo.mediaType);
    info += tr("Total Capacity: %1\n").arg(qltfs::formatSize(mediaInfo.totalCapacity));
    info += tr("Remaining: %1\n").arg(qltfs::formatSize(mediaInfo.remainingCapacity));
    info += tr("Write Protected: %1\n").arg(mediaInfo.isWriteProtected ? tr("Yes") : tr("No"));
    info += tr("LTFS Formatted: %1\n").arg(mediaInfo.isLtfs ? tr("Yes") : tr("No"));
    info += QStringLiteral("\n");

    if (logData.isValid()) {
        info += tr("Drive Statistics\n");
        info += QStringLiteral("==================\n\n");
        info += tr("Temperature: %1°C\n").arg(logData.temperatureCelsius);
        info += tr("Load Count: %1\n").arg(logData.loadCount);
        info += tr("Total Written: %1\n").arg(qltfs::formatSize(logData.totalBytesWritten));
        info += tr("Total Read: %1\n").arg(qltfs::formatSize(logData.totalBytesRead));
    }

    QMessageBox::information(this, tr("Device Properties"), info);
}

void MainWindow::onActionFormatTape()
{
    if (!m_device || !m_device->isOpen()) {
        return;
    }

    if (!askConfirmation(tr("Format Tape"),
                        tr("WARNING: This will erase ALL data on the tape!\n\n"
                           "Are you sure you want to format this tape for LTFS?"))) {
        return;
    }

    // Get volume name
    QString volumeName = QInputDialog::getText(this,
        tr("Volume Name"),
        tr("Enter a name for this tape volume:"),
        QLineEdit::Normal,
        QStringLiteral("LTFS_VOLUME"));

    if (volumeName.isEmpty()) {
        return;
    }

    // Create label
    qltfs::LtfsLabel label;
    label.volumeUuid = QUuid::createUuid();
    label.barcode = volumeName.left(6).toUpper();
    label.creator = QStringLiteral("QLTOTapeMan 1.0.0");
    label.formatTime = QDateTime::currentDateTimeUtc();

    // Show progress dialog
    ProgressDialog progress(tr("Format Tape"), tr("Formatting tape for LTFS..."), this);
    progress.show();

    bool success = m_device->formatForLtfs(label,
        [&progress](qint64 current, qint64 total, const QString &status) {
            progress.setProgress(static_cast<int>(current), static_cast<int>(total));
            progress.setStatusText(status);
            QApplication::processEvents();
        });

    progress.close();

    if (success) {
        showInfo(tr("Format Complete"),
                tr("Tape has been formatted for LTFS.\n\nVolume ID: %1")
                .arg(label.volumeUuid.toString(QUuid::WithoutBraces)));
        updateTapeInfo();
    } else {
        showError(tr("Format Failed"),
                 tr("Failed to format tape: %1").arg(m_device->lastError()));
    }
}

void MainWindow::onActionEjectTape()
{
    if (!m_device) {
        return;
    }

    if (m_device->unload(true)) {
        statusBar()->showMessage(tr("Tape ejected"), 3000);
        updateTapeInfo();
    } else {
        showError(tr("Eject Failed"),
                 tr("Failed to eject tape: %1").arg(m_device->lastError()));
    }
}

void MainWindow::onActionDirectReadWrite()
{
    auto *window = new LtfsWriterWindow(m_device.data(), this);
    window->setAttribute(Qt::WA_DeleteOnClose);
    window->show();
}

void MainWindow::onActionVerifyFiles()
{
    showInfo(tr("Verify Files"),
            tr("File verification feature will be available in a future version."));
}

void MainWindow::onActionSettings()
{
    SettingsDialog dialog(&m_settings, this);
    dialog.exec();
}

void MainWindow::onActionAbout()
{
    AboutDialog dialog(this);
    dialog.exec();
}

void MainWindow::onActionAboutQt()
{
    QMessageBox::aboutQt(this, tr("About Qt"));
}

// ============================================================================
// Device Operation Slots
// ============================================================================

void MainWindow::onDeviceChanged(int index)
{
    Q_UNUSED(index)
    updateUiState();
}

void MainWindow::onDeviceStatusChanged()
{
    updateDeviceStatus();
    updateTapeInfo();
    updateUiState();
}

void MainWindow::onConnectDevice()
{
    QString devicePath = ui->comboDevice->currentData().toString();
    if (devicePath.isEmpty()) {
        return;
    }

    auto deviceInfo = m_enumerator.findTapeDevice(devicePath);
    m_device = QSharedPointer<qltfs::TapeDevice>::create(deviceInfo);

    if (!m_device->open()) {
        showError(tr("Connection Failed"),
                 tr("Failed to open device: %1").arg(m_device->lastError()));
        m_device.reset();
        return;
    }

    setConnected(true);
    statusBar()->showMessage(tr("Connected to %1").arg(deviceInfo.displayName()), 3000);
}

void MainWindow::onDisconnectDevice()
{
    if (m_device) {
        m_device->close();
        m_device.reset();
    }

    setConnected(false);
    statusBar()->showMessage(tr("Disconnected"), 3000);
}

void MainWindow::onMountTape()
{
    if (!m_device) {
        return;
    }

    if (m_device->testReady()) {
        // Try to read LTFS label
        auto label = m_device->readLabel();
        if (label.isValid()) {
            showInfo(tr("Tape Mounted"),
                    tr("LTFS Volume: %1\n"
                       "UUID: %2\n"
                       "Created by: %3")
                    .arg(label.barcode,
                         label.volumeUuid.toString(QUuid::WithoutBraces),
                         label.creator));
        } else {
            showInfo(tr("Tape Ready"),
                    tr("Tape is ready but does not appear to be LTFS formatted."));
        }
    } else {
        showError(tr("Mount Failed"),
                 tr("Tape is not ready: %1").arg(m_device->lastError()));
    }

    updateTapeInfo();
}

void MainWindow::onUnmountTape()
{
    // Just update UI
    updateTapeInfo();
}

void MainWindow::onTapeStatusRefresh()
{
    if (m_device) {
        m_device->refreshStatus();
    }
    updateDeviceStatus();
    updateTapeInfo();
}

// ============================================================================
// Transfer Operation Slots
// ============================================================================

void MainWindow::onWriteFiles()
{
    if (!m_device || !m_device->isOpen()) {
        return;
    }

    FileBrowserDialog dialog(FileBrowserDialog::Mode::SelectForWrite, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    QStringList files = dialog.selectedFiles();
    if (files.isEmpty()) {
        return;
    }

    auto *writer = new LtfsWriterWindow(m_device.data(), this);
    writer->setAttribute(Qt::WA_DeleteOnClose);
    writer->addFiles(files);
    writer->show();
}

void MainWindow::onReadFiles()
{
    if (!m_device || !m_currentIndex) {
        return;
    }

    FileBrowserDialog dialog(FileBrowserDialog::Mode::SelectForRead, this);
    dialog.setIndex(m_currentIndex);

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    // Get destination directory
    QString destDir = QFileDialog::getExistingDirectory(this,
        tr("Select Destination Directory"),
        m_settings.value(QStringLiteral("paths/lastDest")).toString());

    if (destDir.isEmpty()) {
        return;
    }

    m_settings.setValue(QStringLiteral("paths/lastDest"), destDir);

    // TODO: Implement read operation
    showInfo(tr("Read Files"),
            tr("File reading will be implemented in a future version.\n\n"
               "Selected destination: %1").arg(destDir));
}

void MainWindow::onTransferProgress(int percent, qint64 bytesTransferred, qint64 totalBytes)
{
    auto *progressBar = statusBar()->findChild<QProgressBar *>(QStringLiteral("transferProgress"));
    if (progressBar) {
        progressBar->setVisible(true);
        progressBar->setValue(percent);
        progressBar->setFormat(QStringLiteral("%1 / %2")
                              .arg(qltfs::formatSize(bytesTransferred),
                                   qltfs::formatSize(totalBytes)));
    }
}

void MainWindow::onTransferComplete(bool success, const QString &message)
{
    auto *progressBar = statusBar()->findChild<QProgressBar *>(QStringLiteral("transferProgress"));
    if (progressBar) {
        progressBar->setVisible(false);
    }

    if (success) {
        showInfo(tr("Transfer Complete"), message);
    } else {
        showError(tr("Transfer Failed"), message);
    }

    updateTapeInfo();
}
