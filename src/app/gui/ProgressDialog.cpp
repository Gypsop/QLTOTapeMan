/*
 * QLTOTapeMan - Qt-based LTO Tape Manager
 *
 * Copyright (c) 2026 Jeffrey ZHU (zhxsh1225@gmail.com)
 * https://github.com/Gypsop/QLTOTapeMan
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ProgressDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QCloseEvent>
#include <QMessageBox>

namespace qltfs {
namespace app {

ProgressDialog::ProgressDialog(QWidget *parent)
    : QDialog(parent)
    , m_canceled(false)
    , m_cancelable(true)
    , m_indeterminate(false)
    , m_progress(0)
    , m_bytesProcessed(0)
    , m_bytesTotal(0)
    , m_lastBytes(0)
    , m_lastTime(0)
    , m_currentSpeed(0)
{
    setupUi();
}

ProgressDialog::ProgressDialog(const QString &title, QWidget *parent)
    : ProgressDialog(parent)
{
    setTitle(title);
}

void ProgressDialog::setupUi()
{
    setWindowTitle(tr("Progress"));
    setMinimumWidth(450);
    setModal(true);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(10);

    // Title
    m_titleLabel = new QLabel(tr("Operation in progress..."), this);
    m_titleLabel->setStyleSheet("font-weight: bold; font-size: 12pt;");
    mainLayout->addWidget(m_titleLabel);

    // Current item
    m_itemLabel = new QLabel(this);
    m_itemLabel->setWordWrap(true);
    m_itemLabel->setMinimumHeight(40);
    mainLayout->addWidget(m_itemLabel);

    // Progress bar
    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setTextVisible(true);
    mainLayout->addWidget(m_progressBar);

    // Status line: progress % | bytes processed
    QHBoxLayout *statusLayout = new QHBoxLayout();
    
    m_progressLabel = new QLabel("0%", this);
    statusLayout->addWidget(m_progressLabel);

    statusLayout->addStretch();

    m_statusLabel = new QLabel(this);
    statusLayout->addWidget(m_statusLabel);

    mainLayout->addLayout(statusLayout);

    // Time and speed line
    QHBoxLayout *timeLayout = new QHBoxLayout();

    m_timeLabel = new QLabel(tr("Elapsed: 00:00"), this);
    timeLayout->addWidget(m_timeLabel);

    timeLayout->addStretch();

    m_speedLabel = new QLabel(this);
    timeLayout->addWidget(m_speedLabel);

    mainLayout->addLayout(timeLayout);

    mainLayout->addStretch();

    // Cancel button
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();

    m_cancelButton = new QPushButton(tr("Cancel"), this);
    connect(m_cancelButton, &QPushButton::clicked, this, &ProgressDialog::onCancelClicked);
    buttonLayout->addWidget(m_cancelButton);

    mainLayout->addLayout(buttonLayout);

    setLayout(mainLayout);

    // Update timer
    m_updateTimer = new QTimer(this);
    m_updateTimer->setInterval(250);  // 4 Hz update
    connect(m_updateTimer, &QTimer::timeout, this, &ProgressDialog::updateDisplay);
    m_updateTimer->start();

    m_elapsedTimer.start();
}

QString ProgressDialog::title() const
{
    return m_titleLabel->text();
}

void ProgressDialog::setTitle(const QString &title)
{
    m_titleLabel->setText(title);
    setWindowTitle(title);
}

QString ProgressDialog::currentItem() const
{
    return m_itemLabel->text();
}

void ProgressDialog::setCurrentItem(const QString &item)
{
    m_itemLabel->setText(item);
}

int ProgressDialog::progress() const
{
    return m_progress;
}

void ProgressDialog::setProgress(int value)
{
    value = qBound(0, value, 100);
    if (m_progress != value) {
        m_progress = value;
        m_progressBar->setValue(value);
        m_progressLabel->setText(QString("%1%").arg(value));
        emit progressChanged(value);
    }
}

void ProgressDialog::setBytesProcessed(qint64 bytes)
{
    m_bytesProcessed = bytes;
    
    // Calculate progress if total is known
    if (m_bytesTotal > 0) {
        int percent = static_cast<int>(bytes * 100 / m_bytesTotal);
        setProgress(percent);
    }

    updateDisplay();
}

void ProgressDialog::setBytesTotal(qint64 bytes)
{
    m_bytesTotal = bytes;
    setIndeterminate(bytes <= 0);
}

void ProgressDialog::setCancelable(bool cancelable)
{
    m_cancelable = cancelable;
    m_cancelButton->setEnabled(cancelable);
}

void ProgressDialog::setIndeterminate(bool indeterminate)
{
    m_indeterminate = indeterminate;
    if (indeterminate) {
        m_progressBar->setRange(0, 0);  // Indeterminate
    } else {
        m_progressBar->setRange(0, 100);
    }
}

void ProgressDialog::setStatus(const QString &status)
{
    m_statusLabel->setText(status);
}

void ProgressDialog::reset()
{
    m_canceled = false;
    m_progress = 0;
    m_bytesProcessed = 0;
    m_bytesTotal = 0;
    m_lastBytes = 0;
    m_lastTime = 0;
    m_currentSpeed = 0;

    m_progressBar->setValue(0);
    m_progressLabel->setText("0%");
    m_itemLabel->clear();
    m_statusLabel->clear();
    m_timeLabel->setText(tr("Elapsed: 00:00"));
    m_speedLabel->clear();

    m_elapsedTimer.restart();
    m_cancelButton->setEnabled(m_cancelable);
}

void ProgressDialog::cancel()
{
    if (!m_cancelable || m_canceled) {
        return;
    }

    QMessageBox::StandardButton result = QMessageBox::question(
        this, tr("Cancel Operation"),
        tr("Are you sure you want to cancel the current operation?"),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);

    if (result == QMessageBox::Yes) {
        m_canceled = true;
        m_cancelButton->setEnabled(false);
        m_cancelButton->setText(tr("Canceling..."));
        emit canceled();
    }
}

void ProgressDialog::closeEvent(QCloseEvent *event)
{
    if (m_cancelable && !m_canceled) {
        cancel();
        if (!m_canceled) {
            event->ignore();
            return;
        }
    }
    event->accept();
}

void ProgressDialog::onCancelClicked()
{
    cancel();
}

void ProgressDialog::updateDisplay()
{
    qint64 elapsedMs = m_elapsedTimer.elapsed();
    qint64 elapsedSec = elapsedMs / 1000;

    // Update elapsed time
    m_timeLabel->setText(tr("Elapsed: %1").arg(formatTime(elapsedSec)));

    // Calculate speed
    qint64 deltaBytes = m_bytesProcessed - m_lastBytes;
    qint64 deltaTime = elapsedMs - m_lastTime;

    if (deltaTime >= 500 && deltaBytes >= 0) {  // Update speed every 500ms
        m_currentSpeed = static_cast<double>(deltaBytes) * 1000.0 / deltaTime;
        m_lastBytes = m_bytesProcessed;
        m_lastTime = elapsedMs;
    }

    // Update speed display
    if (m_currentSpeed > 0) {
        m_speedLabel->setText(formatSpeed(m_currentSpeed));
    }

    // Update status (bytes processed / total)
    if (m_bytesTotal > 0) {
        setStatus(QString("%1 / %2")
            .arg(formatSize(m_bytesProcessed))
            .arg(formatSize(m_bytesTotal)));

        // Calculate ETA
        if (m_currentSpeed > 0 && m_bytesProcessed > 0) {
            qint64 remaining = m_bytesTotal - m_bytesProcessed;
            qint64 etaSec = static_cast<qint64>(remaining / m_currentSpeed);
            m_timeLabel->setText(tr("Elapsed: %1 | ETA: %2")
                .arg(formatTime(elapsedSec))
                .arg(formatTime(etaSec)));
        }
    } else if (m_bytesProcessed > 0) {
        setStatus(formatSize(m_bytesProcessed));
    }
}

QString ProgressDialog::formatTime(qint64 seconds) const
{
    if (seconds < 0) {
        return "--:--";
    }

    qint64 hours = seconds / 3600;
    qint64 minutes = (seconds % 3600) / 60;
    qint64 secs = seconds % 60;

    if (hours > 0) {
        return QString("%1:%2:%3")
            .arg(hours)
            .arg(minutes, 2, 10, QChar('0'))
            .arg(secs, 2, 10, QChar('0'));
    } else {
        return QString("%1:%2")
            .arg(minutes, 2, 10, QChar('0'))
            .arg(secs, 2, 10, QChar('0'));
    }
}

QString ProgressDialog::formatSize(qint64 bytes) const
{
    const char *units[] = {"B", "KiB", "MiB", "GiB", "TiB"};
    int unitIndex = 0;
    double size = bytes;

    while (size >= 1024.0 && unitIndex < 4) {
        size /= 1024.0;
        unitIndex++;
    }

    return QString("%1 %2").arg(size, 0, 'f', (unitIndex == 0) ? 0 : 2).arg(units[unitIndex]);
}

QString ProgressDialog::formatSpeed(double bytesPerSec) const
{
    return formatSize(static_cast<qint64>(bytesPerSec)) + "/s";
}

} // namespace app
} // namespace qltfs
