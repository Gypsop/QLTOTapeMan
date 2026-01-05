#include "MainWindow.h"
#include "ui_MainWindow.h"
#include "ui/SettingsDialog.h"
#include "ui/AboutDialog.h"
#include <QMessageBox>
#include <QInputDialog>
#include <QFileDialog>
#include <QtConcurrent/QtConcurrent>
#include <QDateTime>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_deviceManager(new DeviceManager(this))
    , m_ltfsManager(new LtfsManager(this))
    , m_indexManager(new IndexManager(this))
    , m_fileBrowser(new FileBrowserWidget(this))
    , m_progressBar(new QProgressBar(this))
    , m_statusLabel(new QLabel(this))
{
    ui->setupUi(this);
    
    // Add File Browser to Tab
    ui->verticalLayout_Browser->addWidget(m_fileBrowser);
    
    // Setup Status Bar
    m_progressBar->setVisible(false);
    m_progressBar->setRange(0, 0); // Indeterminate
    m_progressBar->setMaximumWidth(200);
    ui->statusbar->addPermanentWidget(m_progressBar);
    ui->statusbar->addWidget(m_statusLabel);
    
    connect(m_ltfsManager, &LtfsManager::operationFinished, this, &MainWindow::onLtfsOperationFinished);
    connect(m_ltfsManager, &LtfsManager::outputReceived, this, &MainWindow::onLtfsOutputReceived);
    
    connect(&m_futureWatcher, &QFutureWatcher<bool>::finished, this, &MainWindow::onAsyncOperationFinished);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::logMessage(const QString &message)
{
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
    ui->textLog->append(QString("[%1] %2").arg(timestamp, message));
}

void MainWindow::setBusy(bool busy, const QString &message)
{
    ui->groupBoxOperations->setEnabled(!busy);
    ui->btnScan->setEnabled(!busy);
    m_progressBar->setVisible(busy);
    if (busy) {
        m_statusLabel->setText(message);
        logMessage("Started: " + message);
    } else {
        m_statusLabel->clear();
    }
}

void MainWindow::on_btnScan_clicked()
{
    ui->treeDevices->clear();
    ui->groupBoxOperations->setEnabled(false);
    logMessage("Scanning for devices...");
    
    QList<TapeDeviceInfo> devices = m_deviceManager->scanDevices();
    
    for (const TapeDeviceInfo &device : devices) {
        QTreeWidgetItem *item = new QTreeWidgetItem(ui->treeDevices);
        item->setText(0, device.devicePath);
        item->setText(1, device.vendorId);
        item->setText(2, device.productId);
        item->setText(3, device.serialNumber);
    }
    
    if (devices.isEmpty()) {
        ui->statusbar->showMessage("No devices found.");
        logMessage("No devices found.");
    } else {
        ui->statusbar->showMessage(QString("Found %1 devices.").arg(devices.size()));
        logMessage(QString("Found %1 devices.").arg(devices.size()));
    }
}

void MainWindow::on_treeDevices_itemSelectionChanged()
{
    bool hasSelection = !ui->treeDevices->selectedItems().isEmpty();
    ui->groupBoxOperations->setEnabled(hasSelection);
}

QString MainWindow::getSelectedDevicePath()
{
    auto items = ui->treeDevices->selectedItems();
    if (items.isEmpty()) return QString();
    return items.first()->text(0);
}

void MainWindow::on_btnStatus_clicked()
{
    QString path = getSelectedDevicePath();
    if (path.isEmpty()) return;
    
    m_currentAsyncOperation = "Check Status";
    setBusy(true, "Checking device status...");
    
    QFuture<bool> future = QtConcurrent::run([this, path]() {
        return m_deviceManager->isDeviceReady(path);
    });
    m_futureWatcher.setFuture(future);
}

void MainWindow::on_btnRewind_clicked()
{
    QString path = getSelectedDevicePath();
    if (path.isEmpty()) return;
    
    m_currentAsyncOperation = "Rewind";
    setBusy(true, "Rewinding tape...");
    
    QFuture<bool> future = QtConcurrent::run([this, path]() {
        return m_deviceManager->rewindDevice(path);
    });
    m_futureWatcher.setFuture(future);
}

void MainWindow::on_btnEject_clicked()
{
    QString path = getSelectedDevicePath();
    if (path.isEmpty()) return;
    
    if (QMessageBox::question(this, "Confirm Eject", "Are you sure you want to eject the tape?") != QMessageBox::Yes) {
        return;
    }
    
    m_currentAsyncOperation = "Eject";
    setBusy(true, "Ejecting tape...");
    
    QFuture<bool> future = QtConcurrent::run([this, path]() {
        return m_deviceManager->unloadDevice(path);
    });
    m_futureWatcher.setFuture(future);
}

void MainWindow::onAsyncOperationFinished()
{
    bool result = m_futureWatcher.result();
    setBusy(false);
    
    if (result) {
        logMessage(m_currentAsyncOperation + " completed successfully.");
        QMessageBox::information(this, "Success", m_currentAsyncOperation + " completed successfully.");
    } else {
        logMessage(m_currentAsyncOperation + " failed.");
        QMessageBox::warning(this, "Failed", m_currentAsyncOperation + " failed.");
    }
}

void MainWindow::on_btnFormat_clicked()
{
    QString path = getSelectedDevicePath();
    if (path.isEmpty()) return;
    
    bool ok;
    QString volumeName = QInputDialog::getText(this, "Format Tape", "Enter Volume Name:", QLineEdit::Normal, "TAPE001", &ok);
    if (!ok || volumeName.isEmpty()) return;
    
    if (QMessageBox::warning(this, "Confirm Format", 
                             "WARNING: This will ERASE ALL DATA on the tape.\nAre you sure?", 
                             QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes) {
        return;
    }
    
    setBusy(true, "Formatting tape (mkltfs)...");
    m_ltfsManager->format(path, volumeName);
}

void MainWindow::on_btnMount_clicked()
{
    QString path = getSelectedDevicePath();
    if (path.isEmpty()) return;
    
    // Store Serial
    QTreeWidgetItem *item = ui->treeDevices->currentItem();
    if (item) m_currentMountSerial = item->text(3);
    
    QString mountPoint;
#ifdef Q_OS_WIN
    bool ok;
    mountPoint = QInputDialog::getText(this, "Mount LTFS", "Enter Drive Letter (e.g. X:):", QLineEdit::Normal, "X:", &ok);
    if (!ok || mountPoint.isEmpty()) return;
#else
    mountPoint = QFileDialog::getExistingDirectory(this, "Select Mount Point");
    if (mountPoint.isEmpty()) return;
#endif

    m_currentMountPoint = mountPoint;
    setBusy(true, "Mounting LTFS...");
    m_ltfsManager->mount(path, mountPoint);
}

void MainWindow::on_btnUnmount_clicked()
{
    // For unmount, we ideally need to know where it was mounted.
    // For now, we ask the user to confirm the mount point or drive letter.
    
    QString mountPoint;
#ifdef Q_OS_WIN
    bool ok;
    mountPoint = QInputDialog::getText(this, "Unmount LTFS", "Enter Drive Letter to Unmount (e.g. X:):", QLineEdit::Normal, "X:", &ok);
    if (!ok || mountPoint.isEmpty()) return;
#else
    mountPoint = QFileDialog::getExistingDirectory(this, "Select Mount Point to Unmount");
    if (mountPoint.isEmpty()) return;
#endif

    setBusy(true, "Unmounting LTFS...");
    m_ltfsManager->unmount(mountPoint);
}

void MainWindow::onLtfsOperationFinished(const QString &operation, bool success, const QString &message)
{
    setBusy(false);
    logMessage(QString("%1 finished: %2").arg(operation, message));
    
    if (!success) {
        QMessageBox::critical(this, "Operation Failed", QString("%1 failed.\n%2").arg(operation, message));
    } else {
        QMessageBox::information(this, "Operation Success", QString("%1 completed successfully.").arg(operation));
    }
}

void MainWindow::onLtfsOutputReceived(const QString &text)
{
    ui->textLog->moveCursor(QTextCursor::End);
    ui->textLog->insertPlainText(text);
    ui->textLog->moveCursor(QTextCursor::End);
}

void MainWindow::on_actionSettings_triggered()
{
    SettingsDialog dialog(this);
    dialog.exec();
}

void MainWindow::on_actionExit_triggered()
{
    close();
}

void MainWindow::on_actionAbout_triggered()
{
    AboutDialog dialog(this);
    dialog.exec();
}


void MainWindow::on_tabWidget_currentChanged(int index)
{
    if (index == 1) { // File Browser Tab
        QTreeWidgetItem *item = ui->treeDevices->currentItem();
        if (item) {
            QString serial = item->text(3); // Serial Number column
            if (!serial.isEmpty()) {
                if (m_indexManager->hasIndex(serial)) {
                    LtfsIndex ltfsIndex;
                    if (m_indexManager->loadIndex(serial, ltfsIndex)) {
                        m_fileBrowser->loadIndex(ltfsIndex);
                    }
                } else {
                    m_fileBrowser->clear();
                    QMessageBox::information(this, "No Index", "No index found for this tape (" + serial + ").\nPlease mount the tape to read its index.");
                }
            } else {
                 m_fileBrowser->clear();
            }
        } else {
            m_fileBrowser->clear();
        }
    }
}


