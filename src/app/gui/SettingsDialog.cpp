/*
 * QLTOTapeMan - Qt-based LTO Tape Manager
 *
 * Copyright (c) 2026 Jeffrey ZHU (zhxsh1225@gmail.com)
 * https://github.com/Gypsop/QLTOTapeMan
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "SettingsDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QFileDialog>
#include <QLabel>
#include <QDialogButtonBox>

namespace qltfs {
namespace app {

SettingsDialog::SettingsDialog(QWidget *parent)
    : QDialog(parent)
{
    setupUi();
    loadSettings();
}

void SettingsDialog::setupUi()
{
    setWindowTitle(tr("Settings"));
    setMinimumSize(500, 450);
    resize(550, 500);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // Tab widget
    m_tabWidget = new QTabWidget(this);
    createGeneralTab();
    createWriteTab();
    createDeviceTab();
    createAdvancedTab();
    mainLayout->addWidget(m_tabWidget);

    // Buttons
    QHBoxLayout *buttonLayout = new QHBoxLayout();

    m_defaultsButton = new QPushButton(tr("Restore Defaults"), this);
    connect(m_defaultsButton, &QPushButton::clicked, this, &SettingsDialog::onDefaultsClicked);
    buttonLayout->addWidget(m_defaultsButton);

    buttonLayout->addStretch();

    m_okButton = new QPushButton(tr("OK"), this);
    m_okButton->setDefault(true);
    connect(m_okButton, &QPushButton::clicked, this, &SettingsDialog::accept);
    buttonLayout->addWidget(m_okButton);

    m_applyButton = new QPushButton(tr("Apply"), this);
    connect(m_applyButton, &QPushButton::clicked, this, &SettingsDialog::onApplyClicked);
    buttonLayout->addWidget(m_applyButton);

    m_cancelButton = new QPushButton(tr("Cancel"), this);
    connect(m_cancelButton, &QPushButton::clicked, this, &SettingsDialog::reject);
    buttonLayout->addWidget(m_cancelButton);

    mainLayout->addLayout(buttonLayout);

    setLayout(mainLayout);
}

void SettingsDialog::createGeneralTab()
{
    QWidget *tab = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(tab);

    // Language group
    QGroupBox *langGroup = new QGroupBox(tr("Language"), tab);
    QFormLayout *langLayout = new QFormLayout(langGroup);

    m_languageCombo = new QComboBox(tab);
    m_languageCombo->addItem(tr("System Default"), "");
    m_languageCombo->addItem("English", "en");
    m_languageCombo->addItem("简体中文", "zh_CN");
    m_languageCombo->addItem("繁體中文", "zh_TW");
    langLayout->addRow(tr("Language:"), m_languageCombo);

    layout->addWidget(langGroup);

    // Display group
    QGroupBox *displayGroup = new QGroupBox(tr("Display"), tab);
    QFormLayout *displayLayout = new QFormLayout(displayGroup);

    m_useBinaryUnitsCheck = new QCheckBox(tr("Use binary units (KiB, MiB) instead of SI (KB, MB)"), tab);
    displayLayout->addRow(m_useBinaryUnitsCheck);

    m_showNotificationsCheck = new QCheckBox(tr("Show system notifications"), tab);
    displayLayout->addRow(m_showNotificationsCheck);

    layout->addWidget(displayGroup);

    // Paths group
    QGroupBox *pathsGroup = new QGroupBox(tr("Paths"), tab);
    QFormLayout *pathsLayout = new QFormLayout(pathsGroup);

    QHBoxLayout *indexPathLayout = new QHBoxLayout();
    m_defaultIndexPath = new QLineEdit(tab);
    indexPathLayout->addWidget(m_defaultIndexPath);
    QPushButton *browseIndexBtn = new QPushButton(tr("Browse..."), tab);
    connect(browseIndexBtn, &QPushButton::clicked, this, &SettingsDialog::onBrowseIndexPath);
    indexPathLayout->addWidget(browseIndexBtn);
    pathsLayout->addRow(tr("Default Index Path:"), indexPathLayout);

    QHBoxLayout *logPathLayout = new QHBoxLayout();
    m_logPath = new QLineEdit(tab);
    logPathLayout->addWidget(m_logPath);
    QPushButton *browseLogBtn = new QPushButton(tr("Browse..."), tab);
    connect(browseLogBtn, &QPushButton::clicked, this, &SettingsDialog::onBrowseLogPath);
    logPathLayout->addWidget(browseLogBtn);
    pathsLayout->addRow(tr("Log Path:"), logPathLayout);

    m_logEnabledCheck = new QCheckBox(tr("Enable logging"), tab);
    pathsLayout->addRow(m_logEnabledCheck);

    layout->addWidget(pathsGroup);

    layout->addStretch();
    m_tabWidget->addTab(tab, tr("General"));
}

void SettingsDialog::createWriteTab()
{
    QWidget *tab = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(tab);

    // Block settings group
    QGroupBox *blockGroup = new QGroupBox(tr("Block Settings"), tab);
    QFormLayout *blockLayout = new QFormLayout(blockGroup);

    m_blockSizeCombo = new QComboBox(tab);
    m_blockSizeCombo->addItem("512 KiB (Default)", 524288);
    m_blockSizeCombo->addItem("256 KiB", 262144);
    m_blockSizeCombo->addItem("1 MiB", 1048576);
    blockLayout->addRow(tr("Block Size:"), m_blockSizeCombo);

    m_compressCheck = new QCheckBox(tr("Enable hardware compression"), tab);
    blockLayout->addRow(m_compressCheck);

    layout->addWidget(blockGroup);

    // Hash settings group
    QGroupBox *hashGroup = new QGroupBox(tr("Hash Verification"), tab);
    QFormLayout *hashLayout = new QFormLayout(hashGroup);

    m_hashOnWriteCheck = new QCheckBox(tr("Calculate hash on write"), tab);
    hashLayout->addRow(m_hashOnWriteCheck);

    m_hashAlgorithmCombo = new QComboBox(tab);
    m_hashAlgorithmCombo->addItem("BLAKE3 (Fastest)", "blake3");
    m_hashAlgorithmCombo->addItem("SHA-256", "sha256");
    m_hashAlgorithmCombo->addItem("SHA-1", "sha1");
    m_hashAlgorithmCombo->addItem("MD5", "md5");
    hashLayout->addRow(tr("Hash Algorithm:"), m_hashAlgorithmCombo);

    m_asyncHashCheck = new QCheckBox(tr("Async hash calculation (higher CPU usage)"), tab);
    hashLayout->addRow(m_asyncHashCheck);

    layout->addWidget(hashGroup);

    // Index settings group
    QGroupBox *indexGroup = new QGroupBox(tr("Index Settings"), tab);
    QFormLayout *indexLayout = new QFormLayout(indexGroup);

    m_indexIntervalSpin = new QSpinBox(tab);
    m_indexIntervalSpin->setRange(0, 100);
    m_indexIntervalSpin->setSuffix(" GiB");
    m_indexIntervalSpin->setSpecialValueText(tr("Disabled"));
    indexLayout->addRow(tr("Index Write Interval:"), m_indexIntervalSpin);

    layout->addWidget(indexGroup);

    // File handling group
    QGroupBox *fileGroup = new QGroupBox(tr("File Handling"), tab);
    QFormLayout *fileLayout = new QFormLayout(fileGroup);

    m_overwriteExistingCheck = new QCheckBox(tr("Overwrite existing files"), tab);
    fileLayout->addRow(m_overwriteExistingCheck);

    m_skipSymlinksCheck = new QCheckBox(tr("Skip symbolic links"), tab);
    fileLayout->addRow(m_skipSymlinksCheck);

    layout->addWidget(fileGroup);

    layout->addStretch();
    m_tabWidget->addTab(tab, tr("Write"));
}

void SettingsDialog::createDeviceTab()
{
    QWidget *tab = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(tab);

    // Timeout settings group
    QGroupBox *timeoutGroup = new QGroupBox(tr("Timeouts"), tab);
    QFormLayout *timeoutLayout = new QFormLayout(timeoutGroup);

    m_commandTimeoutSpin = new QSpinBox(tab);
    m_commandTimeoutSpin->setRange(10, 3600);
    m_commandTimeoutSpin->setSuffix(tr(" seconds"));
    timeoutLayout->addRow(tr("Command Timeout:"), m_commandTimeoutSpin);

    m_retryCountSpin = new QSpinBox(tab);
    m_retryCountSpin->setRange(0, 10);
    timeoutLayout->addRow(tr("Retry Count:"), m_retryCountSpin);

    layout->addWidget(timeoutGroup);

    // Refresh settings group
    QGroupBox *refreshGroup = new QGroupBox(tr("Refresh"), tab);
    QFormLayout *refreshLayout = new QFormLayout(refreshGroup);

    m_autoRefreshSpin = new QSpinBox(tab);
    m_autoRefreshSpin->setRange(0, 300);
    m_autoRefreshSpin->setSuffix(tr(" seconds"));
    m_autoRefreshSpin->setSpecialValueText(tr("Disabled"));
    refreshLayout->addRow(tr("Device Auto-Refresh:"), m_autoRefreshSpin);

    m_capacityRefreshSpin = new QSpinBox(tab);
    m_capacityRefreshSpin->setRange(0, 300);
    m_capacityRefreshSpin->setSuffix(tr(" seconds"));
    m_capacityRefreshSpin->setSpecialValueText(tr("Disabled"));
    refreshLayout->addRow(tr("Capacity Refresh:"), m_capacityRefreshSpin);

    layout->addWidget(refreshGroup);

    // Advanced device settings
    QGroupBox *advDevGroup = new QGroupBox(tr("Advanced"), tab);
    QFormLayout *advDevLayout = new QFormLayout(advDevGroup);

    m_disablePartitionCheck = new QCheckBox(tr("Disable partition support"), tab);
    advDevLayout->addRow(m_disablePartitionCheck);

    m_cleanCycleSpin = new QSpinBox(tab);
    m_cleanCycleSpin->setRange(0, 100);
    m_cleanCycleSpin->setSpecialValueText(tr("Disabled"));
    advDevLayout->addRow(tr("Clean Cycle:"), m_cleanCycleSpin);

    layout->addWidget(advDevGroup);

    layout->addStretch();
    m_tabWidget->addTab(tab, tr("Device"));
}

void SettingsDialog::createAdvancedTab()
{
    QWidget *tab = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(tab);

    // Buffer settings group
    QGroupBox *bufferGroup = new QGroupBox(tr("Buffer Settings"), tab);
    QFormLayout *bufferLayout = new QFormLayout(bufferGroup);

    m_preloadFileCountSpin = new QSpinBox(tab);
    m_preloadFileCountSpin->setRange(1, 100);
    bufferLayout->addRow(tr("Preload File Count:"), m_preloadFileCountSpin);

    m_preloadBytesSpin = new QSpinBox(tab);
    m_preloadBytesSpin->setRange(1, 1024);
    m_preloadBytesSpin->setSuffix(" MiB");
    bufferLayout->addRow(tr("Preload Buffer Size:"), m_preloadBytesSpin);

    layout->addWidget(bufferGroup);

    // Performance settings
    QGroupBox *perfGroup = new QGroupBox(tr("Performance"), tab);
    QFormLayout *perfLayout = new QFormLayout(perfGroup);

    m_speedLimitSpin = new QSpinBox(tab);
    m_speedLimitSpin->setRange(0, 1000);
    m_speedLimitSpin->setSuffix(" MiB/s");
    m_speedLimitSpin->setSpecialValueText(tr("No Limit"));
    perfLayout->addRow(tr("Speed Limit:"), m_speedLimitSpin);

    m_threadCountSpin = new QSpinBox(tab);
    m_threadCountSpin->setRange(1, 16);
    perfLayout->addRow(tr("Worker Threads:"), m_threadCountSpin);

    layout->addWidget(perfGroup);

    // Debug settings
    QGroupBox *debugGroup = new QGroupBox(tr("Debugging"), tab);
    QFormLayout *debugLayout = new QFormLayout(debugGroup);

    m_debugModeCheck = new QCheckBox(tr("Enable debug mode"), tab);
    debugLayout->addRow(m_debugModeCheck);

    layout->addWidget(debugGroup);

    layout->addStretch();
    m_tabWidget->addTab(tab, tr("Advanced"));
}

void SettingsDialog::loadSettings()
{
    QSettings settings;

    // General
    settings.beginGroup("General");
    QString lang = settings.value("Language", "").toString();
    int langIndex = m_languageCombo->findData(lang);
    if (langIndex >= 0) m_languageCombo->setCurrentIndex(langIndex);
    m_useBinaryUnitsCheck->setChecked(settings.value("UseBinaryUnits", true).toBool());
    m_showNotificationsCheck->setChecked(settings.value("ShowNotifications", true).toBool());
    m_defaultIndexPath->setText(settings.value("DefaultIndexPath", "").toString());
    m_logPath->setText(settings.value("LogPath", "log").toString());
    m_logEnabledCheck->setChecked(settings.value("LogEnabled", true).toBool());
    settings.endGroup();

    // Write
    settings.beginGroup("Write");
    int blockSize = settings.value("BlockSize", 524288).toInt();
    int blockIndex = m_blockSizeCombo->findData(blockSize);
    if (blockIndex >= 0) m_blockSizeCombo->setCurrentIndex(blockIndex);
    m_compressCheck->setChecked(settings.value("Compression", true).toBool());
    m_hashOnWriteCheck->setChecked(settings.value("HashOnWrite", true).toBool());
    QString hashAlg = settings.value("HashAlgorithm", "blake3").toString();
    int hashIndex = m_hashAlgorithmCombo->findData(hashAlg);
    if (hashIndex >= 0) m_hashAlgorithmCombo->setCurrentIndex(hashIndex);
    m_asyncHashCheck->setChecked(settings.value("AsyncHash", false).toBool());
    m_indexIntervalSpin->setValue(settings.value("IndexInterval", 36).toInt());
    m_overwriteExistingCheck->setChecked(settings.value("OverwriteExisting", false).toBool());
    m_skipSymlinksCheck->setChecked(settings.value("SkipSymlinks", true).toBool());
    settings.endGroup();

    // Device
    settings.beginGroup("Device");
    m_commandTimeoutSpin->setValue(settings.value("CommandTimeout", 300).toInt());
    m_retryCountSpin->setValue(settings.value("RetryCount", 3).toInt());
    m_autoRefreshSpin->setValue(settings.value("AutoRefresh", 30).toInt());
    m_capacityRefreshSpin->setValue(settings.value("CapacityRefresh", 30).toInt());
    m_disablePartitionCheck->setChecked(settings.value("DisablePartition", false).toBool());
    m_cleanCycleSpin->setValue(settings.value("CleanCycle", 3).toInt());
    settings.endGroup();

    // Advanced
    settings.beginGroup("Advanced");
    m_preloadFileCountSpin->setValue(settings.value("PreloadFileCount", 5).toInt());
    m_preloadBytesSpin->setValue(settings.value("PreloadBytes", 32).toInt());
    m_speedLimitSpin->setValue(settings.value("SpeedLimit", 0).toInt());
    m_threadCountSpin->setValue(settings.value("ThreadCount", 4).toInt());
    m_debugModeCheck->setChecked(settings.value("DebugMode", false).toBool());
    settings.endGroup();
}

void SettingsDialog::saveSettings()
{
    QSettings settings;

    // General
    settings.beginGroup("General");
    settings.setValue("Language", m_languageCombo->currentData().toString());
    settings.setValue("UseBinaryUnits", m_useBinaryUnitsCheck->isChecked());
    settings.setValue("ShowNotifications", m_showNotificationsCheck->isChecked());
    settings.setValue("DefaultIndexPath", m_defaultIndexPath->text());
    settings.setValue("LogPath", m_logPath->text());
    settings.setValue("LogEnabled", m_logEnabledCheck->isChecked());
    settings.endGroup();

    // Write
    settings.beginGroup("Write");
    settings.setValue("BlockSize", m_blockSizeCombo->currentData().toInt());
    settings.setValue("Compression", m_compressCheck->isChecked());
    settings.setValue("HashOnWrite", m_hashOnWriteCheck->isChecked());
    settings.setValue("HashAlgorithm", m_hashAlgorithmCombo->currentData().toString());
    settings.setValue("AsyncHash", m_asyncHashCheck->isChecked());
    settings.setValue("IndexInterval", m_indexIntervalSpin->value());
    settings.setValue("OverwriteExisting", m_overwriteExistingCheck->isChecked());
    settings.setValue("SkipSymlinks", m_skipSymlinksCheck->isChecked());
    settings.endGroup();

    // Device
    settings.beginGroup("Device");
    settings.setValue("CommandTimeout", m_commandTimeoutSpin->value());
    settings.setValue("RetryCount", m_retryCountSpin->value());
    settings.setValue("AutoRefresh", m_autoRefreshSpin->value());
    settings.setValue("CapacityRefresh", m_capacityRefreshSpin->value());
    settings.setValue("DisablePartition", m_disablePartitionCheck->isChecked());
    settings.setValue("CleanCycle", m_cleanCycleSpin->value());
    settings.endGroup();

    // Advanced
    settings.beginGroup("Advanced");
    settings.setValue("PreloadFileCount", m_preloadFileCountSpin->value());
    settings.setValue("PreloadBytes", m_preloadBytesSpin->value());
    settings.setValue("SpeedLimit", m_speedLimitSpin->value());
    settings.setValue("ThreadCount", m_threadCountSpin->value());
    settings.setValue("DebugMode", m_debugModeCheck->isChecked());
    settings.endGroup();
}

void SettingsDialog::accept()
{
    saveSettings();
    QDialog::accept();
}

void SettingsDialog::reject()
{
    QDialog::reject();
}

void SettingsDialog::onApplyClicked()
{
    saveSettings();
}

void SettingsDialog::onDefaultsClicked()
{
    setDefaults();
}

void SettingsDialog::setDefaults()
{
    // General
    m_languageCombo->setCurrentIndex(0);
    m_useBinaryUnitsCheck->setChecked(true);
    m_showNotificationsCheck->setChecked(true);
    m_defaultIndexPath->clear();
    m_logPath->setText("log");
    m_logEnabledCheck->setChecked(true);

    // Write
    m_blockSizeCombo->setCurrentIndex(0);
    m_compressCheck->setChecked(true);
    m_hashOnWriteCheck->setChecked(true);
    m_hashAlgorithmCombo->setCurrentIndex(0);
    m_asyncHashCheck->setChecked(false);
    m_indexIntervalSpin->setValue(36);
    m_overwriteExistingCheck->setChecked(false);
    m_skipSymlinksCheck->setChecked(true);

    // Device
    m_commandTimeoutSpin->setValue(300);
    m_retryCountSpin->setValue(3);
    m_autoRefreshSpin->setValue(30);
    m_capacityRefreshSpin->setValue(30);
    m_disablePartitionCheck->setChecked(false);
    m_cleanCycleSpin->setValue(3);

    // Advanced
    m_preloadFileCountSpin->setValue(5);
    m_preloadBytesSpin->setValue(32);
    m_speedLimitSpin->setValue(0);
    m_threadCountSpin->setValue(4);
    m_debugModeCheck->setChecked(false);
}

void SettingsDialog::onBrowseLogPath()
{
    QString dir = QFileDialog::getExistingDirectory(this, tr("Select Log Directory"),
                                                     m_logPath->text());
    if (!dir.isEmpty()) {
        m_logPath->setText(dir);
    }
}

void SettingsDialog::onBrowseIndexPath()
{
    QString dir = QFileDialog::getExistingDirectory(this, tr("Select Default Index Directory"),
                                                     m_defaultIndexPath->text());
    if (!dir.isEmpty()) {
        m_defaultIndexPath->setText(dir);
    }
}

} // namespace app
} // namespace qltfs
