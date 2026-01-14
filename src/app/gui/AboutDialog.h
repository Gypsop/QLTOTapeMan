/*
 * QLTOTapeMan - Qt-based LTO Tape Manager
 *
 * Copyright (c) 2026 Jeffrey ZHU (zhxsh1225@gmail.com)
 * https://github.com/Gypsop/QLTOTapeMan
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ABOUTDIALOG_H
#define ABOUTDIALOG_H

#include <QDialog>
#include <QLabel>
#include <QPushButton>
#include <QTabWidget>
#include <QTextEdit>

namespace qltfs {
namespace app {

/**
 * @brief AboutDialog - Application about dialog
 *
 * Displays:
 * - Application name and version
 * - Copyright information
 * - License information
 * - Credits and acknowledgments
 * - System information
 */
class AboutDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AboutDialog(QWidget *parent = nullptr);
    virtual ~AboutDialog() = default;

private:
    void setupUi();
    void createAboutTab();
    void createLicenseTab();
    void createCreditsTab();
    void createSystemTab();

    QString getSystemInfo() const;

    QTabWidget *m_tabWidget;
    QLabel *m_logoLabel;
    QLabel *m_titleLabel;
    QLabel *m_versionLabel;
    QLabel *m_copyrightLabel;
    QTextEdit *m_licenseText;
    QTextEdit *m_creditsText;
    QTextEdit *m_systemText;
    QPushButton *m_closeButton;
};

} // namespace app
} // namespace qltfs

#endif // ABOUTDIALOG_H
