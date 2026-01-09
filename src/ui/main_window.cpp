#include "main_window.h"

#include "ltfs_writer_window.h"
#include "ui_main_window.h"

#include "../io/tape_factory.h"
#include "../io/tape_enumerator.h"
#include "../io/tape_device.h"

#include <QDateTime>
#include <QHeaderView>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStringList>
#include <QTableView>
#include <sstream>
#include <vector>

namespace qlto {

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(std::make_unique<Ui::MainWindow>()) {
    ui->setupUi(this);

    deviceModel_.setHorizontalHeaderLabels({tr("Name"), tr("Path"), tr("Type"), tr("Status"), tr("Serial")});
    ui->deviceView->setModel(&deviceModel_);
    ui->deviceView->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    connect(ui->refreshButton, &QPushButton::clicked, this, &MainWindow::refreshDevices);
    connect(ui->toggleDebugButton, &QPushButton::clicked, this, &MainWindow::toggleDebugPanel);
    connect(ui->openWriterButton, &QPushButton::clicked, this, &MainWindow::openWriter);
    connect(ui->enterWriterButton, &QPushButton::clicked, this, &MainWindow::openWriter);
    connect(ui->deviceView->selectionModel(), &QItemSelectionModel::currentChanged, this, [this](const QModelIndex &, const QModelIndex &) {
        onDeviceSelected();
    });

    const QList<QPushButton *> actionButtons = {ui->loadButton, ui->unloadButton, ui->mountButton, ui->unmountButton,
                                                ui->mapDriveButton, ui->remountButton, ui->cleanButton};
    for (auto *btn : actionButtons) {
        connect(btn, &QPushButton::clicked, this, &MainWindow::onActionTriggered);
    }

    connect(ui->logSenseButton, &QPushButton::clicked, this, &MainWindow::exportLogSense);
    connect(ui->sendCdbButton, &QPushButton::clicked, this, &MainWindow::sendScsi);

    std::string err;
    enumerator_ = make_default_enumerator(err);
    if (!enumerator_) {
        appendLog(tr("Enumerator unavailable: %1").arg(QString::fromStdString(err)));
    }

    refreshDevices();
    appendLog(tr("Configurator ready."));
}

MainWindow::~MainWindow() = default;

void MainWindow::refreshDevices() {
    populateDevices();
    appendLog(tr("Device list refreshed."));
}

void MainWindow::toggleDebugPanel() {
    const bool visible = !ui->debugGroup->isVisible();
    ui->debugGroup->setVisible(visible);
}

void MainWindow::openWriter() {
    if (!writerWindow_) {
        writerWindow_ = std::make_unique<LTFSWriterWindow>(this);
    }
    const QModelIndex idx = ui->deviceView->currentIndex();
    if (idx.isValid()) {
        const QString path = deviceModel_.data(deviceModel_.index(idx.row(), 1)).toString();
        writerWindow_->setDrivePath(path);
    }
    writerWindow_->show();
    writerWindow_->raise();
    writerWindow_->activateWindow();
    appendLog(tr("Opened LTFSWriter window."));
}

void MainWindow::onDeviceSelected() {
    const QModelIndex idx = ui->deviceView->currentIndex();
    if (!idx.isValid()) return;
    const QString name = deviceModel_.data(deviceModel_.index(idx.row(), 0)).toString();
    const QString status = deviceModel_.data(deviceModel_.index(idx.row(), 3)).toString();
    ui->statusLabel->setText(tr("Status: %1 (%2)").arg(name, status));
}

void MainWindow::onActionTriggered() {
    auto *btn = qobject_cast<QPushButton *>(sender());
    if (!btn) return;
    openCurrentDevice();
    if (!device_) {
        appendLog(tr("No device open."));
        return;
    }

    std::string err;
    bool ok = false;
    if (btn == ui->loadButton) {
        ok = device_->load(false, sense_, err);
    } else if (btn == ui->unloadButton) {
        ok = device_->unload(sense_, err);
    } else if (btn == ui->logSenseButton) {
        LogPage page{};
        ok = device_->log_sense(static_cast<std::uint8_t>(ui->logSensePage->value()), 0, page, sense_, err);
        if (ok) {
            appendLog(tr("LogSense page %1 size %2").arg(ui->logSensePage->value()).arg(page.data.size()));
        }
    } else if (btn == ui->sendCdbButton) {
        // handled separately
        return;
    } else {
        appendLog(tr("Action %1 not supported yet.").arg(btn->text()));
        return;
    }
    if (!ok) {
        appendLog(tr("Action %1 failed: %2").arg(btn->text()).arg(QString::fromStdString(err)));
    } else {
        appendLog(tr("Action %1 succeeded.").arg(btn->text()));
    }
}

void MainWindow::exportLogSense() {
    openCurrentDevice();
    if (!device_) {
        appendLog(tr("No device open."));
        return;
    }
    std::string err;
    LogPage page{};
    if (device_->log_sense(static_cast<std::uint8_t>(ui->logSensePage->value()), 0, page, sense_, err)) {
        ui->debugOutput->appendPlainText(tr("[LogSense] page %1 length %2").arg(ui->logSensePage->value()).arg(page.data.size()));
    } else {
        appendLog(tr("LogSense failed: %1").arg(QString::fromStdString(err)));
    }
}

void MainWindow::sendScsi() {
    const QString cdb = ui->cdbInput->text();
    openCurrentDevice();
    if (!device_) {
        appendLog(tr("No device open."));
        return;
    }
    std::vector<std::uint8_t> cdbBytes;
    std::istringstream iss(cdb.toStdString());
    std::string byteStr;
    while (iss >> byteStr) {
        std::uint32_t val = 0;
        std::stringstream ss;
        ss << std::hex << byteStr;
        ss >> val;
        cdbBytes.push_back(static_cast<std::uint8_t>(val & 0xFF));
    }
    std::vector<std::uint8_t> data(0);
    std::string err;
    if (device_->scsi_pass_through(cdbBytes, data, true, 10000, sense_, err)) {
        ui->debugOutput->appendPlainText(tr("[SCSI] OK, %1 bytes in").arg(data.size()));
    } else {
        ui->debugOutput->appendPlainText(tr("[SCSI] Failed: %1").arg(QString::fromStdString(err)));
    }
    appendLog(tr("SCSI command sent."));
}

void MainWindow::appendLog(const QString &text) {
    const QString ts = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
    ui->logEdit->appendPlainText(QStringLiteral("[%1] %2").arg(ts, text));
}

void MainWindow::populateDevices() {
    deviceModel_.removeRows(0, deviceModel_.rowCount());
    if (!enumerator_) return;
    std::string err;
    const auto devices = enumerator_->list(err);
    if (!err.empty()) {
        appendLog(tr("Enumerate error: %1").arg(QString::fromStdString(err)));
    }
    for (const auto &dev : devices) {
        QList<QStandardItem *> items;
        items << new QStandardItem(QString::fromStdString(dev.display_name));
        items << new QStandardItem(QString::fromStdString(dev.device_path));
        items << new QStandardItem(QString::fromStdString(dev.vendor));
        items << new QStandardItem(tr("Unknown"));
        items << new QStandardItem(QStringLiteral("-"));
        for (auto *it : items) it->setEditable(false);
        deviceModel_.appendRow(items);
    }
    if (deviceModel_.rowCount() > 0) {
        ui->deviceView->selectRow(0);
        onDeviceSelected();
    }
}

void MainWindow::openCurrentDevice() {
    const QModelIndex idx = ui->deviceView->currentIndex();
    if (!idx.isValid()) return;
    const QString path = deviceModel_.data(deviceModel_.index(idx.row(), 1)).toString();
    if (device_ && path == openedPath_) return;
    device_.reset();
    std::string err;
    device_ = make_default_device(err);
    if (!device_) {
        appendLog(tr("Create device failed: %1").arg(QString::fromStdString(err)));
        return;
    }
    if (!device_->open(path.toStdString(), err)) {
        appendLog(tr("Open device failed: %1").arg(QString::fromStdString(err)));
        device_.reset();
    } else {
        appendLog(tr("Opened device %1").arg(path));
        openedPath_ = path;
    }
}

} // namespace qlto
