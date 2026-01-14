/*
 * QLTOTapeMan - Qt-based LTO Tape Manager
 *
 * Copyright (c) 2026 Jeffrey ZHU (zhxsh1225@gmail.com)
 * https://github.com/Gypsop/QLTOTapeMan
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "LtfsWriterWindow.h"
#include "FileBrowserDialog.h"

#include <QCloseEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QFileDialog>
#include <QMessageBox>
#include <QInputDialog>
#include <QHeaderView>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QCryptographicHash>

namespace qltfs {
namespace app {

// ============================================================================
// Constructor / Destructor
// ============================================================================

LtfsWriterWindow::LtfsWriterWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_schema(nullptr)
    , m_plabel(nullptr)
    , m_tapeDevice(nullptr)
    , m_modified(false)
    , m_offlineMode(false)
    , m_indexWriteInterval(36LL * 1024 * 1024 * 1024)  // 36 GiB default
    , m_speedLimit(0)  // No limit
    , m_capacityRefreshInterval(30)  // 30 seconds
    , m_hashOnWrite(true)
    , m_asyncHash(false)
    , m_overwriteExisting(false)
    , m_skipSymlinks(true)
    , m_forceIndex(false)
    , m_cleanCycle(3)
    , m_preloadFileCount(5)
    , m_preloadBytes(32LL * 1024 * 1024)  // 32 MiB
    , m_allowOperation(true)
    , m_stopFlag(false)
    , m_pauseFlag(false)
    , m_flushFlag(false)
    , m_speedHistoryMaxPoints(3600 * 6)  // 6 hours at 1 sample/sec
    , m_logEnabled(true)
    , m_updateTimer(nullptr)
    , m_capacityTimer(nullptr)
    , m_speedHistoryTimer(nullptr)
    , m_treeModel(nullptr)
    , m_listModel(nullptr)
    , m_chart(nullptr)
    , m_speedSeries(nullptr)
    , m_errorRateSeries(nullptr)
    , m_fileRateSeries(nullptr)
{
    // Initialize session
    m_sessionStartTime = QDateTime::currentDateTime();
    m_logFilePath = QString("log/LTFSWriter_%1.log")
        .arg(m_sessionStartTime.toString("yyyyMMdd_HHmmss"));

    // Initialize statistics
    m_statistics.totalBytes = 0;
    m_statistics.processedBytes = 0;
    m_statistics.totalFiles = 0;
    m_statistics.processedFiles = 0;
    m_statistics.unindexedBytes = 0;
    m_statistics.currentSpeed = 0;
    m_statistics.averageSpeed = 0;
    m_statistics.startTime = QDateTime::currentDateTime();
    m_statistics.lastIndexTime = QDateTime::currentDateTime();

    // Setup UI
    setupUi();
    createMenus();
    createToolBar();
    createStatusBar();
    createSpeedChart();
    createFileListView();

    // Load settings
    loadSettings();

    // Setup timers
    m_updateTimer = new QTimer(this);
    m_updateTimer->setInterval(200);  // 5 Hz update
    connect(m_updateTimer, &QTimer::timeout, this, &LtfsWriterWindow::onUpdateTimer);
    m_updateTimer->start();

    m_capacityTimer = new QTimer(this);
    m_capacityTimer->setInterval(m_capacityRefreshInterval * 1000);
    connect(m_capacityTimer, &QTimer::timeout, this, &LtfsWriterWindow::onCapacityTimer);
    if (m_capacityRefreshInterval > 0) {
        m_capacityTimer->start();
    }

    m_speedHistoryTimer = new QTimer(this);
    m_speedHistoryTimer->setInterval(1000);  // 1 Hz for history
    connect(m_speedHistoryTimer, &QTimer::timeout, this, &LtfsWriterWindow::onSpeedHistoryTimer);
    m_speedHistoryTimer->start();

    // Enable drag and drop
    setAcceptDrops(true);
}

LtfsWriterWindow::~LtfsWriterWindow()
{
    saveSettings();

    if (m_updateTimer) {
        m_updateTimer->stop();
    }
    if (m_capacityTimer) {
        m_capacityTimer->stop();
    }
    if (m_speedHistoryTimer) {
        m_speedHistoryTimer->stop();
    }

    delete m_tapeDevice;
}

// ============================================================================
// UI Setup
// ============================================================================

void LtfsWriterWindow::setupUi()
{
    setWindowTitle(tr("LTFS Writer - Direct Read/Write"));
    setMinimumSize(1024, 768);
    resize(1280, 900);

    // Central widget with splitter
    m_mainSplitter = new QSplitter(Qt::Horizontal, this);
    setCentralWidget(m_mainSplitter);
}

void LtfsWriterWindow::createMenus()
{
    QMenuBar *menuBar = this->menuBar();

    // File menu
    m_fileMenu = menuBar->addMenu(tr("&File"));
    
    m_newSessionAction = m_fileMenu->addAction(tr("&New Session"));
    m_newSessionAction->setShortcut(QKeySequence::New);
    connect(m_newSessionAction, &QAction::triggered, this, &LtfsWriterWindow::onNewSession);

    m_fileMenu->addSeparator();

    m_openIndexAction = m_fileMenu->addAction(tr("&Open Index..."));
    m_openIndexAction->setShortcut(QKeySequence::Open);
    connect(m_openIndexAction, &QAction::triggered, this, &LtfsWriterWindow::onOpenIndex);

    m_saveIndexAction = m_fileMenu->addAction(tr("&Save Index"));
    m_saveIndexAction->setShortcut(QKeySequence::Save);
    m_saveIndexAction->setEnabled(false);
    connect(m_saveIndexAction, &QAction::triggered, this, &LtfsWriterWindow::onSaveIndex);

    m_saveIndexAsAction = m_fileMenu->addAction(tr("Save Index &As..."));
    m_saveIndexAsAction->setShortcut(QKeySequence::SaveAs);
    connect(m_saveIndexAsAction, &QAction::triggered, this, &LtfsWriterWindow::onSaveIndexAs);

    m_fileMenu->addSeparator();

    m_exitAction = m_fileMenu->addAction(tr("E&xit"));
    m_exitAction->setShortcut(QKeySequence::Quit);
    connect(m_exitAction, &QAction::triggered, this, &QWidget::close);

    // Edit menu
    m_editMenu = menuBar->addMenu(tr("&Edit"));

    m_selectAllAction = m_editMenu->addAction(tr("Select &All"));
    m_selectAllAction->setShortcut(QKeySequence::SelectAll);
    connect(m_selectAllAction, &QAction::triggered, this, &LtfsWriterWindow::onSelectAll);

    m_selectBySizeAction = m_editMenu->addAction(tr("Select by &Size..."));
    connect(m_selectBySizeAction, &QAction::triggered, this, &LtfsWriterWindow::onSelectBySize);

    m_selectByRegexAction = m_editMenu->addAction(tr("Select by &Pattern..."));
    connect(m_selectByRegexAction, &QAction::triggered, this, &LtfsWriterWindow::onSelectByRegex);

    m_editMenu->addSeparator();

    m_deleteSelectedAction = m_editMenu->addAction(tr("&Delete Selected"));
    m_deleteSelectedAction->setShortcut(QKeySequence::Delete);
    connect(m_deleteSelectedAction, &QAction::triggered, this, &LtfsWriterWindow::onDeleteSelected);

    m_copySelectedAction = m_editMenu->addAction(tr("&Copy"));
    m_copySelectedAction->setShortcut(QKeySequence::Copy);
    connect(m_copySelectedAction, &QAction::triggered, this, &LtfsWriterWindow::onCopySelected);

    m_pasteSelectedAction = m_editMenu->addAction(tr("&Paste"));
    m_pasteSelectedAction->setShortcut(QKeySequence::Paste);
    m_pasteSelectedAction->setVisible(false);
    connect(m_pasteSelectedAction, &QAction::triggered, this, &LtfsWriterWindow::onPasteSelected);

    // View menu
    m_viewMenu = menuBar->addMenu(tr("&View"));

    m_refreshAction = m_viewMenu->addAction(tr("&Refresh"));
    m_refreshAction->setShortcut(QKeySequence::Refresh);
    connect(m_refreshAction, &QAction::triggered, this, &LtfsWriterWindow::refreshView);

    m_viewMenu->addSeparator();

    m_showLogAction = m_viewMenu->addAction(tr("Show &Log..."));
    connect(m_showLogAction, &QAction::triggered, this, &LtfsWriterWindow::onShowLog);

    m_showPropertiesAction = m_viewMenu->addAction(tr("&Properties..."));
    connect(m_showPropertiesAction, &QAction::triggered, this, &LtfsWriterWindow::onShowProperties);

    // Tape menu
    m_tapeMenu = menuBar->addMenu(tr("&Tape"));

    m_startWriteAction = m_tapeMenu->addAction(tr("&Start Write"));
    m_startWriteAction->setIcon(QIcon::fromTheme("media-playback-start"));
    connect(m_startWriteAction, &QAction::triggered, this, &LtfsWriterWindow::startWrite);

    m_pauseWriteAction = m_tapeMenu->addAction(tr("&Pause"));
    m_pauseWriteAction->setIcon(QIcon::fromTheme("media-playback-pause"));
    m_pauseWriteAction->setEnabled(false);
    connect(m_pauseWriteAction, &QAction::triggered, this, &LtfsWriterWindow::pauseWrite);

    m_stopWriteAction = m_tapeMenu->addAction(tr("S&top"));
    m_stopWriteAction->setIcon(QIcon::fromTheme("media-playback-stop"));
    m_stopWriteAction->setEnabled(false);
    connect(m_stopWriteAction, &QAction::triggered, this, &LtfsWriterWindow::stopWrite);

    m_tapeMenu->addSeparator();

    m_flushIndexAction = m_tapeMenu->addAction(tr("&Flush Index"));
    connect(m_flushIndexAction, &QAction::triggered, this, &LtfsWriterWindow::flushIndex);

    m_updateDataIndexAction = m_tapeMenu->addAction(tr("&Update Data Index"));
    m_updateDataIndexAction->setEnabled(false);
    connect(m_updateDataIndexAction, &QAction::triggered, this, &LtfsWriterWindow::onUpdateDataIndex);

    m_tapeMenu->addSeparator();

    m_ejectTapeAction = m_tapeMenu->addAction(tr("&Eject Tape"));
    connect(m_ejectTapeAction, &QAction::triggered, this, &LtfsWriterWindow::ejectTape);

    // Settings menu
    m_settingsMenu = menuBar->addMenu(tr("&Settings"));

    m_overwriteExistingAction = m_settingsMenu->addAction(tr("&Overwrite Existing Files"));
    m_overwriteExistingAction->setCheckable(true);
    m_overwriteExistingAction->setChecked(m_overwriteExisting);
    connect(m_overwriteExistingAction, &QAction::toggled, this, &LtfsWriterWindow::onOverwriteExistingToggled);

    m_skipSymlinksAction = m_settingsMenu->addAction(tr("Skip &Symbolic Links"));
    m_skipSymlinksAction->setCheckable(true);
    m_skipSymlinksAction->setChecked(m_skipSymlinks);
    connect(m_skipSymlinksAction, &QAction::toggled, this, &LtfsWriterWindow::onSkipSymlinksToggled);

    m_settingsMenu->addSeparator();

    m_hashOnWriteAction = m_settingsMenu->addAction(tr("Calculate &Hash on Write"));
    m_hashOnWriteAction->setCheckable(true);
    m_hashOnWriteAction->setChecked(m_hashOnWrite);
    connect(m_hashOnWriteAction, &QAction::toggled, this, &LtfsWriterWindow::onHashOnWriteToggled);

    m_asyncHashAction = m_settingsMenu->addAction(tr("&Async Hash (High CPU)"));
    m_asyncHashAction->setCheckable(true);
    m_asyncHashAction->setChecked(m_asyncHash);
    connect(m_asyncHashAction, &QAction::toggled, this, &LtfsWriterWindow::onAsyncHashToggled);

    m_settingsMenu->addSeparator();

    m_forceIndexAction = m_settingsMenu->addAction(tr("Always Update Data &Index"));
    m_forceIndexAction->setCheckable(true);
    m_forceIndexAction->setChecked(m_forceIndex);
    connect(m_forceIndexAction, &QAction::toggled, this, &LtfsWriterWindow::onForceIndexToggled);

    m_logEnabledAction = m_settingsMenu->addAction(tr("Enable &Logging"));
    m_logEnabledAction->setCheckable(true);
    m_logEnabledAction->setChecked(m_logEnabled);

    m_settingsMenu->addSeparator();

    m_speedLimitAction = m_settingsMenu->addAction(tr("Speed &Limit..."));
    connect(m_speedLimitAction, &QAction::triggered, this, &LtfsWriterWindow::onSpeedLimitChanged);

    m_indexIntervalAction = m_settingsMenu->addAction(tr("Index &Interval..."));
    connect(m_indexIntervalAction, &QAction::triggered, this, &LtfsWriterWindow::onIndexIntervalChanged);

    m_capacityIntervalAction = m_settingsMenu->addAction(tr("&Capacity Refresh..."));
    connect(m_capacityIntervalAction, &QAction::triggered, this, &LtfsWriterWindow::onCapacityIntervalChanged);

    m_cleanCycleAction = m_settingsMenu->addAction(tr("Clean C&ycle..."));
    connect(m_cleanCycleAction, &QAction::triggered, this, &LtfsWriterWindow::onCleanCycleChanged);

    // Help menu
    m_helpMenu = menuBar->addMenu(tr("&Help"));
    m_helpMenu->addAction(tr("&About..."), this, [this]() {
        QMessageBox::about(this, tr("About LTFS Writer"),
            tr("QLTOTapeMan LTFS Writer\n\n"
               "Copyright (c) 2026 Jeffrey ZHU\n"
               "https://github.com/Gypsop/QLTOTapeMan"));
    });

    // Context menu
    m_contextMenu = new QMenu(this);
    m_contextMenu->addAction(m_selectAllAction);
    m_contextMenu->addAction(m_selectBySizeAction);
    m_contextMenu->addAction(m_selectByRegexAction);
    m_contextMenu->addSeparator();
    m_contextMenu->addAction(m_deleteSelectedAction);
    m_contextMenu->addAction(m_copySelectedAction);
    m_contextMenu->addAction(m_pasteSelectedAction);
}

void LtfsWriterWindow::createToolBar()
{
    m_mainToolBar = addToolBar(tr("Main"));
    m_mainToolBar->setMovable(false);

    m_mainToolBar->addAction(m_newSessionAction);
    m_mainToolBar->addAction(m_openIndexAction);
    m_mainToolBar->addAction(m_saveIndexAction);
    m_mainToolBar->addSeparator();
    m_mainToolBar->addAction(m_startWriteAction);
    m_mainToolBar->addAction(m_pauseWriteAction);
    m_mainToolBar->addAction(m_stopWriteAction);
    m_mainToolBar->addSeparator();
    m_mainToolBar->addAction(m_refreshAction);
    m_mainToolBar->addAction(m_ejectTapeAction);
}

void LtfsWriterWindow::createStatusBar()
{
    QStatusBar *status = statusBar();

    // Status lights (colored indicators)
    m_statusLight1 = new QLabel("●", this);
    m_statusLight1->setStyleSheet("color: gray;");
    m_statusLight1->setToolTip(tr("Not Ready"));
    status->addWidget(m_statusLight1);

    m_statusLight3 = new QLabel("●", this);
    m_statusLight3->setStyleSheet("color: gray;");
    m_statusLight3->setToolTip(tr("Pause"));
    status->addWidget(m_statusLight3);

    m_statusLight4 = new QLabel("●", this);
    m_statusLight4->setStyleSheet("color: gray;");
    m_statusLight4->setToolTip(tr("Flush"));
    status->addWidget(m_statusLight4);

    m_statusLight6 = new QLabel("●", this);
    m_statusLight6->setStyleSheet("color: gray;");
    m_statusLight6->setToolTip(tr("I/O"));
    status->addWidget(m_statusLight6);

    status->addWidget(new QLabel(" | ", this));

    // Status message
    m_statusLabel = new QLabel(tr("Ready"), this);
    status->addWidget(m_statusLabel, 1);

    // Warning message
    m_warningLabel = new QLabel(this);
    m_warningLabel->setStyleSheet("color: orange;");
    status->addWidget(m_warningLabel);

    status->addPermanentWidget(new QLabel(" | ", this));

    // Error rate
    m_errorRateLabel = new QLabel("0.00", this);
    m_errorRateLabel->setToolTip(tr("Error Rate Log"));
    status->addPermanentWidget(m_errorRateLabel);

    status->addPermanentWidget(new QLabel(" | ", this));

    // Position
    m_positionLabel = new QLabel(tr("P:- B:-"), this);
    m_positionLabel->setToolTip(tr("Partition:Block"));
    status->addPermanentWidget(m_positionLabel);

    status->addPermanentWidget(new QLabel(" | ", this));

    // Capacity
    m_capacityLabel = new QLabel(tr("Capacity: -"), this);
    status->addPermanentWidget(m_capacityLabel);

    // Progress bar
    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setMaximumWidth(200);
    status->addPermanentWidget(m_progressBar);
}

void LtfsWriterWindow::createSpeedChart()
{
    m_chart = new QChart();
    m_chart->setTitle(tr("Speed (MiB/s)"));
    m_chart->legend()->hide();

    m_speedSeries = new QLineSeries();
    m_speedSeries->setName(tr("Speed"));
    m_chart->addSeries(m_speedSeries);

    m_errorRateSeries = new QLineSeries();
    m_errorRateSeries->setName(tr("Error Rate"));
    m_errorRateSeries->setColor(Qt::red);
    // m_chart->addSeries(m_errorRateSeries);  // Optionally show

    m_fileRateSeries = new QLineSeries();
    m_fileRateSeries->setName(tr("Files/s"));
    m_fileRateSeries->setColor(Qt::green);
    // m_chart->addSeries(m_fileRateSeries);  // Optionally show

    m_timeAxis = new QValueAxis();
    m_timeAxis->setTitleText(tr("Time (s)"));
    m_timeAxis->setRange(0, 600);  // 10 minutes
    m_chart->addAxis(m_timeAxis, Qt::AlignBottom);
    m_speedSeries->attachAxis(m_timeAxis);

    m_speedAxis = new QValueAxis();
    m_speedAxis->setTitleText(tr("Speed (MiB/s)"));
    m_speedAxis->setRange(0, 400);  // Max 400 MiB/s
    m_chart->addAxis(m_speedAxis, Qt::AlignLeft);
    m_speedSeries->attachAxis(m_speedAxis);

    m_chartView = new QChartView(m_chart);
    m_chartView->setRenderHint(QPainter::Antialiasing);
    m_chartView->setMinimumHeight(150);
}

void LtfsWriterWindow::createFileListView()
{
    // Left panel - source files/queue
    m_treeView = new QTreeView(this);
    m_treeModel = new QStandardItemModel(this);
    m_treeModel->setHorizontalHeaderLabels({tr("Name"), tr("Size"), tr("Status")});
    m_treeView->setModel(m_treeModel);
    m_treeView->setAlternatingRowColors(true);
    m_treeView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_treeView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_treeView, &QTreeView::customContextMenuRequested, this, &LtfsWriterWindow::showContextMenu);
    m_mainSplitter->addWidget(m_treeView);

    // Right panel - tape contents / list view
    QWidget *rightPanel = new QWidget(this);
    QVBoxLayout *rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(0, 0, 0, 0);

    m_listView = new QTableView(this);
    m_listModel = new QStandardItemModel(this);
    m_listModel->setHorizontalHeaderLabels({tr("Name"), tr("Size"), tr("Modified"), tr("Hash")});
    m_listView->setModel(m_listModel);
    m_listView->setAlternatingRowColors(true);
    m_listView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_listView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_listView->horizontalHeader()->setStretchLastSection(true);
    m_listView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_listView, &QTableView::customContextMenuRequested, this, &LtfsWriterWindow::showContextMenu);
    rightLayout->addWidget(m_listView, 1);

    // Add chart below the list
    rightLayout->addWidget(m_chartView);

    m_mainSplitter->addWidget(rightPanel);

    // Set splitter sizes
    m_mainSplitter->setSizes({400, 600});
}

// ============================================================================
// Settings
// ============================================================================

void LtfsWriterWindow::loadSettings()
{
    QSettings settings;
    settings.beginGroup("LTFSWriter");

    m_overwriteExisting = settings.value("OverwriteExist", false).toBool();
    m_skipSymlinks = settings.value("SkipSymlink", true).toBool();
    m_hashOnWrite = settings.value("HashOnWriting", true).toBool();
    m_asyncHash = settings.value("HashAsync", false).toBool();
    m_forceIndex = settings.value("ForceIndex", false).toBool();
    m_logEnabled = settings.value("LogEnabled", true).toBool();
    m_indexWriteInterval = settings.value("IndexWriteInterval", 36LL * 1024 * 1024 * 1024).toLongLong();
    m_capacityRefreshInterval = settings.value("CapacityRefreshInterval", 30).toInt();
    m_speedLimit = settings.value("SpeedLimit", 0).toLongLong();
    m_cleanCycle = settings.value("CleanCycle", 3).toInt();
    m_preloadFileCount = settings.value("PreLoadFileCount", 5).toInt();
    m_preloadBytes = settings.value("PreLoadBytes", 32LL * 1024 * 1024).toLongLong();

    settings.endGroup();

    // Update UI
    m_overwriteExistingAction->setChecked(m_overwriteExisting);
    m_skipSymlinksAction->setChecked(m_skipSymlinks);
    m_hashOnWriteAction->setChecked(m_hashOnWrite);
    m_asyncHashAction->setChecked(m_asyncHash);
    m_forceIndexAction->setChecked(m_forceIndex);
    m_logEnabledAction->setChecked(m_logEnabled);
}

void LtfsWriterWindow::saveSettings()
{
    QSettings settings;
    settings.beginGroup("LTFSWriter");

    settings.setValue("OverwriteExist", m_overwriteExisting);
    settings.setValue("SkipSymlink", m_skipSymlinks);
    settings.setValue("HashOnWriting", m_hashOnWrite);
    settings.setValue("HashAsync", m_asyncHash);
    settings.setValue("ForceIndex", m_forceIndex);
    settings.setValue("LogEnabled", m_logEnabled);
    settings.setValue("IndexWriteInterval", m_indexWriteInterval);
    settings.setValue("CapacityRefreshInterval", m_capacityRefreshInterval);
    settings.setValue("SpeedLimit", m_speedLimit);
    settings.setValue("CleanCycle", m_cleanCycle);
    settings.setValue("PreLoadFileCount", m_preloadFileCount);
    settings.setValue("PreLoadBytes", m_preloadBytes);

    settings.endGroup();
}

// ============================================================================
// Property Setters
// ============================================================================

void LtfsWriterWindow::setTapeDrive(const QString &drive)
{
    m_tapeDrive = drive;
    setWindowTitle(tr("LTFS Writer - %1").arg(drive.isEmpty() ? tr("No Device") : drive));
}

void LtfsWriterWindow::setSchema(core::LtfsIndex *schema)
{
    m_schema = schema;
    refreshView();
}

void LtfsWriterWindow::setPlabel(core::LtfsLabel *label)
{
    m_plabel = label;
}

void LtfsWriterWindow::setModified(bool modified)
{
    m_modified = modified;
    m_saveIndexAction->setEnabled(modified);
}

void LtfsWriterWindow::setOfflineMode(bool offline)
{
    m_offlineMode = offline;
}

void LtfsWriterWindow::setBarcode(const QString &barcode)
{
    m_barcode = barcode;
}

void LtfsWriterWindow::setIndexWriteInterval(qint64 interval)
{
    m_indexWriteInterval = qMax(0LL, interval);
    if (m_indexWriteInterval == 0) {
        m_indexIntervalAction->setText(tr("Index Interval: None"));
    } else {
        m_indexIntervalAction->setText(tr("Index Interval: %1").arg(formatSize(m_indexWriteInterval)));
    }
}

void LtfsWriterWindow::setSpeedLimit(qint64 limit)
{
    m_speedLimit = qMax(0LL, limit);
    if (m_speedLimit == 0) {
        m_speedLimitAction->setText(tr("Speed Limit: None"));
    } else {
        m_speedLimitAction->setText(tr("Speed Limit: %1 MiB/s").arg(m_speedLimit));
    }
}

void LtfsWriterWindow::setCapacityRefreshInterval(int interval)
{
    m_capacityRefreshInterval = qMax(0, interval);
    if (m_capacityTimer) {
        if (m_capacityRefreshInterval > 0) {
            m_capacityTimer->setInterval(m_capacityRefreshInterval * 1000);
            m_capacityTimer->start();
        } else {
            m_capacityTimer->stop();
        }
    }
}

void LtfsWriterWindow::setHashOnWrite(bool enable)
{
    m_hashOnWrite = enable;
    m_hashOnWriteAction->setChecked(enable);
}

void LtfsWriterWindow::setAllowOperation(bool allow)
{
    m_allowOperation = allow;
    m_startWriteAction->setEnabled(allow);
    m_ejectTapeAction->setEnabled(allow);
}

// ============================================================================
// Event Handlers
// ============================================================================

void LtfsWriterWindow::closeEvent(QCloseEvent *event)
{
    if (m_modified) {
        QMessageBox::StandardButton result = QMessageBox::question(
            this, tr("Unsaved Changes"),
            tr("There are unsaved changes. Do you want to save before closing?"),
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);

        if (result == QMessageBox::Save) {
            onSaveIndex();
        } else if (result == QMessageBox::Cancel) {
            event->ignore();
            return;
        }
    }

    saveSettings();
    event->accept();
}

void LtfsWriterWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void LtfsWriterWindow::dropEvent(QDropEvent *event)
{
    QStringList paths;
    for (const QUrl &url : event->mimeData()->urls()) {
        if (url.isLocalFile()) {
            paths.append(url.toLocalFile());
        }
    }
    if (!paths.isEmpty()) {
        addFiles(paths);
    }
}

// ============================================================================
// Timer Handlers
// ============================================================================

void LtfsWriterWindow::onUpdateTimer()
{
    updateStatusLights();

    // Update status text
    m_statusLabel->setText(m_statusLabel->text());

    // Update progress
    if (m_statistics.totalBytes > 0) {
        int percent = static_cast<int>(m_statistics.processedBytes * 100 / m_statistics.totalBytes);
        m_progressBar->setValue(percent);
        emit progressChanged(percent);
    }
}

void LtfsWriterWindow::onCapacityTimer()
{
    updateCapacityDisplay();
}

void LtfsWriterWindow::onSpeedHistoryTimer()
{
    if (m_statistics.currentSpeed > 0 || !m_speedHistory.isEmpty()) {
        SpeedHistoryPoint point;
        point.timestamp = QDateTime::currentDateTime();
        point.speed = m_statistics.currentSpeed / (1024.0 * 1024.0);  // Convert to MiB/s
        point.errorRate = 0;  // TODO: Get from device
        point.fileRate = 0;   // TODO: Calculate

        m_speedHistory.append(point);

        // Limit history size
        while (m_speedHistory.size() > m_speedHistoryMaxPoints) {
            m_speedHistory.removeFirst();
        }

        updateSpeedGraph();
    }
}

// ============================================================================
// Public Slots
// ============================================================================

void LtfsWriterWindow::addFiles(const QStringList &paths)
{
    for (const QString &path : paths) {
        QFileInfo info(path);
        if (!info.exists()) {
            continue;
        }

        QList<QStandardItem *> row;
        row.append(new QStandardItem(info.fileName()));
        row.append(new QStandardItem(info.isDir() ? tr("<DIR>") : formatSize(info.size())));
        row.append(new QStandardItem(tr("Pending")));

        m_treeModel->appendRow(row);
    }

    setModified(true);
    printMessage(tr("Added %1 items to queue").arg(paths.size()));
}

void LtfsWriterWindow::removeSelectedItems()
{
    QModelIndexList selected = m_treeView->selectionModel()->selectedRows();
    
    // Remove in reverse order to maintain indices
    for (int i = selected.size() - 1; i >= 0; --i) {
        m_treeModel->removeRow(selected[i].row());
    }

    setModified(true);
}

void LtfsWriterWindow::clearQueue()
{
    m_treeModel->removeRows(0, m_treeModel->rowCount());
    setModified(true);
}

void LtfsWriterWindow::startWrite()
{
    if (!m_allowOperation) {
        QMessageBox::warning(this, tr("Operation Not Allowed"),
            tr("An operation is already in progress."));
        return;
    }

    // TODO: Implement actual write operation
    printMessage(tr("Starting write operation..."));
    m_startWriteAction->setEnabled(false);
    m_pauseWriteAction->setEnabled(true);
    m_stopWriteAction->setEnabled(true);
}

void LtfsWriterWindow::pauseWrite()
{
    m_pauseFlag = !m_pauseFlag;
    if (m_pauseFlag) {
        printMessage(tr("Write paused"));
        m_pauseWriteAction->setText(tr("&Resume"));
    } else {
        printMessage(tr("Write resumed"));
        m_pauseWriteAction->setText(tr("&Pause"));
    }
}

void LtfsWriterWindow::stopWrite()
{
    m_stopFlag = true;
    printMessage(tr("Stopping write operation..."));
}

void LtfsWriterWindow::flushIndex()
{
    m_flushFlag = true;
    printMessage(tr("Flushing index..."));
}

void LtfsWriterWindow::ejectTape()
{
    if (!m_allowOperation) {
        return;
    }

    // TODO: Implement actual eject
    printMessage(tr("Ejecting tape..."));
    emit tapeEjected();
}

void LtfsWriterWindow::refreshView()
{
    // Refresh the tape contents view
    m_listModel->removeRows(0, m_listModel->rowCount());

    if (m_schema) {
        // Populate from schema
        // TODO: Traverse LtfsIndex and populate list
    }
}

void LtfsWriterWindow::showFileBrowser()
{
    if (m_schema) {
        FileBrowserDialog::showDialog(m_schema, this);
    }
}

// ============================================================================
// Menu Action Slots
// ============================================================================

void LtfsWriterWindow::onNewSession()
{
    if (m_modified) {
        QMessageBox::StandardButton result = QMessageBox::question(
            this, tr("Unsaved Changes"),
            tr("There are unsaved changes. Do you want to save first?"),
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);

        if (result == QMessageBox::Save) {
            onSaveIndex();
        } else if (result == QMessageBox::Cancel) {
            return;
        }
    }

    clearQueue();
    m_listModel->removeRows(0, m_listModel->rowCount());
    m_speedHistory.clear();
    m_statistics = WriteStatistics();
    m_statistics.startTime = QDateTime::currentDateTime();
    setModified(false);

    printMessage(tr("New session started"));
}

void LtfsWriterWindow::onOpenIndex()
{
    QString fileName = QFileDialog::getOpenFileName(
        this, tr("Open LTFS Index"),
        QString(), tr("LTFS Index (*.xml);;All Files (*)"));

    if (fileName.isEmpty()) {
        return;
    }

    // TODO: Parse index file
    printMessage(tr("Opened index: %1").arg(fileName));
}

void LtfsWriterWindow::onSaveIndex()
{
    // TODO: Implement save
    printMessage(tr("Index saved"));
    setModified(false);
}

void LtfsWriterWindow::onSaveIndexAs()
{
    QString fileName = QFileDialog::getSaveFileName(
        this, tr("Save LTFS Index"),
        QString(), tr("LTFS Index (*.xml);;All Files (*)"));

    if (fileName.isEmpty()) {
        return;
    }

    // TODO: Implement save
    printMessage(tr("Index saved to: %1").arg(fileName));
    setModified(false);
}

void LtfsWriterWindow::onOverwriteExistingToggled(bool checked)
{
    m_overwriteExisting = checked;
}

void LtfsWriterWindow::onSkipSymlinksToggled(bool checked)
{
    m_skipSymlinks = checked;
}

void LtfsWriterWindow::onHashOnWriteToggled(bool checked)
{
    m_hashOnWrite = checked;
}

void LtfsWriterWindow::onAsyncHashToggled(bool checked)
{
    m_asyncHash = checked;
}

void LtfsWriterWindow::onUpdateDataIndex()
{
    // TODO: Implement data index update
    printMessage(tr("Updating data partition index..."));
}

void LtfsWriterWindow::onForceIndexToggled(bool checked)
{
    m_forceIndex = checked;
}

void LtfsWriterWindow::onSelectAll()
{
    m_treeView->selectAll();
}

void LtfsWriterWindow::onSelectBySize()
{
    // TODO: Show size filter dialog
}

void LtfsWriterWindow::onSelectByRegex()
{
    // TODO: Show regex filter dialog
}

void LtfsWriterWindow::onDeleteSelected()
{
    removeSelectedItems();
}

void LtfsWriterWindow::onCopySelected()
{
    // TODO: Implement copy to internal clipboard
}

void LtfsWriterWindow::onPasteSelected()
{
    // TODO: Implement paste from internal clipboard
}

void LtfsWriterWindow::onSpeedLimitChanged()
{
    bool ok;
    int limit = QInputDialog::getInt(this, tr("Speed Limit"),
        tr("Enter speed limit in MiB/s (0 = no limit):"),
        static_cast<int>(m_speedLimit), 0, 1000, 1, &ok);

    if (ok) {
        setSpeedLimit(limit);
    }
}

void LtfsWriterWindow::onIndexIntervalChanged()
{
    bool ok;
    int interval = QInputDialog::getInt(this, tr("Index Interval"),
        tr("Enter index interval in GiB (0 = disabled):"),
        static_cast<int>(m_indexWriteInterval / (1024 * 1024 * 1024)), 0, 1000, 1, &ok);

    if (ok) {
        setIndexWriteInterval(static_cast<qint64>(interval) * 1024 * 1024 * 1024);
    }
}

void LtfsWriterWindow::onCapacityIntervalChanged()
{
    bool ok;
    int interval = QInputDialog::getInt(this, tr("Capacity Refresh"),
        tr("Enter refresh interval in seconds (0 = disabled):"),
        m_capacityRefreshInterval, 0, 3600, 1, &ok);

    if (ok) {
        setCapacityRefreshInterval(interval);
    }
}

void LtfsWriterWindow::onCleanCycleChanged()
{
    bool ok;
    int cycle = QInputDialog::getInt(this, tr("Clean Cycle"),
        tr("Enter clean cycle count (0 = disabled):"),
        m_cleanCycle, 0, 100, 1, &ok);

    if (ok) {
        m_cleanCycle = cycle;
    }
}

void LtfsWriterWindow::onPreloadSettingsChanged()
{
    // TODO: Implement preload settings dialog
}

void LtfsWriterWindow::onShowProperties()
{
    // TODO: Show properties dialog
}

void LtfsWriterWindow::onShowLog()
{
    // TODO: Show log viewer
}

void LtfsWriterWindow::showContextMenu(const QPoint &pos)
{
    QWidget *sender = qobject_cast<QWidget *>(this->sender());
    if (sender) {
        m_contextMenu->exec(sender->mapToGlobal(pos));
    }
}

// ============================================================================
// Helper Methods
// ============================================================================

void LtfsWriterWindow::printMessage(const QString &message, bool warning,
                                    const QString &tooltip, bool logOnly)
{
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
    QString logType = warning ? "warn" : "info";

    if (m_logEnabled) {
        QString logLine = QString("%1 %2> %3").arg(timestamp, logType, message);
        if (!tooltip.isEmpty()) {
            logLine += QString(" (%1)").arg(tooltip);
        }
        m_logBuffer += logLine + "\n";

        // Flush log periodically (would be implemented with a timer in production)
    }

    if (!logOnly) {
        if (warning) {
            m_warningLabel->setText(message);
            m_warningLabel->setToolTip(tooltip.isEmpty() ? message : tooltip);
        } else {
            m_statusLabel->setText(message);
            m_statusLabel->setToolTip(tooltip.isEmpty() ? message : tooltip);
        }
    }

    emit statusMessage(message, warning);
}

void LtfsWriterWindow::updateStatusLights()
{
    // Update IO indicator
    if (m_tapeDevice && m_tapeDevice->isOpen()) {
        m_statusLight6->setStyleSheet("color: green;");
    } else {
        m_statusLight6->setStyleSheet("color: gray;");
    }

    // Update pause indicator
    if (m_pauseFlag) {
        m_statusLight3->setStyleSheet("color: orange;");
    } else {
        m_statusLight3->setStyleSheet("color: gray;");
    }

    // Update flush indicator
    if (m_flushFlag) {
        m_statusLight4->setStyleSheet("color: orange;");
    } else {
        m_statusLight4->setStyleSheet("color: gray;");
    }

    // Update main status light
    if (!m_allowOperation) {
        m_statusLight1->setStyleSheet("color: orange;");
        m_statusLight1->setToolTip(tr("Busy"));
    } else if (m_tapeDevice && m_tapeDevice->isOpen()) {
        m_statusLight1->setStyleSheet("color: blue;");
        m_statusLight1->setToolTip(tr("Idle"));
    } else {
        m_statusLight1->setStyleSheet("color: gray;");
        m_statusLight1->setToolTip(tr("Not Ready"));
    }
}

void LtfsWriterWindow::updateCapacityDisplay()
{
    // TODO: Read actual capacity from tape device
    m_capacityLabel->setText(tr("Capacity: -"));
}

void LtfsWriterWindow::updateSpeedGraph()
{
    m_speedSeries->clear();

    if (m_speedHistory.isEmpty()) {
        return;
    }

    qreal maxSpeed = 10;  // Minimum Y axis value
    QDateTime startTime = m_speedHistory.first().timestamp;

    for (const SpeedHistoryPoint &point : m_speedHistory) {
        qreal x = startTime.secsTo(point.timestamp);
        m_speedSeries->append(x, point.speed);
        maxSpeed = qMax(maxSpeed, point.speed);
    }

    // Update axes
    qreal duration = startTime.secsTo(m_speedHistory.last().timestamp);
    m_timeAxis->setRange(0, qMax(60.0, duration));
    m_speedAxis->setRange(0, maxSpeed * 1.1);
}

QString LtfsWriterWindow::formatSize(qint64 bytes) const
{
    const char *units[] = {"B", "KiB", "MiB", "GiB", "TiB"};
    int unitIndex = 0;
    double size = bytes;

    while (size >= 1024.0 && unitIndex < 4) {
        size /= 1024.0;
        unitIndex++;
    }

    return QString("%1 %2").arg(size, 0, 'f', 2).arg(units[unitIndex]);
}

QString LtfsWriterWindow::formatSpeed(double bytesPerSec) const
{
    return formatSize(static_cast<qint64>(bytesPerSec)) + "/s";
}

QString LtfsWriterWindow::formatDuration(qint64 seconds) const
{
    qint64 hours = seconds / 3600;
    qint64 minutes = (seconds % 3600) / 60;
    qint64 secs = seconds % 60;

    if (hours > 0) {
        return QString("%1:%2:%3").arg(hours).arg(minutes, 2, 10, QChar('0')).arg(secs, 2, 10, QChar('0'));
    } else {
        return QString("%1:%2").arg(minutes).arg(secs, 2, 10, QChar('0'));
    }
}

} // namespace app
} // namespace qltfs
