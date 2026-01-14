/*
 * QLTOTapeMan - Qt-based LTO Tape Manager
 * libqltfs - LTFS Core Library
 *
 * Copyright (c) 2026 Jeffrey ZHU (zhxsh1225@gmail.com)
 * https://github.com/Gypsop/QLTOTapeMan
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include "libqltfs_global.h"
#include "core/LtfsTypes.h"
#include "core/LtfsIndex.h"
#include "device/TapeDevice.h"
#include "io/HashCalculator.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QFile>
#include <QDir>
#include <QQueue>
#include <QMutex>
#include <functional>

namespace qltfs {

/**
 * @brief File transfer operation type
 */
enum class TransferType {
    Write,      ///< Write files to tape
    Read,       ///< Read files from tape
    Verify      ///< Verify files on tape
};

/**
 * @brief Transfer status for individual files
 */
enum class TransferStatus {
    Pending,        ///< Waiting to be processed
    InProgress,     ///< Currently being transferred
    Completed,      ///< Successfully completed
    Failed,         ///< Transfer failed
    Skipped,        ///< Skipped (e.g., already exists)
    Cancelled       ///< Cancelled by user
};

/**
 * @brief Information about a file to be transferred
 */
struct LIBQLTFS_EXPORT TransferItem {
    QString sourcePath;         ///< Full path on source (local for write, tape for read)
    QString destPath;           ///< Full path on destination
    QString relativePath;       ///< Path relative to source root
    qint64 size = 0;            ///< File size in bytes
    TransferStatus status = TransferStatus::Pending;
    QString errorMessage;

    // Progress tracking
    qint64 bytesTransferred = 0;

    // Hash verification
    QString sourceHash;
    QString destHash;
    bool hashVerified = false;

    // Metadata
    QDateTime modifiedTime;
    bool isDirectory = false;

    /**
     * @brief Get display name for UI
     */
    QString displayName() const;
};

/**
 * @brief Overall transfer statistics
 */
struct LIBQLTFS_EXPORT TransferStats {
    qint64 totalFiles = 0;
    qint64 totalBytes = 0;
    qint64 completedFiles = 0;
    qint64 completedBytes = 0;
    qint64 failedFiles = 0;
    qint64 skippedFiles = 0;

    // Speed tracking
    qint64 elapsedMs = 0;
    double bytesPerSecond = 0.0;
    qint64 estimatedRemainingMs = 0;

    /**
     * @brief Get progress percentage (0-100)
     */
    int progressPercent() const;

    /**
     * @brief Get human-readable speed string
     */
    QString speedString() const;

    /**
     * @brief Get human-readable ETA string
     */
    QString etaString() const;
};

/**
 * @brief Callback types for transfer operations
 */
using TransferProgressCallback = std::function<void(const TransferItem &item, const TransferStats &stats)>;
using TransferErrorCallback = std::function<bool(const TransferItem &item, const QString &error)>;  // Return true to continue

/**
 * @brief Transfer options
 */
struct LIBQLTFS_EXPORT TransferOptions {
    HashMode hashMode = HashMode::SHA256;   ///< Hash algorithm for verification
    bool verifyAfterWrite = true;           ///< Verify hashes after writing
    bool preserveTimestamps = true;         ///< Preserve file timestamps
    bool skipExisting = false;              ///< Skip files that already exist
    bool overwriteExisting = true;          ///< Overwrite existing files
    bool createDirectories = true;          ///< Create directories as needed
    bool continueOnError = true;            ///< Continue after errors
    int maxRetries = 3;                     ///< Maximum retries per file
    quint32 blockSize = DEFAULT_BLOCK_SIZE; ///< Block size for tape I/O
};

/**
 * @brief High-level tape I/O manager
 *
 * Manages file transfers between local filesystem and LTFS tape,
 * with support for progress tracking, hash verification, and
 * error handling.
 */
class LIBQLTFS_EXPORT TapeIO : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool running READ isRunning NOTIFY runningChanged)
    Q_PROPERTY(bool paused READ isPaused NOTIFY pausedChanged)

public:
    /**
     * @brief Constructor
     * @param device Tape device to use (must be open)
     * @param parent Parent object
     */
    explicit TapeIO(TapeDevice *device, QObject *parent = nullptr);
    ~TapeIO() override;

    // Disable copy
    TapeIO(const TapeIO &) = delete;
    TapeIO &operator=(const TapeIO &) = delete;

    /**
     * @brief Get associated tape device
     */
    TapeDevice *device() const;

    /**
     * @brief Get/set transfer options
     */
    TransferOptions options() const;
    void setOptions(const TransferOptions &options);

    /**
     * @brief Get current LTFS index
     */
    QSharedPointer<LtfsIndex> index() const;

    /**
     * @brief Set LTFS index to use
     */
    void setIndex(QSharedPointer<LtfsIndex> index);

    // === Write Operations ===

    /**
     * @brief Add file to write queue
     * @param sourcePath Local file path
     * @param destPath Destination path on tape (relative to root)
     */
    void addFile(const QString &sourcePath, const QString &destPath = QString());

    /**
     * @brief Add directory to write queue (recursive)
     * @param sourceDir Local directory path
     * @param destDir Destination directory on tape
     */
    void addDirectory(const QString &sourceDir, const QString &destDir = QString());

    /**
     * @brief Add multiple files to write queue
     * @param files List of local file paths
     * @param destDir Common destination directory
     */
    void addFiles(const QStringList &files, const QString &destDir = QString());

    /**
     * @brief Clear pending write queue
     */
    void clearQueue();

    /**
     * @brief Get number of items in queue
     */
    int queueCount() const;

    /**
     * @brief Get total size of queued items
     */
    qint64 queuedBytes() const;

    /**
     * @brief Start writing queued files to tape
     * @return true if started successfully
     */
    bool startWrite();

    // === Read Operations ===

    /**
     * @brief Read file from tape to local path
     * @param tapeFile File entry from index
     * @param destPath Local destination path
     * @return true if successful
     */
    bool readFile(const LtfsFile &tapeFile, const QString &destPath);

    /**
     * @brief Read directory from tape (recursive)
     * @param tapeDir Directory entry from index
     * @param destPath Local destination directory
     * @return true if successful
     */
    bool readDirectory(const LtfsDirectory &tapeDir, const QString &destPath);

    /**
     * @brief Read files by extent info (for direct read without index)
     * @param files List of files with extent info
     * @param destDir Destination directory
     */
    bool readFiles(const QList<LtfsFile> &files, const QString &destDir);

    // === Verification ===

    /**
     * @brief Verify file on tape matches local file
     * @param tapeFile File entry from index
     * @param localPath Local file path
     * @return true if verification passed
     */
    bool verifyFile(const LtfsFile &tapeFile, const QString &localPath);

    /**
     * @brief Calculate and store hash for file
     * @param filePath Path to file
     * @return Hash result
     */
    HashResult calculateHash(const QString &filePath);

    // === Control ===

    /**
     * @brief Check if transfer is in progress
     */
    bool isRunning() const;

    /**
     * @brief Check if transfer is paused
     */
    bool isPaused() const;

    /**
     * @brief Pause current transfer
     */
    void pause();

    /**
     * @brief Resume paused transfer
     */
    void resume();

    /**
     * @brief Cancel current transfer
     */
    void cancel();

    /**
     * @brief Wait for current transfer to complete
     * @param timeoutMs Timeout in milliseconds (-1 = infinite)
     * @return true if completed, false if timeout
     */
    bool waitForCompletion(int timeoutMs = -1);

    // === Statistics ===

    /**
     * @brief Get current transfer statistics
     */
    TransferStats statistics() const;

    /**
     * @brief Get list of transfer items
     */
    QList<TransferItem> items() const;

    /**
     * @brief Get failed items
     */
    QList<TransferItem> failedItems() const;

    /**
     * @brief Get last error message
     */
    QString lastError() const;

signals:
    /**
     * @brief Emitted when transfer starts
     */
    void transferStarted(TransferType type, int fileCount, qint64 totalBytes);

    /**
     * @brief Emitted for each file as it starts
     */
    void fileStarted(const TransferItem &item);

    /**
     * @brief Emitted during file transfer
     */
    void fileProgress(const TransferItem &item, qint64 bytesTransferred, qint64 totalBytes);

    /**
     * @brief Emitted when file completes
     */
    void fileCompleted(const TransferItem &item);

    /**
     * @brief Emitted on file error
     */
    void fileError(const TransferItem &item, const QString &error);

    /**
     * @brief Emitted when all transfers complete
     */
    void transferCompleted(const TransferStats &stats);

    /**
     * @brief Emitted on overall progress change
     */
    void progressChanged(int percent, const TransferStats &stats);

    /**
     * @brief Emitted when running state changes
     */
    void runningChanged(bool running);

    /**
     * @brief Emitted when paused state changes
     */
    void pausedChanged(bool paused);

    /**
     * @brief Emitted on error
     */
    void errorOccurred(const QString &message);

private:
    class Private;
    Private *d;

    bool writeFileToTape(TransferItem &item);
    bool readFileFromTape(TransferItem &item);
    bool verifyFileHash(TransferItem &item);
    void updateStatistics();
    void processQueue();
};

} // namespace qltfs
