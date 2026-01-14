/*
 * QLTOTapeMan - Qt-based LTO Tape Manager
 *
 * Copyright (c) 2026 Jeffrey ZHU (zhxsh1225@gmail.com)
 * https://github.com/Gypsop/QLTOTapeMan
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef LTFSWRITERWINDOW_H
#define LTFSWRITERWINDOW_H

#include <QMainWindow>
#include <QTreeView>
#include <QListView>
#include <QTableView>
#include <QSplitter>
#include <QStatusBar>
#include <QMenuBar>
#include <QToolBar>
#include <QProgressBar>
#include <QLabel>
#include <QTimer>
#include <QThread>
#include <QMutex>
#include <QSettings>
#include <QChart>
#include <QChartView>
#include <QLineSeries>
#include <QValueAxis>
#include <QStandardItemModel>
#include <QFileSystemModel>

#include "LtfsIndex.h"
#include "LtfsLabel.h"
#include "TapeDevice.h"

namespace qltfs {
namespace app {

/**
 * @brief Write task item for the queue
 */
struct WriteTaskItem
{
    QString sourcePath;          ///< Source file/directory path
    QString targetPath;          ///< Target path on tape
    qint64 size;                 ///< Size in bytes
    bool isDirectory;            ///< Whether this is a directory
    bool selected;               ///< Whether this item is selected
};

/**
 * @brief Write statistics
 */
struct WriteStatistics
{
    qint64 totalBytes;           ///< Total bytes to write
    qint64 processedBytes;       ///< Bytes processed so far
    qint64 totalFiles;           ///< Total files to write
    qint64 processedFiles;       ///< Files processed so far
    qint64 unindexedBytes;       ///< Bytes written since last index
    double currentSpeed;         ///< Current write speed (bytes/sec)
    double averageSpeed;         ///< Average write speed (bytes/sec)
    QDateTime startTime;         ///< Start time of operation
    QDateTime lastIndexTime;     ///< Time of last index write
};

/**
 * @brief Speed history for graphing
 */
struct SpeedHistoryPoint
{
    QDateTime timestamp;
    double speed;                ///< Speed in MiB/s
    double errorRate;            ///< Error rate log
    double fileRate;             ///< Files per second
};

/**
 * @brief LtfsWriterWindow - Direct read/write window for LTFS operations
 *
 * This is the main tape writing interface, providing:
 * - File browser with drag-and-drop support
 * - Write queue management
 * - Real-time speed graph
 * - Tape capacity monitoring
 * - Index management
 * - Hash verification on write
 *
 * This is a faithful reimplementation of LTFSWriter.vb from LTFSCopyGUI.
 */
class LtfsWriterWindow : public QMainWindow
{
    Q_OBJECT

    // Properties mirroring VB.NET version
    Q_PROPERTY(QString tapeDrive READ tapeDrive WRITE setTapeDrive)
    Q_PROPERTY(bool modified READ isModified WRITE setModified)
    Q_PROPERTY(bool offlineMode READ isOfflineMode WRITE setOfflineMode)
    Q_PROPERTY(QString barcode READ barcode WRITE setBarcode)
    Q_PROPERTY(qint64 indexWriteInterval READ indexWriteInterval WRITE setIndexWriteInterval)
    Q_PROPERTY(qint64 speedLimit READ speedLimit WRITE setSpeedLimit)
    Q_PROPERTY(int capacityRefreshInterval READ capacityRefreshInterval WRITE setCapacityRefreshInterval)
    Q_PROPERTY(bool hashOnWrite READ hashOnWrite WRITE setHashOnWrite)
    Q_PROPERTY(bool allowOperation READ allowOperation WRITE setAllowOperation)

public:
    explicit LtfsWriterWindow(QWidget *parent = nullptr);
    virtual ~LtfsWriterWindow();

    // Property accessors
    QString tapeDrive() const { return m_tapeDrive; }
    void setTapeDrive(const QString &drive);

    core::LtfsIndex *schema() const { return m_schema; }
    void setSchema(core::LtfsIndex *schema);

    core::LtfsLabel *plabel() const { return m_plabel; }
    void setPlabel(core::LtfsLabel *label);

    bool isModified() const { return m_modified; }
    void setModified(bool modified);

    bool isOfflineMode() const { return m_offlineMode; }
    void setOfflineMode(bool offline);

    QString barcode() const { return m_barcode; }
    void setBarcode(const QString &barcode);

    qint64 indexWriteInterval() const { return m_indexWriteInterval; }
    void setIndexWriteInterval(qint64 interval);

    qint64 speedLimit() const { return m_speedLimit; }
    void setSpeedLimit(qint64 limit);

    int capacityRefreshInterval() const { return m_capacityRefreshInterval; }
    void setCapacityRefreshInterval(int interval);

    bool hashOnWrite() const { return m_hashOnWrite; }
    void setHashOnWrite(bool enable);

    bool allowOperation() const { return m_allowOperation; }
    void setAllowOperation(bool allow);

    // Statistics
    const WriteStatistics &statistics() const { return m_statistics; }

signals:
    /**
     * @brief Emitted when LTFS is loaded successfully
     */
    void ltfsLoaded();

    /**
     * @brief Emitted when write operation finishes
     */
    void writeFinished();

    /**
     * @brief Emitted when tape is ejected
     */
    void tapeEjected();

    /**
     * @brief Emitted when progress updates
     * @param percent Progress percentage (0-100)
     */
    void progressChanged(int percent);

    /**
     * @brief Emitted when status message changes
     * @param message Status message
     * @param isWarning Whether this is a warning
     */
    void statusMessage(const QString &message, bool isWarning);

public slots:
    // File operations
    void addFiles(const QStringList &paths);
    void removeSelectedItems();
    void clearQueue();

    // Tape operations
    void startWrite();
    void pauseWrite();
    void stopWrite();
    void flushIndex();
    void ejectTape();

    // View operations
    void refreshView();
    void showFileBrowser();

protected:
    void closeEvent(QCloseEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private slots:
    // Timer handlers
    void onUpdateTimer();
    void onCapacityTimer();
    void onSpeedHistoryTimer();

    // Menu actions
    void onNewSession();
    void onOpenIndex();
    void onSaveIndex();
    void onSaveIndexAs();

    void onOverwriteExistingToggled(bool checked);
    void onSkipSymlinksToggled(bool checked);
    void onHashOnWriteToggled(bool checked);
    void onAsyncHashToggled(bool checked);

    void onUpdateDataIndex();
    void onForceIndexToggled(bool checked);

    void onSelectAll();
    void onSelectBySize();
    void onSelectByRegex();
    void onDeleteSelected();
    void onCopySelected();
    void onPasteSelected();

    void onSpeedLimitChanged();
    void onIndexIntervalChanged();
    void onCapacityIntervalChanged();
    void onCleanCycleChanged();
    void onPreloadSettingsChanged();

    void onShowProperties();
    void onShowLog();

    // Context menu
    void showContextMenu(const QPoint &pos);

private:
    // UI setup
    void setupUi();
    void createMenus();
    void createToolBar();
    void createStatusBar();
    void createSpeedChart();
    void createFileListView();

    // Settings
    void loadSettings();
    void saveSettings();

    // Helper methods
    void printMessage(const QString &message, bool warning = false, 
                      const QString &tooltip = QString(), bool logOnly = false);
    void updateStatusLights();
    void updateCapacityDisplay();
    void updateSpeedGraph();
    QString formatSize(qint64 bytes) const;
    QString formatSpeed(double bytesPerSec) const;
    QString formatDuration(qint64 seconds) const;

    // Write operations
    void performWrite();
    void writeFile(const QString &sourcePath, const QString &targetPath);
    void writeDirectory(const QString &sourcePath, const QString &targetPath);
    bool shouldWriteIndex() const;

    // Data members
    QString m_tapeDrive;
    core::LtfsIndex *m_schema;
    core::LtfsLabel *m_plabel;
    device::TapeDevice *m_tapeDevice;

    bool m_modified;
    bool m_offlineMode;
    QString m_barcode;
    QByteArray m_encryptionKey;

    // Settings
    qint64 m_indexWriteInterval;
    qint64 m_speedLimit;
    int m_capacityRefreshInterval;
    bool m_hashOnWrite;
    bool m_asyncHash;
    bool m_overwriteExisting;
    bool m_skipSymlinks;
    bool m_forceIndex;
    int m_cleanCycle;
    int m_preloadFileCount;
    qint64 m_preloadBytes;

    // State
    bool m_allowOperation;
    bool m_stopFlag;
    bool m_pauseFlag;
    bool m_flushFlag;
    QMutex m_operationLock;

    // Statistics
    WriteStatistics m_statistics;
    QList<SpeedHistoryPoint> m_speedHistory;
    int m_speedHistoryMaxPoints;

    // Timers
    QTimer *m_updateTimer;
    QTimer *m_capacityTimer;
    QTimer *m_speedHistoryTimer;

    // Logging
    QDateTime m_sessionStartTime;
    QString m_logFilePath;
    bool m_logEnabled;
    QString m_logBuffer;

    // UI Components
    QSplitter *m_mainSplitter;

    // Left panel - File browser / queue
    QTreeView *m_treeView;
    QStandardItemModel *m_treeModel;

    // Right panel - Tape contents
    QTableView *m_listView;
    QStandardItemModel *m_listModel;

    // Bottom panel - Speed chart
    QChartView *m_chartView;
    QChart *m_chart;
    QLineSeries *m_speedSeries;
    QLineSeries *m_errorRateSeries;
    QLineSeries *m_fileRateSeries;
    QValueAxis *m_timeAxis;
    QValueAxis *m_speedAxis;

    // Status bar components
    QLabel *m_statusLabel;
    QLabel *m_warningLabel;
    QLabel *m_capacityLabel;
    QLabel *m_positionLabel;
    QLabel *m_errorRateLabel;
    QProgressBar *m_progressBar;

    // Status lights
    QLabel *m_statusLight1;  // Main status
    QLabel *m_statusLight2;  // Reserved
    QLabel *m_statusLight3;  // Pause indicator
    QLabel *m_statusLight4;  // Flush indicator
    QLabel *m_statusLight6;  // IO indicator

    // Menus
    QMenu *m_fileMenu;
    QMenu *m_editMenu;
    QMenu *m_viewMenu;
    QMenu *m_tapeMenu;
    QMenu *m_settingsMenu;
    QMenu *m_helpMenu;
    QMenu *m_contextMenu;

    // Actions
    QAction *m_newSessionAction;
    QAction *m_openIndexAction;
    QAction *m_saveIndexAction;
    QAction *m_saveIndexAsAction;
    QAction *m_exitAction;

    QAction *m_selectAllAction;
    QAction *m_selectBySizeAction;
    QAction *m_selectByRegexAction;
    QAction *m_deleteSelectedAction;
    QAction *m_copySelectedAction;
    QAction *m_pasteSelectedAction;

    QAction *m_refreshAction;
    QAction *m_showLogAction;
    QAction *m_showPropertiesAction;

    QAction *m_startWriteAction;
    QAction *m_pauseWriteAction;
    QAction *m_stopWriteAction;
    QAction *m_flushIndexAction;
    QAction *m_updateDataIndexAction;
    QAction *m_ejectTapeAction;

    QAction *m_overwriteExistingAction;
    QAction *m_skipSymlinksAction;
    QAction *m_hashOnWriteAction;
    QAction *m_asyncHashAction;
    QAction *m_forceIndexAction;
    QAction *m_logEnabledAction;

    QAction *m_speedLimitAction;
    QAction *m_indexIntervalAction;
    QAction *m_capacityIntervalAction;
    QAction *m_cleanCycleAction;
    QAction *m_preloadCountAction;
    QAction *m_preloadBytesAction;

    // Toolbar
    QToolBar *m_mainToolBar;

    // Clipboard
    QList<core::LtfsDirectoryEntry> m_clipboardDirectories;
    QList<core::LtfsFileEntry> m_clipboardFiles;
};

} // namespace app
} // namespace qltfs

#endif // LTFSWRITERWINDOW_H
