/*
 * QLTOTapeMan - Qt-based LTO Tape Manager
 *
 * Copyright (c) 2026 Jeffrey ZHU (zhxsh1225@gmail.com)
 * https://github.com/Gypsop/QLTOTapeMan
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>
#include <QTabWidget>
#include <QLineEdit>
#include <QSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QPushButton>
#include <QSettings>

namespace qltfs {
namespace app {

/**
 * @brief SettingsDialog - Application settings dialog
 *
 * Provides configuration for:
 * - General settings (language, units, paths)
 * - Write settings (block size, compression, hash)
 * - Device settings (timeouts, retries)
 * - Advanced settings (buffer sizes, threads)
 */
class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget *parent = nullptr);
    virtual ~SettingsDialog() = default;

    /**
     * @brief Load settings from QSettings
     */
    void loadSettings();

    /**
     * @brief Save settings to QSettings
     */
    void saveSettings();

public slots:
    void accept() override;
    void reject() override;

private slots:
    void onApplyClicked();
    void onDefaultsClicked();
    void onBrowseLogPath();
    void onBrowseIndexPath();

private:
    void setupUi();
    void createGeneralTab();
    void createWriteTab();
    void createDeviceTab();
    void createAdvancedTab();
    void setDefaults();

    // UI Components
    QTabWidget *m_tabWidget;
    QPushButton *m_okButton;
    QPushButton *m_cancelButton;
    QPushButton *m_applyButton;
    QPushButton *m_defaultsButton;

    // General tab
    QComboBox *m_languageCombo;
    QCheckBox *m_useBinaryUnitsCheck;
    QCheckBox *m_showNotificationsCheck;
    QLineEdit *m_defaultIndexPath;
    QLineEdit *m_logPath;
    QCheckBox *m_logEnabledCheck;

    // Write tab
    QComboBox *m_blockSizeCombo;
    QCheckBox *m_compressCheck;
    QCheckBox *m_hashOnWriteCheck;
    QComboBox *m_hashAlgorithmCombo;
    QCheckBox *m_asyncHashCheck;
    QSpinBox *m_indexIntervalSpin;
    QCheckBox *m_overwriteExistingCheck;
    QCheckBox *m_skipSymlinksCheck;

    // Device tab
    QSpinBox *m_commandTimeoutSpin;
    QSpinBox *m_retryCountSpin;
    QSpinBox *m_autoRefreshSpin;
    QCheckBox *m_disablePartitionCheck;
    QSpinBox *m_cleanCycleSpin;
    QSpinBox *m_capacityRefreshSpin;

    // Advanced tab
    QSpinBox *m_preloadFileCountSpin;
    QSpinBox *m_preloadBytesSpin;
    QSpinBox *m_speedLimitSpin;
    QSpinBox *m_threadCountSpin;
    QCheckBox *m_debugModeCheck;
};

} // namespace app
} // namespace qltfs

#endif // SETTINGSDIALOG_H
