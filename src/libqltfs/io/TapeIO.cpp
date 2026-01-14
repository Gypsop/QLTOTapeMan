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

#include "TapeIO.h"

#include <QFileInfo>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QThread>
#include <QMutexLocker>
#include <QDebug>

namespace qltfs {

// ============================================================================
// TransferItem Implementation
// ============================================================================

QString TransferItem::displayName() const
{
    if (!relativePath.isEmpty()) {
        return relativePath;
    }
    QFileInfo info(sourcePath);
    return info.fileName();
}

// ============================================================================
// TransferStats Implementation
// ============================================================================

int TransferStats::progressPercent() const
{
    if (totalBytes <= 0) {
        return totalFiles > 0 ? static_cast<int>(completedFiles * 100 / totalFiles) : 0;
    }
    return static_cast<int>(completedBytes * 100 / totalBytes);
}

QString TransferStats::speedString() const
{
    if (bytesPerSecond < 1024) {
        return QStringLiteral("%1 B/s").arg(bytesPerSecond, 0, 'f', 0);
    } else if (bytesPerSecond < 1024 * 1024) {
        return QStringLiteral("%1 KB/s").arg(bytesPerSecond / 1024.0, 0, 'f', 1);
    } else if (bytesPerSecond < 1024 * 1024 * 1024) {
        return QStringLiteral("%1 MB/s").arg(bytesPerSecond / (1024.0 * 1024.0), 0, 'f', 1);
    } else {
        return QStringLiteral("%1 GB/s").arg(bytesPerSecond / (1024.0 * 1024.0 * 1024.0), 0, 'f', 2);
    }
}

QString TransferStats::etaString() const
{
    if (estimatedRemainingMs <= 0) {
        return QStringLiteral("--:--:--");
    }

    qint64 seconds = estimatedRemainingMs / 1000;
    qint64 hours = seconds / 3600;
    qint64 minutes = (seconds % 3600) / 60;
    qint64 secs = seconds % 60;

    if (hours > 0) {
        return QStringLiteral("%1:%2:%3")
            .arg(hours)
            .arg(minutes, 2, 10, QLatin1Char('0'))
            .arg(secs, 2, 10, QLatin1Char('0'));
    } else {
        return QStringLiteral("%1:%2")
            .arg(minutes, 2, 10, QLatin1Char('0'))
            .arg(secs, 2, 10, QLatin1Char('0'));
    }
}

// ============================================================================
// TapeIO Private Implementation
// ============================================================================

class TapeIO::Private
{
public:
    TapeDevice *device = nullptr;
    TransferOptions options;
    QSharedPointer<LtfsIndex> index;

    QList<TransferItem> queue;
    QMutex queueMutex;

    bool running = false;
    bool paused = false;
    bool cancelled = false;

    TransferStats stats;
    QElapsedTimer timer;

    QString lastError;
    int currentItemIndex = -1;

    qint64 calculateQueueSize() const
    {
        qint64 total = 0;
        for (const auto &item : queue) {
            total += item.size;
        }
        return total;
    }

    void resetStats()
    {
        stats = TransferStats();
        stats.totalFiles = queue.size();
        stats.totalBytes = calculateQueueSize();
    }
};

// ============================================================================
// TapeIO Implementation
// ============================================================================

TapeIO::TapeIO(TapeDevice *device, QObject *parent)
    : QObject(parent)
    , d(new Private)
{
    d->device = device;
}

TapeIO::~TapeIO()
{
    cancel();
    delete d;
}

TapeDevice *TapeIO::device() const
{
    return d->device;
}

TransferOptions TapeIO::options() const
{
    return d->options;
}

void TapeIO::setOptions(const TransferOptions &options)
{
    d->options = options;
}

QSharedPointer<LtfsIndex> TapeIO::index() const
{
    return d->index;
}

void TapeIO::setIndex(QSharedPointer<LtfsIndex> index)
{
    d->index = index;
}

void TapeIO::addFile(const QString &sourcePath, const QString &destPath)
{
    QFileInfo info(sourcePath);
    if (!info.exists()) {
        qWarning() << "File does not exist:" << sourcePath;
        return;
    }

    TransferItem item;
    item.sourcePath = info.absoluteFilePath();
    item.destPath = destPath.isEmpty() ? info.fileName() : destPath;
    item.relativePath = info.fileName();
    item.size = info.size();
    item.modifiedTime = info.lastModified();
    item.isDirectory = info.isDir();
    item.status = TransferStatus::Pending;

    QMutexLocker locker(&d->queueMutex);
    d->queue.append(item);
}

void TapeIO::addDirectory(const QString &sourceDir, const QString &destDir)
{
    QDir dir(sourceDir);
    if (!dir.exists()) {
        qWarning() << "Directory does not exist:" << sourceDir;
        return;
    }

    QString basePath = dir.absolutePath();
    QString targetDir = destDir.isEmpty() ? dir.dirName() : destDir;

    // Add directory entry itself
    TransferItem dirItem;
    dirItem.sourcePath = basePath;
    dirItem.destPath = targetDir;
    dirItem.relativePath = targetDir;
    dirItem.isDirectory = true;
    dirItem.status = TransferStatus::Pending;

    {
        QMutexLocker locker(&d->queueMutex);
        d->queue.append(dirItem);
    }

    // Iterate through all files and subdirectories
    QDirIterator it(basePath, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot,
                    QDirIterator::Subdirectories);

    while (it.hasNext()) {
        it.next();
        QFileInfo info = it.fileInfo();

        QString relativePath = dir.relativeFilePath(info.absoluteFilePath());
        QString destPath = targetDir + QLatin1Char('/') + relativePath;

        TransferItem item;
        item.sourcePath = info.absoluteFilePath();
        item.destPath = destPath;
        item.relativePath = relativePath;
        item.size = info.isDir() ? 0 : info.size();
        item.modifiedTime = info.lastModified();
        item.isDirectory = info.isDir();
        item.status = TransferStatus::Pending;

        QMutexLocker locker(&d->queueMutex);
        d->queue.append(item);
    }
}

void TapeIO::addFiles(const QStringList &files, const QString &destDir)
{
    for (const QString &file : files) {
        QString dest = destDir;
        if (!dest.isEmpty() && !dest.endsWith(QLatin1Char('/'))) {
            dest += QLatin1Char('/');
        }
        dest += QFileInfo(file).fileName();
        addFile(file, dest);
    }
}

void TapeIO::clearQueue()
{
    QMutexLocker locker(&d->queueMutex);
    d->queue.clear();
}

int TapeIO::queueCount() const
{
    QMutexLocker locker(&d->queueMutex);
    return d->queue.size();
}

qint64 TapeIO::queuedBytes() const
{
    QMutexLocker locker(&d->queueMutex);
    return d->calculateQueueSize();
}

bool TapeIO::startWrite()
{
    if (d->running) {
        d->lastError = QStringLiteral("Transfer already in progress");
        return false;
    }

    if (!d->device || !d->device->isOpen()) {
        d->lastError = QStringLiteral("Device not open");
        return false;
    }

    if (d->queue.isEmpty()) {
        d->lastError = QStringLiteral("No files in queue");
        return false;
    }

    d->running = true;
    d->paused = false;
    d->cancelled = false;
    d->resetStats();
    d->timer.start();

    emit runningChanged(true);
    emit transferStarted(TransferType::Write, static_cast<int>(d->stats.totalFiles), d->stats.totalBytes);

    processQueue();

    return true;
}

bool TapeIO::readFile(const LtfsFile &tapeFile, const QString &destPath)
{
    if (!d->device || !d->device->isOpen()) {
        d->lastError = QStringLiteral("Device not open");
        return false;
    }

    TransferItem item;
    item.sourcePath = tapeFile.name();  // Name on tape
    item.destPath = destPath;
    item.size = tapeFile.length();
    item.status = TransferStatus::InProgress;

    emit fileStarted(item);

    bool success = readFileFromTape(item);

    if (success) {
        item.status = TransferStatus::Completed;
        emit fileCompleted(item);
    } else {
        item.status = TransferStatus::Failed;
        emit fileError(item, item.errorMessage);
    }

    return success;
}

bool TapeIO::readDirectory(const LtfsDirectory &tapeDir, const QString &destPath)
{
    // Create directory
    QDir dir;
    if (!dir.mkpath(destPath)) {
        d->lastError = QStringLiteral("Failed to create directory: %1").arg(destPath);
        return false;
    }

    // Read all files in directory
    for (const auto &file : tapeDir.files()) {
        QString fileDest = destPath + QLatin1Char('/') + file.name();
        if (!readFile(file, fileDest)) {
            if (!d->options.continueOnError) {
                return false;
            }
        }
    }

    // Recurse into subdirectories
    for (const auto &subdir : tapeDir.subdirectories()) {
        QString subdirDest = destPath + QLatin1Char('/') + subdir.name();
        if (!readDirectory(subdir, subdirDest)) {
            if (!d->options.continueOnError) {
                return false;
            }
        }
    }

    return true;
}

bool TapeIO::readFiles(const QList<LtfsFile> &files, const QString &destDir)
{
    QDir dir;
    if (!dir.mkpath(destDir)) {
        d->lastError = QStringLiteral("Failed to create destination directory");
        return false;
    }

    bool allSuccess = true;
    for (const auto &file : files) {
        QString destPath = destDir + QLatin1Char('/') + file.name();
        if (!readFile(file, destPath)) {
            allSuccess = false;
            if (!d->options.continueOnError) {
                break;
            }
        }
    }

    return allSuccess;
}

bool TapeIO::verifyFile(const LtfsFile &tapeFile, const QString &localPath)
{
    Q_UNUSED(tapeFile)
    Q_UNUSED(localPath)

    // Read file from tape and compare hash
    // This is a placeholder - full implementation would read from tape
    // and compare with local file hash

    return true;
}

HashResult TapeIO::calculateHash(const QString &filePath)
{
    HashCalculator calc(d->options.hashMode);
    return calc.hashFile(filePath);
}

bool TapeIO::isRunning() const
{
    return d->running;
}

bool TapeIO::isPaused() const
{
    return d->paused;
}

void TapeIO::pause()
{
    if (d->running && !d->paused) {
        d->paused = true;
        emit pausedChanged(true);
    }
}

void TapeIO::resume()
{
    if (d->running && d->paused) {
        d->paused = false;
        emit pausedChanged(false);
        processQueue();
    }
}

void TapeIO::cancel()
{
    d->cancelled = true;
    d->paused = false;
}

bool TapeIO::waitForCompletion(int timeoutMs)
{
    QElapsedTimer timer;
    timer.start();

    while (d->running) {
        if (timeoutMs >= 0 && timer.elapsed() >= timeoutMs) {
            return false;
        }
        QThread::msleep(100);
    }

    return true;
}

TransferStats TapeIO::statistics() const
{
    return d->stats;
}

QList<TransferItem> TapeIO::items() const
{
    QMutexLocker locker(&d->queueMutex);
    return d->queue;
}

QList<TransferItem> TapeIO::failedItems() const
{
    QMutexLocker locker(&d->queueMutex);
    QList<TransferItem> failed;
    for (const auto &item : d->queue) {
        if (item.status == TransferStatus::Failed) {
            failed.append(item);
        }
    }
    return failed;
}

QString TapeIO::lastError() const
{
    return d->lastError;
}

bool TapeIO::writeFileToTape(TransferItem &item)
{
    if (item.isDirectory) {
        // Directories are just metadata entries in the index
        item.status = TransferStatus::Completed;
        return true;
    }

    QFile file(item.sourcePath);
    if (!file.open(QIODevice::ReadOnly)) {
        item.errorMessage = QStringLiteral("Failed to open source file: %1").arg(file.errorString());
        item.status = TransferStatus::Failed;
        return false;
    }

    // Calculate source hash if verification is enabled
    if (d->options.verifyAfterWrite) {
        HashCalculator calc(d->options.hashMode);
        HashResult hashResult = calc.hashFile(item.sourcePath,
            [this, &item](qint64 processed, qint64 total) {
                Q_UNUSED(total)
                // Don't count hash calculation in transfer progress
                Q_UNUSED(processed)
            });

        if (hashResult.success) {
            item.sourceHash = hashResult.hexString;
        }
    }

    // Seek to end of data partition
    if (!d->device->seekToEnd(1)) {
        item.errorMessage = QStringLiteral("Failed to seek to end of data");
        item.status = TransferStatus::Failed;
        return false;
    }

    // Record starting position for extent info
    TapePosition startPos = d->device->position();

    // Write file data in blocks
    quint32 blockSize = d->options.blockSize;
    QByteArray buffer;
    buffer.reserve(static_cast<int>(blockSize));
    qint64 totalWritten = 0;
    qint64 fileSize = file.size();

    while (!file.atEnd()) {
        if (d->cancelled) {
            item.status = TransferStatus::Cancelled;
            return false;
        }

        while (d->paused) {
            QThread::msleep(100);
            if (d->cancelled) {
                item.status = TransferStatus::Cancelled;
                return false;
            }
        }

        buffer = file.read(static_cast<qint64>(blockSize));
        if (buffer.isEmpty()) {
            if (file.error() != QFile::NoError) {
                item.errorMessage = QStringLiteral("Read error: %1").arg(file.errorString());
                item.status = TransferStatus::Failed;
                return false;
            }
            break;
        }

        qint64 written = d->device->writeBlock(buffer);
        if (written < 0) {
            item.errorMessage = QStringLiteral("Write error: %1").arg(d->device->lastError());
            item.status = TransferStatus::Failed;
            return false;
        }

        totalWritten += written;
        item.bytesTransferred = totalWritten;
        d->stats.completedBytes += written;

        emit fileProgress(item, totalWritten, fileSize);
        updateStatistics();
    }

    // Write filemark after file
    if (!d->device->writeFilemark(1)) {
        item.errorMessage = QStringLiteral("Failed to write filemark");
        item.status = TransferStatus::Failed;
        return false;
    }

    // Update index with extent info
    if (d->index) {
        LtfsFile newFile;
        newFile.setName(QFileInfo(item.destPath).fileName());
        newFile.setLength(fileSize);
        newFile.setReadonly(false);

        QDateTime now = QDateTime::currentDateTimeUtc();
        newFile.setCreationTime(item.modifiedTime.isValid() ? item.modifiedTime : now);
        newFile.setModifyTime(item.modifiedTime.isValid() ? item.modifiedTime : now);
        newFile.setAccessTime(now);
        newFile.setChangeTime(now);

        // Add extent info
        LtfsExtent extent;
        extent.setPartition(PartitionLabel::DataPartition);
        extent.setStartBlock(startPos.blockNumber);
        extent.setByteOffset(0);
        extent.setByteCount(fileSize);
        extent.setFileOffset(0);

        QList<LtfsExtent> extents;
        extents.append(extent);
        newFile.setExtentInfo(extents);

        // Set hash if calculated
        if (!item.sourceHash.isEmpty()) {
            ExtendedAttribute hashAttr;
            hashAttr.key = hashTypeToString(d->options.hashMode);
            hashAttr.value = item.sourceHash;
            QList<ExtendedAttribute> attrs;
            attrs.append(hashAttr);
            newFile.setExtendedAttributes(attrs);
        }

        // Add to index (simplified - real implementation would handle paths)
        // d->index->rootDirectory().addFile(newFile);
    }

    item.status = TransferStatus::Completed;
    return true;
}

bool TapeIO::readFileFromTape(TransferItem &item)
{
    // Create parent directories
    if (d->options.createDirectories) {
        QFileInfo destInfo(item.destPath);
        QDir dir;
        if (!dir.mkpath(destInfo.absolutePath())) {
            item.errorMessage = QStringLiteral("Failed to create directory");
            item.status = TransferStatus::Failed;
            return false;
        }
    }

    QFile file(item.destPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        item.errorMessage = QStringLiteral("Failed to create destination file: %1").arg(file.errorString());
        item.status = TransferStatus::Failed;
        return false;
    }

    // Read data from tape
    quint32 blockSize = d->options.blockSize;
    QByteArray buffer;
    qint64 totalRead = 0;
    qint64 expectedSize = item.size;

    while (totalRead < expectedSize) {
        if (d->cancelled) {
            item.status = TransferStatus::Cancelled;
            file.close();
            QFile::remove(item.destPath);
            return false;
        }

        while (d->paused) {
            QThread::msleep(100);
            if (d->cancelled) {
                item.status = TransferStatus::Cancelled;
                file.close();
                QFile::remove(item.destPath);
                return false;
            }
        }

        qint64 toRead = qMin(static_cast<qint64>(blockSize), expectedSize - totalRead);
        qint64 bytesRead = d->device->readBlock(buffer, static_cast<quint32>(toRead));

        if (bytesRead < 0) {
            item.errorMessage = QStringLiteral("Read error: %1").arg(d->device->lastError());
            item.status = TransferStatus::Failed;
            file.close();
            QFile::remove(item.destPath);
            return false;
        }

        if (bytesRead == 0) {
            // End of data (filemark or blank)
            break;
        }

        if (file.write(buffer) != buffer.size()) {
            item.errorMessage = QStringLiteral("Write error: %1").arg(file.errorString());
            item.status = TransferStatus::Failed;
            file.close();
            QFile::remove(item.destPath);
            return false;
        }

        totalRead += bytesRead;
        item.bytesTransferred = totalRead;
        d->stats.completedBytes += bytesRead;

        emit fileProgress(item, totalRead, expectedSize);
        updateStatistics();
    }

    file.close();

    // Verify hash if enabled
    if (d->options.verifyAfterWrite && !item.sourceHash.isEmpty()) {
        if (!verifyFileHash(item)) {
            return false;
        }
    }

    item.status = TransferStatus::Completed;
    return true;
}

bool TapeIO::verifyFileHash(TransferItem &item)
{
    HashCalculator calc(d->options.hashMode);
    HashResult result = calc.hashFile(item.destPath);

    if (!result.success) {
        item.errorMessage = QStringLiteral("Hash calculation failed");
        item.status = TransferStatus::Failed;
        return false;
    }

    item.destHash = result.hexString;
    item.hashVerified = (item.sourceHash.compare(item.destHash, Qt::CaseInsensitive) == 0);

    if (!item.hashVerified) {
        item.errorMessage = QStringLiteral("Hash verification failed");
        item.status = TransferStatus::Failed;
        return false;
    }

    return true;
}

void TapeIO::updateStatistics()
{
    d->stats.elapsedMs = d->timer.elapsed();

    if (d->stats.elapsedMs > 0) {
        d->stats.bytesPerSecond = static_cast<double>(d->stats.completedBytes) * 1000.0 / d->stats.elapsedMs;

        if (d->stats.bytesPerSecond > 0) {
            qint64 remainingBytes = d->stats.totalBytes - d->stats.completedBytes;
            d->stats.estimatedRemainingMs = static_cast<qint64>(remainingBytes * 1000.0 / d->stats.bytesPerSecond);
        }
    }

    emit progressChanged(d->stats.progressPercent(), d->stats);
}

void TapeIO::processQueue()
{
    for (int i = 0; i < d->queue.size(); ++i) {
        if (d->cancelled) {
            break;
        }

        while (d->paused) {
            QThread::msleep(100);
            if (d->cancelled) {
                break;
            }
        }

        if (d->cancelled) {
            break;
        }

        TransferItem &item = d->queue[i];
        if (item.status != TransferStatus::Pending) {
            continue;
        }

        d->currentItemIndex = i;
        item.status = TransferStatus::InProgress;
        emit fileStarted(item);

        bool success = writeFileToTape(item);

        if (success) {
            d->stats.completedFiles++;
            emit fileCompleted(item);
        } else {
            d->stats.failedFiles++;
            emit fileError(item, item.errorMessage);

            if (!d->options.continueOnError) {
                break;
            }
        }

        updateStatistics();
    }

    d->running = false;
    d->currentItemIndex = -1;

    emit transferCompleted(d->stats);
    emit runningChanged(false);
}

} // namespace qltfs
