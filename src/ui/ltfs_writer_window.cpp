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

    populateStubTree();
    if (treeModel_.rowCount() > 0) {
        ui->treeView->expandAll();
        ui->treeView->setCurrentIndex(treeModel_.index(0, 0));
        refreshFileTable(treeModel_.index(0, 0));
    }

    setStatusLight(tr("Idle"), QStringLiteral("#888"));
    appendLog(tr("LTFSWriter ready (stub mode)."));
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

void LTFSWriterWindow::populateStubTree() {
    treeModel_.removeRows(0, treeModel_.rowCount());

    auto addFile = [](QStandardItem *parent, const QString &name, qint64 size, const QString &path) {
        auto *nameItem = new QStandardItem(name);
        nameItem->setCheckable(true);
        nameItem->setCheckState(Qt::Unchecked);
        nameItem->setEditable(false);
        nameItem->setData(path, Qt::UserRole + 1);

        auto *sizeItem = new QStandardItem(QString::number(size / 1024) + QStringLiteral(" KB"));
        sizeItem->setEditable(false);
        auto *pathItem = new QStandardItem(path);
        pathItem->setEditable(false);
        parent->appendRow({nameItem, sizeItem, pathItem});
    };

    auto *root = new QStandardItem(tr("Volume"));
    root->setCheckable(true);
    root->setCheckState(Qt::Unchecked);
    root->setEditable(false);
    root->setData(QStringLiteral("/"), Qt::UserRole + 1);
    auto *rootSize = new QStandardItem(QStringLiteral(""));
    auto *rootPath = new QStandardItem(QStringLiteral("/"));
    treeModel_.appendRow({root, rootSize, rootPath});

    auto *projA = new QStandardItem(QStringLiteral("ProjectA"));
    projA->setCheckable(true);
    projA->setCheckState(Qt::Unchecked);
    projA->setEditable(false);
    projA->setData(QStringLiteral("/ProjectA"), Qt::UserRole + 1);
    root->appendRow({projA, new QStandardItem(QStringLiteral("")), new QStandardItem(QStringLiteral("/ProjectA"))});
    addFile(projA, QStringLiteral("scene1.mov"), 5'000'000, QStringLiteral("/ProjectA/scene1.mov"));
    addFile(projA, QStringLiteral("scene2.mov"), 3'200'000, QStringLiteral("/ProjectA/scene2.mov"));

    auto *projB = new QStandardItem(QStringLiteral("ProjectB"));
    projB->setCheckable(true);
    projB->setCheckState(Qt::Unchecked);
    projB->setEditable(false);
    projB->setData(QStringLiteral("/ProjectB"), Qt::UserRole + 1);
    root->appendRow({projB, new QStandardItem(QStringLiteral("")), new QStandardItem(QStringLiteral("/ProjectB"))});
    addFile(projB, QStringLiteral("report.pdf"), 120'000, QStringLiteral("/ProjectB/report.pdf"));
    addFile(projB, QStringLiteral("data.bin"), 800'000, QStringLiteral("/ProjectB/data.bin"));

    ui->treeView->expand(root->index());
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

void LTFSWriterWindow::startJob() {
    progressValue_ = 0;
    paused_ = false;
    progressBar_->setValue(progressValue_);
    progressTimer_.start();
    setStatusLight(tr("Running"), QStringLiteral("#2e7d32"));
    appendLog(tr("Started writing job."));
}

void LTFSWriterWindow::pauseJob() {
    paused_ = true;
    setStatusLight(tr("Paused"), QStringLiteral("#f9a825"));
    appendLog(tr("Paused."));
}

void LTFSWriterWindow::resumeJob() {
    if (!progressTimer_.isActive()) progressTimer_.start();
    paused_ = false;
    setStatusLight(tr("Running"), QStringLiteral("#2e7d32"));
    appendLog(tr("Resumed."));
}

void LTFSWriterWindow::stopJob() {
    progressTimer_.stop();
    progressValue_ = 0;
    progressBar_->setValue(progressValue_);
    speedText_->setText(tr("0 MB/s"));
    setStatusLight(tr("Stopped"), QStringLiteral("#c62828"));
    appendLog(tr("Stopped."));
}

void LTFSWriterWindow::flushJob() { appendLog(tr("Flush requested.")); }

void LTFSWriterWindow::ejectTape() {
    appendLog(tr("Eject requested."));
    setStatusLight(tr("Ejected"), QStringLiteral("#455a64"));
}

void LTFSWriterWindow::browseFiles() {
    FileBrowserDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
        const QStringList paths = dialog.selectedPaths();
        if (!paths.isEmpty()) {
            appendLog(tr("Selected files:\n%1").arg(paths.join('\n')));
            // mark selected in tree if paths match
            for (int r = 0; r < treeModel_.rowCount(); ++r) {
                auto *root = treeModel_.item(r, 0);
                if (!root) continue;
                QList<QStandardItem *> stack{root};
                while (!stack.isEmpty()) {
                    auto *node = stack.takeFirst();
                    const QString nodePath = node->data(Qt::UserRole + 1).toString();
                    if (paths.contains(nodePath)) {
                        node->setCheckState(Qt::Checked);
                    }
                    for (int i = 0; i < node->rowCount(); ++i) {
                        if (auto *child = node->child(i, 0)) stack.append(child);
                    }
                }
            }
            refreshFileTable(ui->treeView->currentIndex());
        }
    }
}

void LTFSWriterWindow::updateProgress() {
    if (paused_) return;
    progressValue_ = qMin(100, progressValue_ + 2);
    progressBar_->setValue(progressValue_);
    speedText_->setText(tr("%1 MB/s").arg(ui->speedLimitSpin->value() == 0 ? 280 : ui->speedLimitSpin->value()));
    if (progressValue_ >= 100) {
        progressTimer_.stop();
        setStatusLight(tr("Completed"), QStringLiteral("#1565c0"));
        appendLog(tr("Job completed (stub)."));
    }
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

} // namespace qlto
