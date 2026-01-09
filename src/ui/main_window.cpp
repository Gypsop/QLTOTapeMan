#include "main_window.h"

#include "ltfs_writer_window.h"
#include "ui_main_window.h"

#include <QDateTime>
#include <QHeaderView>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStringList>
#include <QTableView>
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

    refreshDevices();
    appendLog(tr("Configurator ready."));
}

MainWindow::~MainWindow() = default;

void MainWindow::refreshDevices() {
    populateStubDevices();
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
    appendLog(tr("Action: %1").arg(btn->text()));
}

void MainWindow::exportLogSense() {
    appendLog(tr("Export LogSense page %1").arg(ui->logSensePage->value()));
    ui->debugOutput->appendPlainText(tr("[LogSense] page %1 exported (stub).")
                                         .arg(ui->logSensePage->value()));
}

void MainWindow::sendScsi() {
    const QString cdb = ui->cdbInput->text();
    ui->debugOutput->appendPlainText(tr("[SCSI] Sent CDB: %1 (stub)\nResponse: OK").arg(cdb));
    appendLog(tr("SCSI command sent."));
}

void MainWindow::appendLog(const QString &text) {
    const QString ts = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
    ui->logEdit->appendPlainText(QStringLiteral("[%1] %2").arg(ts, text));
}

void MainWindow::populateStubDevices() {
    deviceModel_.removeRows(0, deviceModel_.rowCount());
    struct Row {
        QString name;
        QString path;
        QString type;
        QString status;
        QString serial;
    };
    const std::vector<Row> rows = {
        {tr("HP Ultrium 7"), QStringLiteral("/dev/st0"), tr("Tape"), tr("Ready"), QStringLiteral("SN123456")},
        {tr("IBM TS1155"), QStringLiteral("/dev/st1"), tr("Tape"), tr("Mounted"), QStringLiteral("SN654321")},
        {tr("Scalar i3"), QStringLiteral("changer0"), tr("Changer"), tr("Idle"), QStringLiteral("CH001122")}};

    for (const auto &r : rows) {
        QList<QStandardItem *> items;
        items << new QStandardItem(r.name);
        items << new QStandardItem(r.path);
        items << new QStandardItem(r.type);
        items << new QStandardItem(r.status);
        items << new QStandardItem(r.serial);
        for (auto *it : items) it->setEditable(false);
        deviceModel_.appendRow(items);
    }
    if (deviceModel_.rowCount() > 0) {
        ui->deviceView->selectRow(0);
        onDeviceSelected();
    }
}

} // namespace qlto
