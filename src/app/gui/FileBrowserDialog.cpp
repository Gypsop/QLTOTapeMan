/*
 * QLTOTapeMan - Qt-based LTO Tape Manager
 *
 * Copyright (c) 2026 Jeffrey ZHU (zhxsh1225@gmail.com)
 * https://github.com/Gypsop/QLTOTapeMan
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "FileBrowserDialog.h"

#include <QSettings>
#include <QHeaderView>
#include <QCloseEvent>
#include <QFileIconProvider>

namespace qltfs {
namespace app {

// ============================================================================
// TreeWidgetEx Implementation
// ============================================================================

TreeWidgetEx::TreeWidgetEx(QWidget *parent)
    : QTreeWidget(parent)
{
}

void TreeWidgetEx::setItemCheckState(QTreeWidgetItem *item, Qt::CheckState state)
{
    if (item) {
        item->setCheckState(0, state);
    }
}

Qt::CheckState TreeWidgetEx::itemCheckState(QTreeWidgetItem *item) const
{
    if (item) {
        return item->checkState(0);
    }
    return Qt::Unchecked;
}

// ============================================================================
// FileBrowserDialog Implementation
// ============================================================================

FileBrowserDialog::FileBrowserDialog(QWidget *parent)
    : QDialog(parent)
    , m_treeWidget(nullptr)
    , m_okButton(nullptr)
    , m_cancelButton(nullptr)
    , m_copyInfoCheckBox(nullptr)
    , m_contextMenu(nullptr)
    , m_selectAllAction(nullptr)
    , m_selectBySizeAction(nullptr)
    , m_selectByRegexAction(nullptr)
    , m_index(nullptr)
    , m_isUpdating(false)
{
    setupUi();
    createContextMenu();
    loadSettings();
}

void FileBrowserDialog::setupUi()
{
    setWindowTitle(tr("File Browser"));
    setMinimumSize(600, 400);
    resize(800, 600);

    // Main layout
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(10);

    // Tree widget
    m_treeWidget = new TreeWidgetEx(this);
    m_treeWidget->setColumnCount(1);
    m_treeWidget->setHeaderLabel(tr("Files and Folders"));
    m_treeWidget->setAlternatingRowColors(true);
    m_treeWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    m_treeWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    m_treeWidget->header()->setStretchLastSection(true);
    
    // Connect signals
    connect(m_treeWidget, &QTreeWidget::currentItemChanged,
            this, &FileBrowserDialog::onItemSelectionChanged);
    connect(m_treeWidget, &QTreeWidget::itemChanged,
            this, &FileBrowserDialog::onItemChanged);
    connect(m_treeWidget, &QTreeWidget::customContextMenuRequested,
            this, &FileBrowserDialog::showContextMenu);

    mainLayout->addWidget(m_treeWidget);

    // Bottom layout with checkbox and buttons
    QHBoxLayout *bottomLayout = new QHBoxLayout();
    bottomLayout->setSpacing(10);

    m_copyInfoCheckBox = new QCheckBox(tr("Copy info to clipboard on selection"), this);
    m_copyInfoCheckBox->setChecked(true);
    bottomLayout->addWidget(m_copyInfoCheckBox);

    bottomLayout->addStretch();

    m_okButton = new QPushButton(tr("OK"), this);
    m_okButton->setDefault(true);
    connect(m_okButton, &QPushButton::clicked, this, &FileBrowserDialog::onOkClicked);
    bottomLayout->addWidget(m_okButton);

    m_cancelButton = new QPushButton(tr("Cancel"), this);
    connect(m_cancelButton, &QPushButton::clicked, this, &FileBrowserDialog::onCancelClicked);
    bottomLayout->addWidget(m_cancelButton);

    mainLayout->addLayout(bottomLayout);

    setLayout(mainLayout);
}

void FileBrowserDialog::createContextMenu()
{
    m_contextMenu = new QMenu(this);

    m_selectAllAction = new QAction(tr("Select All"), this);
    connect(m_selectAllAction, &QAction::triggered, this, &FileBrowserDialog::onSelectAll);
    m_contextMenu->addAction(m_selectAllAction);

    m_selectBySizeAction = new QAction(tr("Select by Size..."), this);
    connect(m_selectBySizeAction, &QAction::triggered, this, &FileBrowserDialog::onSelectBySize);
    m_contextMenu->addAction(m_selectBySizeAction);

    m_selectByRegexAction = new QAction(tr("Select by Filename Pattern..."), this);
    connect(m_selectByRegexAction, &QAction::triggered, this, &FileBrowserDialog::onSelectByRegex);
    m_contextMenu->addAction(m_selectByRegexAction);
}

void FileBrowserDialog::loadSettings()
{
    QSettings settings;
    settings.beginGroup("FileBrowser");
    m_copyInfoCheckBox->setChecked(settings.value("CopyInfo", true).toBool());
    settings.endGroup();
}

void FileBrowserDialog::saveSettings()
{
    QSettings settings;
    settings.beginGroup("FileBrowser");
    settings.setValue("CopyInfo", m_copyInfoCheckBox->isChecked());
    settings.endGroup();
}

void FileBrowserDialog::setIndex(core::LtfsIndex *index)
{
    m_index = index;
    populateTree();
}

int FileBrowserDialog::showDialog(core::LtfsIndex *index, QWidget *parent)
{
    FileBrowserDialog dialog(parent);
    dialog.setIndex(index);
    return dialog.exec();
}

void FileBrowserDialog::showBrowser(core::LtfsIndex *index, QWidget *parent)
{
    FileBrowserDialog *dialog = new FileBrowserDialog(parent);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setIndex(index);
    dialog->show();
}

void FileBrowserDialog::populateTree()
{
    QMutexLocker locker(&m_eventLock);
    m_isUpdating = true;

    m_treeWidget->clear();

    if (!m_index) {
        m_isUpdating = false;
        return;
    }

    // Get root contents from index
    const core::LtfsIndex *idx = m_index;
    
    // Add root directories and files
    addContents(nullptr, idx->directories(), idx->files());

    // Refresh check states for all top-level items
    for (int i = 0; i < m_treeWidget->topLevelItemCount(); ++i) {
        refreshCheckState(m_treeWidget->topLevelItem(i));
    }

    m_isUpdating = false;
}

void FileBrowserDialog::addContents(QTreeWidgetItem *parent,
                                    const QList<core::LtfsDirectoryEntry> &directories,
                                    const QList<core::LtfsFileEntry> &files)
{
    QFileIconProvider iconProvider;

    // Add directories
    for (int i = 0; i < directories.size(); ++i) {
        const core::LtfsDirectoryEntry &dir = directories[i];

        QTreeWidgetItem *item;
        if (parent) {
            item = new QTreeWidgetItem(parent);
        } else {
            item = new QTreeWidgetItem(m_treeWidget);
        }

        item->setText(0, dir.name);
        item->setIcon(0, iconProvider.icon(QFileIconProvider::Folder));
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(0, dir.selected ? Qt::Checked : Qt::Unchecked);

        // Store metadata
        item->setData(0, ItemTypeRole, QVariant::fromValue(static_cast<int>(ItemType::Directory)));
        item->setData(0, DirectoryIndexRole, i);
        item->setData(0, PathRole, dir.name);

        // Recursively add subdirectories and files
        addContents(item, dir.subdirectories, dir.files);
    }

    // Add files
    for (int i = 0; i < files.size(); ++i) {
        const core::LtfsFileEntry &file = files[i];

        QTreeWidgetItem *item;
        if (parent) {
            item = new QTreeWidgetItem(parent);
        } else {
            item = new QTreeWidgetItem(m_treeWidget);
        }

        item->setText(0, file.name);
        item->setIcon(0, iconProvider.icon(QFileIconProvider::File));
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(0, file.selected ? Qt::Checked : Qt::Unchecked);

        // Store metadata
        item->setData(0, ItemTypeRole, QVariant::fromValue(static_cast<int>(ItemType::File)));
        item->setData(0, FileIndexRole, i);
        item->setData(0, PathRole, file.name);
    }
}

void FileBrowserDialog::onItemSelectionChanged(QTreeWidgetItem *current, QTreeWidgetItem * /*previous*/)
{
    if (m_isUpdating) {
        return;
    }

    QMutexLocker locker(&m_eventLock);
    m_isUpdating = true;

    if (current) {
        int itemType = current->data(0, ItemTypeRole).toInt();

        if (itemType == static_cast<int>(ItemType::File)) {
            // File selected
            QString fileName = current->text(0);
            setWindowTitle(tr("File: %1").arg(fileName));

            if (m_copyInfoCheckBox->isChecked()) {
                QString clipText = QString("File\t%1\n").arg(fileName);
                QGuiApplication::clipboard()->setText(clipText);
            }
        } else if (itemType == static_cast<int>(ItemType::Directory)) {
            // Directory selected
            QString dirName = current->text(0);
            int dirCount = 0;
            int fileCount = 0;

            // Count children
            for (int i = 0; i < current->childCount(); ++i) {
                QTreeWidgetItem *child = current->child(i);
                int childType = child->data(0, ItemTypeRole).toInt();
                if (childType == static_cast<int>(ItemType::Directory)) {
                    dirCount++;
                } else {
                    fileCount++;
                }
            }

            setWindowTitle(tr("Directory: %1 (DirCount=%2 FileCount=%3)")
                          .arg(dirName).arg(dirCount).arg(fileCount));

            if (m_copyInfoCheckBox->isChecked()) {
                QString clipText;
                for (int i = 0; i < current->childCount(); ++i) {
                    QTreeWidgetItem *child = current->child(i);
                    int childType = child->data(0, ItemTypeRole).toInt();
                    QString typeName = (childType == static_cast<int>(ItemType::Directory))
                                       ? "Directory" : "File";
                    clipText += QString("%1\t%2\n").arg(typeName).arg(child->text(0));
                }
                QGuiApplication::clipboard()->setText(clipText);
            }
        }
    }

    m_isUpdating = false;
}

void FileBrowserDialog::onItemChanged(QTreeWidgetItem *item, int column)
{
    if (m_isUpdating || column != 0) {
        return;
    }

    QMutexLocker locker(&m_eventLock);
    m_isUpdating = true;

    // If this item has children, recursively set their check state
    if (item->childCount() > 0) {
        bool checked = (item->checkState(0) == Qt::Checked);
        recursivelySetCheckState(item, checked);
    }

    // Update check states for all top-level items
    for (int i = 0; i < m_treeWidget->topLevelItemCount(); ++i) {
        refreshCheckState(m_treeWidget->topLevelItem(i));
    }

    m_isUpdating = false;
}

void FileBrowserDialog::recursivelySetCheckState(QTreeWidgetItem *item, bool checked)
{
    item->setCheckState(0, checked ? Qt::Checked : Qt::Unchecked);

    for (int i = 0; i < item->childCount(); ++i) {
        recursivelySetCheckState(item->child(i), checked);
    }
}

Qt::CheckState FileBrowserDialog::refreshCheckState(QTreeWidgetItem *item)
{
    if (!item) {
        return Qt::Unchecked;
    }

    // Leaf node (no children)
    if (item->childCount() == 0) {
        bool checked = (item->checkState(0) == Qt::Checked);
        updateIndexSelection(item, checked);
        return checked ? Qt::Checked : Qt::Unchecked;
    }

    // Node with children - check child states
    int checkedCount = 0;
    int uncheckedCount = 0;

    for (int i = 0; i < item->childCount(); ++i) {
        Qt::CheckState childState = refreshCheckState(item->child(i));
        switch (childState) {
        case Qt::Checked:
            checkedCount++;
            break;
        case Qt::Unchecked:
            uncheckedCount++;
            break;
        case Qt::PartiallyChecked:
            checkedCount++;
            uncheckedCount++;
            break;
        }
    }

    Qt::CheckState result;
    if (checkedCount > 0 && uncheckedCount == 0) {
        // All children checked
        updateIndexSelection(item, true);
        result = Qt::Checked;
    } else if (checkedCount == 0 && uncheckedCount > 0) {
        // All children unchecked
        updateIndexSelection(item, false);
        result = Qt::Unchecked;
    } else if (checkedCount > 0 && uncheckedCount > 0) {
        // Mixed state
        updateIndexSelection(item, true);
        result = Qt::PartiallyChecked;
    } else {
        // No children processed, use current state
        bool checked = (item->checkState(0) == Qt::Checked);
        updateIndexSelection(item, checked);
        result = checked ? Qt::Checked : Qt::Unchecked;
    }

    // Set the visual state
    item->setCheckState(0, result);

    return result;
}

void FileBrowserDialog::updateIndexSelection(QTreeWidgetItem *item, bool selected)
{
    if (!item || !m_index) {
        return;
    }

    // In a real implementation, we would update the underlying LtfsIndex data
    // This requires traversing the index structure to find the corresponding entry
    // For now, we just store the selection state in the item
    
    // Note: The full implementation would need to track the path to each item
    // and update the corresponding entry in m_index
    Q_UNUSED(selected)
}

QList<QTreeWidgetItem *> FileBrowserDialog::getAllLeafNodes() const
{
    QList<QTreeWidgetItem *> result;

    for (int i = 0; i < m_treeWidget->topLevelItemCount(); ++i) {
        QTreeWidgetItem *item = m_treeWidget->topLevelItem(i);
        if (item->childCount() == 0) {
            result.append(item);
        } else {
            collectLeafNodes(item, result);
        }
    }

    return result;
}

void FileBrowserDialog::collectLeafNodes(QTreeWidgetItem *item, QList<QTreeWidgetItem *> &result) const
{
    if (item->childCount() == 0) {
        result.append(item);
    } else {
        for (int i = 0; i < item->childCount(); ++i) {
            collectLeafNodes(item->child(i), result);
        }
    }
}

void FileBrowserDialog::onOkClicked()
{
    saveSettings();
    accept();
}

void FileBrowserDialog::onCancelClicked()
{
    saveSettings();
    reject();
}

void FileBrowserDialog::closeEvent(QCloseEvent *event)
{
    saveSettings();
    QDialog::closeEvent(event);
}

void FileBrowserDialog::showContextMenu(const QPoint &pos)
{
    m_contextMenu->exec(m_treeWidget->mapToGlobal(pos));
}

void FileBrowserDialog::onSelectAll()
{
    QMutexLocker locker(&m_eventLock);
    m_isUpdating = true;

    for (int i = 0; i < m_treeWidget->topLevelItemCount(); ++i) {
        QTreeWidgetItem *item = m_treeWidget->topLevelItem(i);
        recursivelySetCheckState(item, true);
    }

    // Refresh states
    for (int i = 0; i < m_treeWidget->topLevelItemCount(); ++i) {
        refreshCheckState(m_treeWidget->topLevelItem(i));
    }

    m_isUpdating = false;
}

void FileBrowserDialog::onSelectBySize()
{
    bool ok;
    
    // Get minimum size
    qint64 minSize = QInputDialog::getInt(this, tr("By Size"), 
                                           tr("Minimum Bytes:"), 0, 0, INT_MAX, 1, &ok);
    if (!ok) {
        return;
    }

    // Get maximum size
    qint64 maxSize = QInputDialog::getInt(this, tr("By Size"), 
                                           tr("Maximum Bytes:"), INT_MAX, 0, INT_MAX, 1, &ok);
    if (!ok) {
        return;
    }

    if (maxSize < minSize) {
        QMessageBox::warning(this, tr("Invalid Range"), 
                            tr("Maximum size must be greater than or equal to minimum size."));
        return;
    }

    QMutexLocker locker(&m_eventLock);
    m_isUpdating = true;

    // First, uncheck all
    for (int i = 0; i < m_treeWidget->topLevelItemCount(); ++i) {
        recursivelySetCheckState(m_treeWidget->topLevelItem(i), false);
    }

    // Get all leaf nodes (files)
    QList<QTreeWidgetItem *> leafNodes = getAllLeafNodes();

    // Check files that match the size criteria
    for (QTreeWidgetItem *item : leafNodes) {
        int itemType = item->data(0, ItemTypeRole).toInt();
        if (itemType == static_cast<int>(ItemType::File)) {
            // Get file size from underlying data
            // In a full implementation, we would get the actual file size from LtfsIndex
            // For now, we'll need to access the file entry through the stored index
            
            // TODO: Get actual file size from m_index using the path/index stored in item
            // For demonstration, we'll check all files (size filtering would need actual data)
            
            // This is a placeholder - real implementation needs file size access
            item->setCheckState(0, Qt::Checked);
        }
    }

    // Refresh states
    for (int i = 0; i < m_treeWidget->topLevelItemCount(); ++i) {
        refreshCheckState(m_treeWidget->topLevelItem(i));
    }

    m_isUpdating = false;
}

void FileBrowserDialog::onSelectByRegex()
{
    bool ok;
    QString pattern = QInputDialog::getText(this, tr("By Pattern"), 
                                            tr("Regex Pattern:"), 
                                            QLineEdit::Normal, ".*", &ok);
    if (!ok || pattern.isEmpty()) {
        return;
    }

    QRegularExpression regex(pattern);
    if (!regex.isValid()) {
        QMessageBox::warning(this, tr("Invalid Pattern"), 
                            tr("The regular expression is invalid: %1")
                            .arg(regex.errorString()));
        return;
    }

    QMutexLocker locker(&m_eventLock);
    m_isUpdating = true;

    // First, uncheck all
    for (int i = 0; i < m_treeWidget->topLevelItemCount(); ++i) {
        recursivelySetCheckState(m_treeWidget->topLevelItem(i), false);
    }

    // Get all leaf nodes (files)
    QList<QTreeWidgetItem *> leafNodes = getAllLeafNodes();

    // Check files that match the pattern
    for (QTreeWidgetItem *item : leafNodes) {
        int itemType = item->data(0, ItemTypeRole).toInt();
        if (itemType == static_cast<int>(ItemType::File)) {
            QString fileName = item->text(0);
            QRegularExpressionMatch match = regex.match(fileName);
            if (match.hasMatch()) {
                item->setCheckState(0, Qt::Checked);
            }
        }
    }

    // Refresh states
    for (int i = 0; i < m_treeWidget->topLevelItemCount(); ++i) {
        refreshCheckState(m_treeWidget->topLevelItem(i));
    }

    m_isUpdating = false;
}

} // namespace app
} // namespace qltfs
