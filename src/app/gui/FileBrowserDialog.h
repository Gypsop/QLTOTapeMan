/*
 * QLTOTapeMan - Qt-based LTO Tape Manager
 *
 * Copyright (c) 2026 Jeffrey ZHU (zhxsh1225@gmail.com)
 * https://github.com/Gypsop/QLTOTapeMan
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef FILEBROWSERDIALOG_H
#define FILEBROWSERDIALOG_H

#include <QDialog>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QPushButton>
#include <QCheckBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMenu>
#include <QAction>
#include <QClipboard>
#include <QGuiApplication>
#include <QInputDialog>
#include <QMessageBox>
#include <QRegularExpression>
#include <QMutex>
#include <QScopedPointer>

#include "LtfsIndex.h"

namespace qltfs {
namespace app {

/**
 * @brief Extended TreeWidget with tri-state checkbox support
 *
 * This class extends QTreeWidget to support three-state checkboxes
 * (Checked, Unchecked, Indeterminate) similar to the VB.NET TreeViewEx.
 */
class TreeWidgetEx : public QTreeWidget
{
    Q_OBJECT

public:
    explicit TreeWidgetEx(QWidget *parent = nullptr);
    virtual ~TreeWidgetEx() = default;

    /**
     * @brief Set the check state for a specific item
     * @param item The tree item to modify
     * @param state The check state to set
     */
    void setItemCheckState(QTreeWidgetItem *item, Qt::CheckState state);

    /**
     * @brief Get the check state for a specific item
     * @param item The tree item to query
     * @return The current check state
     */
    Qt::CheckState itemCheckState(QTreeWidgetItem *item) const;
};

/**
 * @brief FileBrowserDialog - Complete file browser dialog with checkbox selection
 *
 * This dialog provides a tree-based file browser for LTFS index contents.
 * It supports:
 * - Tri-state checkboxes (Checked, Unchecked, Indeterminate)
 * - Recursive selection (selecting a folder selects all children)
 * - Selection by file size range
 * - Selection by filename regex pattern
 * - Copy file info to clipboard on selection
 *
 * This is a faithful reimplementation of FileBrowser.vb from LTFSCopyGUI.
 */
class FileBrowserDialog : public QDialog
{
    Q_OBJECT

public:
    /**
     * @brief Construct a FileBrowserDialog
     * @param parent Parent widget
     */
    explicit FileBrowserDialog(QWidget *parent = nullptr);
    virtual ~FileBrowserDialog() = default;

    /**
     * @brief Set the LTFS index to display
     * @param index Pointer to the LtfsIndex (ownership not transferred)
     */
    void setIndex(core::LtfsIndex *index);

    /**
     * @brief Get the LTFS index
     * @return Pointer to the current LtfsIndex
     */
    core::LtfsIndex *index() const { return m_index; }

    /**
     * @brief Static convenience method to show dialog modally
     * @param index The LTFS index to display
     * @param parent Parent widget
     * @return QDialog::DialogCode result
     */
    static int showDialog(core::LtfsIndex *index, QWidget *parent = nullptr);

    /**
     * @brief Static convenience method to show non-modal dialog
     * @param index The LTFS index to display
     * @param parent Parent widget
     */
    static void showBrowser(core::LtfsIndex *index, QWidget *parent = nullptr);

    /**
     * @brief Get/Set whether to copy file info to clipboard on selection
     */
    bool copyInfoToClipboard() const { return m_copyInfoCheckBox->isChecked(); }
    void setCopyInfoToClipboard(bool enable) { m_copyInfoCheckBox->setChecked(enable); }

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    /**
     * @brief Handle tree item selection change
     * @param current Currently selected item
     * @param previous Previously selected item
     */
    void onItemSelectionChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous);

    /**
     * @brief Handle tree item check state change
     * @param item The item whose check state changed
     * @param column The column (always 0)
     */
    void onItemChanged(QTreeWidgetItem *item, int column);

    /**
     * @brief Handle OK button click
     */
    void onOkClicked();

    /**
     * @brief Handle Cancel button click
     */
    void onCancelClicked();

    /**
     * @brief Handle "Select All" context menu action
     */
    void onSelectAll();

    /**
     * @brief Handle "By Size" context menu action
     */
    void onSelectBySize();

    /**
     * @brief Handle "By Regex" context menu action
     */
    void onSelectByRegex();

    /**
     * @brief Show context menu
     * @param pos Position where the menu was requested
     */
    void showContextMenu(const QPoint &pos);

private:
    /**
     * @brief Initialize the UI components
     */
    void setupUi();

    /**
     * @brief Create context menu
     */
    void createContextMenu();

    /**
     * @brief Load settings from QSettings
     */
    void loadSettings();

    /**
     * @brief Save settings to QSettings
     */
    void saveSettings();

    /**
     * @brief Populate tree from LTFS index
     */
    void populateTree();

    /**
     * @brief Add contents to tree (recursive helper)
     * @param parent Parent tree item (nullptr for root)
     * @param directories List of directories to add
     * @param files List of files to add
     */
    void addContents(QTreeWidgetItem *parent,
                     const QList<core::LtfsDirectoryEntry> &directories,
                     const QList<core::LtfsFileEntry> &files);

    /**
     * @brief Recursively set check state for an item and all its children
     * @param item The item to set
     * @param checked Whether to check or uncheck
     */
    void recursivelySetCheckState(QTreeWidgetItem *item, bool checked);

    /**
     * @brief Refresh check state for an item based on its children
     * @param item The item to refresh
     * @return The resulting check state
     */
    Qt::CheckState refreshCheckState(QTreeWidgetItem *item);

    /**
     * @brief Update the Selected property in the underlying index data
     * @param item The tree item
     * @param selected The selection state
     */
    void updateIndexSelection(QTreeWidgetItem *item, bool selected);

    /**
     * @brief Get all leaf nodes (files) in the tree
     * @return List of all file tree items
     */
    QList<QTreeWidgetItem *> getAllLeafNodes() const;

    /**
     * @brief Get all leaf nodes recursively from a starting item
     * @param item Starting item
     * @param result Output list
     */
    void collectLeafNodes(QTreeWidgetItem *item, QList<QTreeWidgetItem *> &result) const;

    // UI Components
    TreeWidgetEx *m_treeWidget;
    QPushButton *m_okButton;
    QPushButton *m_cancelButton;
    QCheckBox *m_copyInfoCheckBox;
    QMenu *m_contextMenu;
    QAction *m_selectAllAction;
    QAction *m_selectBySizeAction;
    QAction *m_selectByRegexAction;

    // Data
    core::LtfsIndex *m_index;

    // Event lock to prevent recursive event handling
    QMutex m_eventLock;
    bool m_isUpdating;

    // Tag data types for tree items
    enum class ItemType {
        Directory,
        File
    };

    // Custom roles for tree item data
    static constexpr int ItemTypeRole = Qt::UserRole;
    static constexpr int DirectoryIndexRole = Qt::UserRole + 1;
    static constexpr int FileIndexRole = Qt::UserRole + 2;
    static constexpr int PathRole = Qt::UserRole + 3;
};

} // namespace app
} // namespace qltfs

#endif // FILEBROWSERDIALOG_H
