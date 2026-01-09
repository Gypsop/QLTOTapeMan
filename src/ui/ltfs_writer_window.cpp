#include "ltfs_writer_window.h"

#include "file_browser_dialog.h"
#include "ui_ltfs_writer_window.h"

#include <QDateTime>
#include <QHeaderView>
#include <QLabel>
#include <QProgressBar>
#include <QStandardItem>
#include <QTimer>
#include <QtGlobal>
#include <QMetaObject>
#include <functional>
#include <QFileInfo>

#include "../core/ltfs_service.h"
#include "../io/tape_factory.h"

namespace qlto {

LTFSWriterWindow::LTFSWriterWindow(QWidget *parent)
    : QMainWindow(parent), ui(std::make_unique<Ui::LTFSWriterWindow>()) {
    ui->setupUi(this);

    setupStatusBar();

    treeModel_.setHorizontalHeaderLabels({tr("Name"), tr("Size"), tr("Path")});
    ui->treeView->setModel(&treeModel_);
    ui->treeView->header()->setSectionResizeMode(0, QHeaderView::Stretch);

    fileModel_.setHorizontalHeaderLabels({tr("Name"), tr("Size"), tr("Path"), tr("Selected")});
    ui->fileTable->setModel(&fileModel_);
    ui->fileTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    ui->fileTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    ui->fileTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    ui->fileTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);

    connect(ui->startButton, &QPushButton::clicked, this, &LTFSWriterWindow::startJob);
    connect(ui->pauseButton, &QPushButton::clicked, this, &LTFSWriterWindow::pauseJob);
    connect(ui->resumeButton, &QPushButton::clicked, this, &LTFSWriterWindow::resumeJob);
    connect(ui->stopButton, &QPushButton::clicked, this, &LTFSWriterWindow::stopJob);
    connect(ui->flushButton, &QPushButton::clicked, this, &LTFSWriterWindow::flushJob);
    connect(ui->ejectButton, &QPushButton::clicked, this, &LTFSWriterWindow::ejectTape);
    connect(ui->browseButton, &QPushButton::clicked, this, &LTFSWriterWindow::browseFiles);

    connect(&progressTimer_, &QTimer::timeout, this, &LTFSWriterWindow::updateProgress);
    progressTimer_.setInterval(200);

    connect(&treeModel_, &QStandardItemModel::itemChanged, this, &LTFSWriterWindow::handleTreeChanged);
    connect(ui->treeView->selectionModel(), &QItemSelectionModel::currentChanged, this,
            &LTFSWriterWindow::handleTreeSelectionChanged);

    setStatusLight(tr("Idle"), QStringLiteral("#888"));
    appendLog(tr("LTFSWriter ready."));
}

LTFSWriterWindow::~LTFSWriterWindow() = default;

void LTFSWriterWindow::setupStatusBar() {
    progressBar_ = new QProgressBar(this);
    progressBar_->setRange(0, 100);
    progressBar_->setValue(0);
    statusText_ = new QLabel(tr("Idle"), this);
    speedText_ = new QLabel(tr("0 MB/s"), this);
    ui->statusbar->addPermanentWidget(progressBar_, 1);
    ui->statusbar->addPermanentWidget(statusText_);
    ui->statusbar->addPermanentWidget(speedText_);
}

void LTFSWriterWindow::appendLog(const QString &text) {
    const QString ts = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
    ui->logEdit->appendPlainText(QStringLiteral("[%1] %2").arg(ts, text));
}

void LTFSWriterWindow::ensurePathItem(const QStringList &parts, qint64 sizeBytes) {
    if (parts.isEmpty()) return;
    QStandardItem *parent = treeModel_.invisibleRootItem();
    QString currentPath;
    for (int i = 0; i < parts.size(); ++i) {
        currentPath += "/" + parts[i];
        QStandardItem *match = nullptr;
        for (int r = 0; r < parent->rowCount(); ++r) {
            auto *child = parent->child(r, 0);
            if (child && child->text() == parts[i]) {
                match = child;
                break;
            }
        }
        if (!match) {
            match = new QStandardItem(parts[i]);
            match->setCheckable(true);
            match->setCheckState(Qt::Checked);
            match->setEditable(false);
            match->setData(currentPath, Qt::UserRole + 1);
            auto *sizeItem = new QStandardItem();
            auto *pathItem = new QStandardItem(currentPath);
            parent->appendRow({match, sizeItem, pathItem});
        }
        parent = match;
    }
    if (parent) {
        parent->setCheckState(Qt::Checked);
        const QModelIndex idx = parent->index();
        auto *sizeItem = treeModel_.itemFromIndex(idx.sibling(idx.row(), 1));
        if (sizeItem && sizeBytes >= 0) {
            sizeItem->setText(QString::number(sizeBytes / 1024) + QStringLiteral(" KB"));
        }
    }
}

void LTFSWriterWindow::refreshFileTable(const QModelIndex &dirIndex) {
    fileModel_.removeRows(0, fileModel_.rowCount());
    if (!dirIndex.isValid()) return;
    auto *item = treeModel_.itemFromIndex(dirIndex);
    if (!item) return;
    for (int i = 0; i < item->rowCount(); ++i) {
        auto *child = item->child(i, 0);
        auto *sizeItem = item->child(i, 1);
        auto *pathItem = item->child(i, 2);
        if (!child) continue;
        QList<QStandardItem *> row;
        row << new QStandardItem(child->text());
        row << new QStandardItem(sizeItem ? sizeItem->text() : QString());
        row << new QStandardItem(pathItem ? pathItem->text() : QString());
        auto *selected = new QStandardItem(child->checkState() == Qt::Checked ? tr("Yes") : tr("No"));
        row << selected;
        for (auto *it : row) it->setEditable(false);
        fileModel_.appendRow(row);
    }
}

void LTFSWriterWindow::setStatusLight(const QString &text, const QString &color) {
    ui->statusLight->setText(text);
    ui->statusLight->setStyleSheet(QStringLiteral("QLabel { background:%1; color: white; padding:2px; }").arg(color));
    if (statusText_) statusText_->setText(text);
}

void LTFSWriterWindow::setDrivePath(const QString &path) {
    drivePath_ = path;
    ui->drivePathEdit->setText(path);
}

void LTFSWriterWindow::startJob() {
    std::string err;
    if (!ensureServiceReady(err)) {
        appendLog(QString::fromStdString(err));
        return;
    }

    const auto paths = selectedFilePaths();
    if (paths.empty()) {
        appendLog(tr("No files selected."));
        return;
    }

    std::vector<LtfsFile> files;
    files.reserve(paths.size());
    for (const auto &p : paths) {
        QFileInfo info(p);
        if (!info.exists() || !info.isFile()) continue;
        LtfsFile f{};
        f.name = info.fileName().toStdString();
        f.length = static_cast<std::uint64_t>(info.size());
        LtfsXAttr xa{};
        xa.name = "source_path";
        xa.value = info.absoluteFilePath().toStdString();
        f.extendedattributes.push_back(std::move(xa));
        files.push_back(std::move(f));
    }
    if (files.empty()) {
        appendLog(tr("Selected entries are not valid files."));
        return;
    }

    WriteOptions opts{};
    opts.block_len = static_cast<std::uint32_t>(ui->blockSizeSpin->value()) * 1024;
    opts.hash_on_write = ui->hashCheck->isChecked();
    opts.speed_limit_mib_s = static_cast<std::uint32_t>(ui->speedLimitSpin->value());
    opts.index_interval_bytes = static_cast<std::uint64_t>(ui->indexIntervalSpin->value()) * opts.block_len;
    opts.capacity_interval_sec = static_cast<std::uint32_t>(ui->capacityIntervalSpin->value());
    opts.extra_partition_count = static_cast<std::uint8_t>(ui->extraPartitionSpin->value());
    opts.offline_mode = ui->offlineToggle->isChecked();

    ServiceCallbacks cb{};
    cb.progress = [this](double v) {
        int percent = static_cast<int>(v * 100.0);
        QMetaObject::invokeMethod(progressBar_, [this, percent]() { progressBar_->setValue(percent); }, Qt::QueuedConnection);
    };
    cb.log = [this](const std::string &msg) {
        QMetaObject::invokeMethod(this, [this, msg]() { appendLog(QString::fromStdString(msg)); }, Qt::QueuedConnection);
    };
    cb.status_text = [this](const std::string &msg) {
        QMetaObject::invokeMethod(this, [this, msg]() { appendLog(QString::fromStdString(msg)); }, Qt::QueuedConnection);
    };
    cb.status_light = [this](LWStatus st) {
        QString text;
        QString color = "#888";
        switch (st) {
        case LWStatus::Busy: text = tr("Running"); color = "#2e7d32"; break;
        case LWStatus::Pause: text = tr("Paused"); color = "#f9a825"; break;
        case LWStatus::Err: text = tr("Error"); color = "#c62828"; break;
        case LWStatus::Succ: text = tr("Completed"); color = "#1565c0"; break;
        case LWStatus::Stopped: text = tr("Stopped"); color = "#455a64"; break;
        default: text = tr("Idle"); break;
        }
        QMetaObject::invokeMethod(this, [this, text, color]() { setStatusLight(text, color); }, Qt::QueuedConnection);
    };
    cb.capacity = [this](const CapacityInfo &cap) {
        QMetaObject::invokeMethod(this, [this, cap]() {
            statusText_->setText(tr("Free %1 / Total %2 MB")
                                     .arg(static_cast<double>(cap.bytes_free) / (1024.0 * 1024.0), 0, 'f', 1)
                                     .arg(static_cast<double>(cap.bytes_total) / (1024.0 * 1024.0), 0, 'f', 1));
        });
    };
    service_->set_callbacks(cb);

    progressBar_->setValue(0);
    setStatusLight(tr("Running"), QStringLiteral("#2e7d32"));
    appendLog(tr("Started writing job."));

    std::thread([this, files = std::move(files), opts]() {
        std::string err;
        bool ok = service_->write_files(files, opts, err);
        if (!ok) {
            QMetaObject::invokeMethod(this, [this, err]() {
                appendLog(QStringLiteral("Write failed: %1").arg(QString::fromStdString(err)));
                setStatusLight(tr("Error"), QStringLiteral("#c62828"));
            }, Qt::QueuedConnection);
        } else {
            QMetaObject::invokeMethod(this, [this]() { setStatusLight(tr("Completed"), QStringLiteral("#1565c0")); }, Qt::QueuedConnection);
        }
    }).detach();
}

void LTFSWriterWindow::pauseJob() {
    appendLog(tr("Pause is not supported for the current operation."));
}

void LTFSWriterWindow::resumeJob() {
    appendLog(tr("Resume is not supported for the current operation."));
}

void LTFSWriterWindow::stopJob() {
    progressTimer_.stop();
    progressValue_ = 0;
    progressBar_->setValue(progressValue_);
    speedText_->setText(tr("0 MB/s"));
    setStatusLight(tr("Stopped"), QStringLiteral("#c62828"));
    appendLog(tr("Stop requested. Current write cannot be interrupted."));
}

void LTFSWriterWindow::flushJob() {
    std::string err;
    if (!ensureServiceReady(err)) {
        appendLog(QString::fromStdString(err));
        return;
    }
    if (!service_->flush(err)) {
        appendLog(QStringLiteral("Flush failed: %1").arg(QString::fromStdString(err)));
        setStatusLight(tr("Error"), QStringLiteral("#c62828"));
        return;
    }
    appendLog(tr("Flush complete."));
}

void LTFSWriterWindow::ejectTape() {
    std::string err;
    if (!ensureServiceReady(err)) {
        appendLog(QString::fromStdString(err));
        return;
    }
    if (!service_->eject(err)) {
        appendLog(QStringLiteral("Eject failed: %1").arg(QString::fromStdString(err)));
        setStatusLight(tr("Error"), QStringLiteral("#c62828"));
        return;
    }
    appendLog(tr("Tape unloaded."));
    setStatusLight(tr("Ejected"), QStringLiteral("#455a64"));
}

void LTFSWriterWindow::browseFiles() {
    FileBrowserDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
        const QStringList paths = dialog.selectedPaths();
        if (!paths.isEmpty()) {
            appendLog(tr("Selected files:\n%1").arg(paths.join('\n')));
            for (const auto &p : paths) {
                QFileInfo info(p);
                QStringList parts = info.filePath().split('/', Qt::SkipEmptyParts);
                ensurePathItem(parts, info.size());
            }
            ui->treeView->expandAll();
            refreshFileTable(ui->treeView->currentIndex());
        }
    }
}

void LTFSWriterWindow::updateProgress() {
    // progress updates are driven by service callbacks; timer unused
}

void LTFSWriterWindow::handleTreeChanged(QStandardItem *item) {
    if (!item) return;
    const Qt::CheckState state = item->checkState();
    setCheckStateRecursive(item, state);
    updateParentState(item->parent());
    refreshFileTable(ui->treeView->currentIndex());
}

void LTFSWriterWindow::handleTreeSelectionChanged(const QModelIndex &index) { refreshFileTable(index); }

void LTFSWriterWindow::setCheckStateRecursive(QStandardItem *item, Qt::CheckState state) {
    if (!item) return;
    for (int i = 0; i < item->rowCount(); ++i) {
        auto *child = item->child(i, 0);
        if (!child) continue;
        child->setCheckState(state);
        setCheckStateRecursive(child, state);
    }
}

void LTFSWriterWindow::updateParentState(QStandardItem *item) {
    if (!item) return;
    int checked = 0;
    int unchecked = 0;
    for (int i = 0; i < item->rowCount(); ++i) {
        auto *child = item->child(i, 0);
        if (!child) continue;
        if (child->checkState() == Qt::Checked)
            ++checked;
        else if (child->checkState() == Qt::Unchecked)
            ++unchecked;
    }
    if (checked == item->rowCount()) {
        item->setCheckState(Qt::Checked);
    } else if (unchecked == item->rowCount()) {
        item->setCheckState(Qt::Unchecked);
    } else {
        item->setCheckState(Qt::PartiallyChecked);
    }
    updateParentState(item->parent());
}

std::vector<QString> LTFSWriterWindow::selectedFilePaths() const {
    std::vector<QString> paths;
    std::function<void(QStandardItem *)> walk = [&](QStandardItem *item) {
        if (!item) return;
        if (item->rowCount() == 0 && item->checkState() == Qt::Checked) {
            paths.push_back(item->data(Qt::UserRole + 1).toString());
        }
        for (int i = 0; i < item->rowCount(); ++i) {
            walk(item->child(i, 0));
        }
    };
    walk(treeModel_.invisibleRootItem());
    return paths;
}

bool LTFSWriterWindow::ensureServiceReady(std::string &err) {
    if (!service_) service_ = make_ltfs_service();
    QString path = drivePath_;
    if (path.isEmpty()) path = ui->drivePathEdit->text();
    if (path.isEmpty()) {
        err = "Drive path is not set";
        return false;
    }

    auto dev = make_default_device(err);
    if (!dev) return false;
    if (!dev->open(path.toStdString(), err)) return false;
    if (!service_->attach_device(std::move(dev), err)) return false;

    std::string tmp_err;
    LtfsLabel label{};
    service_->load_label(label, tmp_err); // best effort
    LtfsIndex index{};
    service_->load_index(index, ui->offlineToggle->isChecked(), tmp_err); // best effort
    err.clear();
    return true;
}

} // namespace qlto
