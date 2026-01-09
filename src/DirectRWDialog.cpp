#include "DirectRWDialog.h"
#include "ui_DirectRWDialog.h"

#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QStandardItem>
#include <QTime>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QHeaderView>
#include <QDirIterator>
#include <functional>
#include <utility>

namespace {
static constexpr int ROLE_IS_DIR = Qt::UserRole + 1;
static constexpr int ROLE_SIZE = Qt::UserRole + 2;
static constexpr int ROLE_PATH = Qt::UserRole + 3;
}

DirectRWDialog::DirectRWDialog(const QList<TapeDeviceInfo> &devices, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::DirectRWDialog)
    , m_devices(devices)
{
    ui->setupUi(this);

    m_treeModel = new QStandardItemModel(this);
    m_childrenModel = new QStandardItemModel(this);
    ui->treeTapeCurrent->setModel(m_treeModel);
    ui->treeChildren->setModel(m_childrenModel);
    m_treeModel->setHorizontalHeaderLabels({tr("名称"), tr("大小(字节)")});
    m_childrenModel->setHorizontalHeaderLabels({tr("名称"), tr("大小(字节)")});
    ui->treeTapeCurrent->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui->treeChildren->header()->setSectionResizeMode(QHeaderView::ResizeToContents);

    populateDevices();
    loadCurrentTape();

    connect(ui->treeTapeCurrent->selectionModel(), &QItemSelectionModel::currentChanged,
            this, &DirectRWDialog::on_treeTapeCurrent_clicked);
}

DirectRWDialog::~DirectRWDialog()
{
    delete ui;
}

void DirectRWDialog::populateDevices()
{
    ui->comboDevices->clear();
    for (const TapeDeviceInfo &dev : std::as_const(m_devices)) {
        const QString label = dev.displayName.isEmpty() ? dev.devicePath : dev.displayName;
        ui->comboDevices->addItem(label, dev.devicePath);
    }
}

QString DirectRWDialog::currentDevicePath() const
{
    return ui->comboDevices->currentData().toString();
}

QStandardItem *DirectRWDialog::buildItemFromNode(const TapeNode &node, const QString &currentPath)
{
    auto *nameItem = new QStandardItem(node.name.isEmpty() ? QStringLiteral("/") : node.name);
    nameItem->setData(node.isDir, ROLE_IS_DIR);
    nameItem->setData(node.size, ROLE_SIZE);
    nameItem->setData(currentPath, ROLE_PATH);
    nameItem->setEditable(false);

    auto *sizeItem = new QStandardItem(node.isDir ? QString() : QString::number(node.size));
    sizeItem->setData(node.isDir, ROLE_IS_DIR);
    sizeItem->setData(node.size, ROLE_SIZE);
    sizeItem->setData(currentPath, ROLE_PATH);
    sizeItem->setEditable(false);

    for (const TapeNode &child : node.children) {
        const QString childPath = currentPath == "/" ? "/" + child.name : currentPath + "/" + child.name;
        QStandardItem *childItem = buildItemFromNode(child, childPath);
        auto *childSize = new QStandardItem(child.isDir ? QString() : QString::number(child.size));
        childSize->setData(child.isDir, ROLE_IS_DIR);
        childSize->setData(child.size, ROLE_SIZE);
        childSize->setData(childPath, ROLE_PATH);
        childSize->setEditable(false);
        nameItem->appendRow({childItem, childSize});
    }

    return nameItem;
}

void DirectRWDialog::appendLog(const QString &line)
{
    ui->textLog->append(QStringLiteral("[%1] %2").arg(QTime::currentTime().toString("HH:mm:ss"), line));
}

void DirectRWDialog::loadCurrentTape()
{
    const QString dev = currentDevicePath();
    m_currentTree = m_deviceManager.readTapeIndex(dev);
    rebuildTree();
    appendLog(tr("Loaded tape index from %1").arg(dev.isEmpty() ? tr("(no device)") : dev));
}
QString DirectRWDialog::itemPath(QStandardItem *item) const
{
    if (!item) return QString();
    return item->data(ROLE_PATH).toString();
}

const TapeNode *DirectRWDialog::findNode(const QString &path) const
{
    QString trimmed = path;
    if (trimmed == "/" || trimmed.isEmpty()) return &m_currentTree;
    if (trimmed.startsWith('/')) trimmed = trimmed.mid(1);
    const QStringList parts = trimmed.split('/', Qt::SkipEmptyParts);
    const TapeNode *current = &m_currentTree;
    for (const QString &p : parts) {
        bool ok = false;
        for (const TapeNode &child : current->children) {
            if (child.name == p) {
                current = &child;
                ok = true;
                break;
            }
        }
        if (!ok) return nullptr;
    }
    return current;
}

bool DirectRWDialog::ensureMountValid()
{
    const QString mountPoint = currentDevicePath();
    if (mountPoint.isEmpty()) {
        QMessageBox::warning(this, tr("Warning"), tr("Device/mount path is empty."));
        return false;
    }
    if (!QFileInfo::exists(mountPoint) || !QFileInfo(mountPoint).isDir()) {
        QMessageBox::warning(this, tr("Warning"), tr("Mount path is invalid: %1").arg(mountPoint));
        return false;
    }
    return true;
}

void DirectRWDialog::rebuildTree()
{
    m_treeModel->clear();
    m_treeModel->setHorizontalHeaderLabels({tr("名称"), tr("大小(字节)")});
    QStandardItem *rootItem = buildItemFromNode(m_currentTree, QStringLiteral("/"));
    auto *rootSize = new QStandardItem;
    rootSize->setData(true, ROLE_IS_DIR);
    rootSize->setData(m_currentTree.size, ROLE_SIZE);
    rootSize->setData(QStringLiteral("/"), ROLE_PATH);
    rootSize->setEditable(false);
    m_treeModel->appendRow({rootItem, rootSize});
    ui->treeTapeCurrent->expandAll();

    // Restore selection
    QStandardItem *target = rootItem;
    const QString targetPath = m_selectedPath.isEmpty() ? QStringLiteral("/") : m_selectedPath;
    std::function<QStandardItem*(QStandardItem*, const QString&)> finder = [&](QStandardItem *item, const QString &path) -> QStandardItem* {
        if (!item) return nullptr;
        if (item->data(ROLE_PATH).toString() == path) return item;
        for (int i = 0; i < item->rowCount(); ++i) {
            if (QStandardItem *found = finder(item->child(i, 0), path)) return found;
        }
        return nullptr;
    };
    if (QStandardItem *sel = finder(rootItem, targetPath)) target = sel;
    QModelIndex idx = target->index();
    ui->treeTapeCurrent->setCurrentIndex(idx);
    on_treeTapeCurrent_clicked(idx);
}

void DirectRWDialog::rebuildChildren(const QString &dirPath)
{
    m_childrenModel->clear();
    m_childrenModel->setHorizontalHeaderLabels({tr("名称"), tr("大小(字节)")});
    const TapeNode *node = findNode(dirPath);
    if (!node) return;
    for (const TapeNode &child : node->children) {
        const QString childPath = dirPath == "/" ? "/" + child.name : dirPath + "/" + child.name;
        auto *nameItem = new QStandardItem(child.name);
        nameItem->setData(child.isDir, ROLE_IS_DIR);
        nameItem->setData(child.size, ROLE_SIZE);
        nameItem->setData(childPath, ROLE_PATH);
        nameItem->setEditable(false);

        auto *sizeItem = new QStandardItem(child.isDir ? QString() : QString::number(child.size));
        sizeItem->setData(child.isDir, ROLE_IS_DIR);
        sizeItem->setData(child.size, ROLE_SIZE);
        sizeItem->setData(childPath, ROLE_PATH);
        sizeItem->setEditable(false);

        m_childrenModel->appendRow({nameItem, sizeItem});
    }
}

void DirectRWDialog::on_btnRefresh_clicked()
{
    loadCurrentTape();
}

void DirectRWDialog::on_btnRewind_clicked()
{
    QMessageBox::information(this, tr("Info"), tr("Rewind will be implemented later."));
}

void DirectRWDialog::on_btnLocate_clicked()
{
    QMessageBox::information(this, tr("Info"), tr("Locate/seek will be implemented later."));
}

void DirectRWDialog::on_btnEject_clicked()
{
    QMessageBox::information(this, tr("Info"), tr("Unload/Eject will be implemented later."));
}

void DirectRWDialog::on_btnAddFiles_clicked()
{
    if (!ensureMountValid()) return;
    QString targetPath = m_selectedPath.isEmpty() ? QStringLiteral("/") : m_selectedPath;
    QStringList files = QFileDialog::getOpenFileNames(this, tr("Choose files to add"));
    if (files.isEmpty()) return;
    const QString mountPoint = currentDevicePath();
    int copied = 0;
    for (const QString &file : std::as_const(files)) {
        QFileInfo info(file);
        const QString dest = QDir(mountPoint).filePath(targetPath == "/" ? info.fileName() : targetPath.mid(1) + "/" + info.fileName());
        QDir().mkpath(QFileInfo(dest).absolutePath());
        if (QFile::exists(dest)) QFile::remove(dest);
        if (QFile::copy(file, dest)) {
            ++copied;
            appendLog(tr("Added file %1").arg(dest));
        } else {
            appendLog(tr("Failed to copy %1").arg(file));
        }
    }
    loadCurrentTape();
    appendLog(tr("Queued %1 file(s) copied." ).arg(copied));
}

void DirectRWDialog::on_btnAddFolder_clicked()
{
    if (!ensureMountValid()) return;
    bool ok = false;
    const QString name = QInputDialog::getText(this, tr("New Folder"), tr("Folder name"), QLineEdit::Normal, tr("New Folder"), &ok);
    if (!ok || name.trimmed().isEmpty()) return;
    const QString targetPath = m_selectedPath.isEmpty() ? QStringLiteral("/") : m_selectedPath;
    const QString dest = QDir(currentDevicePath()).filePath(targetPath == "/" ? name.trimmed() : targetPath.mid(1) + "/" + name.trimmed());
    if (QDir().mkpath(dest)) {
        appendLog(tr("Created folder %1").arg(dest));
    } else {
        appendLog(tr("Failed to create folder: %1").arg(dest));
    }
    loadCurrentTape();
}

void DirectRWDialog::on_btnImportDir_clicked()
{
    if (!ensureMountValid()) return;
    QString dir = QFileDialog::getExistingDirectory(this, tr("Choose directory to import"));
    if (dir.isEmpty()) return;
    const QString targetPath = m_selectedPath.isEmpty() ? QStringLiteral("/") : m_selectedPath;
    const QString mountPoint = currentDevicePath();
    QDirIterator it(dir, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        QFileInfo info(it.fileInfo());
        const QString relative = QDir(dir).relativeFilePath(info.filePath());
        const QString dest = QDir(mountPoint).filePath(targetPath == "/" ? relative : targetPath.mid(1) + "/" + relative);
        if (info.isDir()) {
            QDir().mkpath(dest);
        } else {
            QDir().mkpath(QFileInfo(dest).absolutePath());
            if (QFile::exists(dest)) QFile::remove(dest);
            QFile::copy(info.filePath(), dest);
        }
    }
    appendLog(tr("Imported directory %1").arg(QFileInfo(dir).fileName()));
    loadCurrentTape();
}

void DirectRWDialog::on_btnRename_clicked()
{
    if (!ensureMountValid()) return;
    QModelIndex idx = ui->treeChildren->currentIndex();
    if (!idx.isValid()) idx = ui->treeTapeCurrent->currentIndex();
    if (!idx.isValid()) return;
    QStandardItem *item = m_childrenModel->itemFromIndex(idx);
    if (!item) item = m_treeModel->itemFromIndex(idx);
    if (!item) return;
    const QString path = itemPath(item);
    if (path == "/") return;
    bool ok = false;
    const QString newName = QInputDialog::getText(this, tr("Rename"), tr("New name"), QLineEdit::Normal, item->text(), &ok);
    if (!ok || newName.trimmed().isEmpty()) return;
    QFileInfo info(QDir(currentDevicePath()).filePath(path.mid(1)));
    const QString newPath = info.dir().filePath(newName.trimmed());
    if (QFile::rename(info.filePath(), newPath)) {
        appendLog(tr("Renamed to %1").arg(newName));
    } else {
        appendLog(tr("Rename failed for %1").arg(info.filePath()));
    }
    loadCurrentTape();
}

void DirectRWDialog::on_btnDelete_clicked()
{
    if (!ensureMountValid()) return;
    QModelIndex idx = ui->treeChildren->currentIndex();
    if (!idx.isValid()) idx = ui->treeTapeCurrent->currentIndex();
    if (!idx.isValid()) return;
    QStandardItem *item = m_childrenModel->itemFromIndex(idx);
    if (!item) item = m_treeModel->itemFromIndex(idx);
    if (!item) return;
    const QString path = itemPath(item);
    if (path == "/") {
        QMessageBox::warning(this, tr("Warning"), tr("Cannot remove root."));
        return;
    }
    const QString fullPath = QDir(currentDevicePath()).filePath(path.mid(1));
    QFileInfo info(fullPath);
    bool ok = false;
    if (info.isDir()) {
        ok = QDir(fullPath).removeRecursively();
    } else {
        ok = QFile::remove(fullPath);
    }
    if (ok) {
        appendLog(tr("Removed %1").arg(path));
    } else {
        appendLog(tr("Failed to remove %1").arg(path));
    }
    loadCurrentTape();
}
void DirectRWDialog::on_treeTapeCurrent_clicked(const QModelIndex &index)
{
    QStandardItem *item = m_treeModel->itemFromIndex(index);
    if (!item) return;
    m_selectedPath = itemPath(item);
    rebuildChildren(m_selectedPath);
}
