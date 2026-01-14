/*
 * QLTOTapeMan - Qt-based LTO Tape Manager
 *
 * Copyright (c) 2026 Jeffrey ZHU (zhxsh1225@gmail.com)
 * https://github.com/Gypsop/QLTOTapeMan
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef PROGRESSDIALOG_H
#define PROGRESSDIALOG_H

#include <QDialog>
#include <QProgressBar>
#include <QLabel>
#include <QPushButton>
#include <QElapsedTimer>
#include <QTimer>

namespace qltfs {
namespace app {

/**
 * @brief ProgressDialog - Dialog for showing operation progress
 *
 * A progress dialog that supports:
 * - Overall progress with percentage
 * - Current file/operation name
 * - Elapsed and estimated time
 * - Speed display
 * - Cancel button with confirmation
 */
class ProgressDialog : public QDialog
{
    Q_OBJECT

    Q_PROPERTY(QString title READ title WRITE setTitle)
    Q_PROPERTY(QString currentItem READ currentItem WRITE setCurrentItem)
    Q_PROPERTY(int progress READ progress WRITE setProgress NOTIFY progressChanged)
    Q_PROPERTY(qint64 bytesProcessed READ bytesProcessed WRITE setBytesProcessed)
    Q_PROPERTY(qint64 bytesTotal READ bytesTotal WRITE setBytesTotal)
    Q_PROPERTY(bool cancelable READ isCancelable WRITE setCancelable)

public:
    explicit ProgressDialog(QWidget *parent = nullptr);
    explicit ProgressDialog(const QString &title, QWidget *parent = nullptr);
    virtual ~ProgressDialog() = default;

    // Properties
    QString title() const;
    void setTitle(const QString &title);

    QString currentItem() const;
    void setCurrentItem(const QString &item);

    int progress() const;
    void setProgress(int value);

    qint64 bytesProcessed() const { return m_bytesProcessed; }
    void setBytesProcessed(qint64 bytes);

    qint64 bytesTotal() const { return m_bytesTotal; }
    void setBytesTotal(qint64 bytes);

    bool isCancelable() const { return m_cancelable; }
    void setCancelable(bool cancelable);

    /**
     * @brief Check if cancel was requested
     */
    bool wasCanceled() const { return m_canceled; }

    /**
     * @brief Set indeterminate mode (unknown total)
     */
    void setIndeterminate(bool indeterminate);

    /**
     * @brief Set status message
     */
    void setStatus(const QString &status);

    /**
     * @brief Reset the dialog for reuse
     */
    void reset();

signals:
    void progressChanged(int value);
    void canceled();

public slots:
    /**
     * @brief Request cancel (will emit canceled signal after confirmation)
     */
    void cancel();

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onCancelClicked();
    void updateDisplay();

private:
    void setupUi();
    QString formatTime(qint64 seconds) const;
    QString formatSize(qint64 bytes) const;
    QString formatSpeed(double bytesPerSec) const;

    // UI Components
    QLabel *m_titleLabel;
    QLabel *m_itemLabel;
    QLabel *m_statusLabel;
    QLabel *m_progressLabel;
    QLabel *m_timeLabel;
    QLabel *m_speedLabel;
    QProgressBar *m_progressBar;
    QPushButton *m_cancelButton;

    // State
    bool m_canceled;
    bool m_cancelable;
    bool m_indeterminate;
    int m_progress;
    qint64 m_bytesProcessed;
    qint64 m_bytesTotal;

    // Timing
    QElapsedTimer m_elapsedTimer;
    QTimer *m_updateTimer;
    qint64 m_lastBytes;
    qint64 m_lastTime;
    double m_currentSpeed;
};

} // namespace app
} // namespace qltfs

#endif // PROGRESSDIALOG_H
