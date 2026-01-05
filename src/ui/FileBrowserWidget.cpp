#include "FileBrowserWidget.h"
#include "ui_FileBrowserWidget.h"
#include <QDateTime>

FileBrowserWidget::FileBrowserWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::FileBrowserWidget)
{
    ui->setupUi(this);
    ui->treeFiles->setColumnWidth(0, 300);
}

FileBrowserWidget::~FileBrowserWidget()
{
    delete ui;
}

void FileBrowserWidget::loadIndex(const LtfsIndex &index)
{
    m_currentIndex = index;
    ui->lblTapeName->setText(QString("Tape: %1 (Serial: %2)").arg(index.volumeUuid, index.volumeUuid)); // Using UUID as serial for now if not available
    
    ui->treeFiles->clear();
    
    // Root directory
    QTreeWidgetItem *rootItem = new QTreeWidgetItem(ui->treeFiles);
    rootItem->setText(0, "/");
    rootItem->setIcon(0, style()->standardIcon(QStyle::SP_DirIcon));
    
    populateTree(index.rootDirectory, rootItem);
    
    rootItem->setExpanded(true);
}

void FileBrowserWidget::clear()
{
    ui->treeFiles->clear();
    ui->lblTapeName->setText("No Tape Selected");
}

void FileBrowserWidget::populateTree(const LtfsDirectory &dir, QTreeWidgetItem *parentItem)
{
    // Add subdirectories
    for (const auto &subDir : dir.subdirectories) {
        QTreeWidgetItem *item = new QTreeWidgetItem(parentItem);
        item->setText(0, subDir.name);
        item->setText(2, subDir.modifyTime.toString("yyyy-MM-dd HH:mm:ss"));
        item->setIcon(0, style()->standardIcon(QStyle::SP_DirIcon));
        item->setData(0, Qt::UserRole, "dir");
        
        populateTree(subDir, item);
    }

    // Add files
    for (const auto &file : dir.files) {
        QTreeWidgetItem *item = new QTreeWidgetItem(parentItem);
        item->setText(0, file.name);
        item->setText(1, formatSize(file.length));
        item->setText(2, file.modifyTime.toString("yyyy-MM-dd HH:mm:ss"));
        item->setText(3, file.sha1); // SHA1 column
        item->setIcon(0, style()->standardIcon(QStyle::SP_FileIcon));
        item->setData(0, Qt::UserRole, "file");
    }
}

QString FileBrowserWidget::formatSize(long long size)
{
    if (size < 1024) return QString("%1 B").arg(size);
    if (size < 1024 * 1024) return QString("%1 KB").arg(size / 1024.0, 0, 'f', 2);
    if (size < 1024 * 1024 * 1024) return QString("%1 MB").arg(size / (1024.0 * 1024.0), 0, 'f', 2);
    return QString("%1 GB").arg(size / (1024.0 * 1024.0 * 1024.0), 0, 'f', 2);
}

void FileBrowserWidget::on_txtSearch_textChanged(const QString &arg1)
{
    // Simple search implementation: hide items that don't match
    // This is a recursive search which might be slow for large trees, 
    // but sufficient for a prototype.
    
    QTreeWidgetItemIterator it(ui->treeFiles);
    while (*it) {
        if (arg1.isEmpty()) {
            (*it)->setHidden(false);
        } else {
            if ((*it)->text(0).contains(arg1, Qt::CaseInsensitive)) {
                (*it)->setHidden(false);
                // Ensure parents are visible
                QTreeWidgetItem *parent = (*it)->parent();
                while (parent) {
                    parent->setHidden(false);
                    parent->setExpanded(true);
                    parent = parent->parent();
                }
            } else {
                (*it)->setHidden(true);
            }
        }
        ++it;
    }
}
