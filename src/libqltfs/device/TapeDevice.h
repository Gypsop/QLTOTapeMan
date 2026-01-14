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
#include "core/LtfsLabel.h"
#include "device/ScsiCommand.h"
#include "device/DeviceEnumerator.h"

#include <QObject>
#include <QString>
#include <QByteArray>
#include <QSharedPointer>
#include <functional>

namespace qltfs {

/**
 * @brief Tape device status
 */
enum class TapeStatus {
    Unknown,        ///< Status unknown
    NotReady,       ///< Device not ready (no media, etc.)
    Ready,          ///< Device ready with media
    NoMedia,        ///< No media in drive
    Loading,        ///< Media is loading
    Unloading,      ///< Media is unloading
    Rewinding,      ///< Media is rewinding
    Locating,       ///< Media is locating to position
    Writing,        ///< Currently writing
    Reading,        ///< Currently reading
    WriteProtected, ///< Media is write-protected
    Error           ///< Error condition
};

/**
 * @brief Media type information
 */
struct LIBQLTFS_EXPORT TapeMediaInfo {
    QString mediaType;          ///< Media type string (LTO-5, LTO-6, etc.)
    quint8 densityCode = 0;     ///< SCSI density code
    bool isLtfs = false;        ///< Media is formatted with LTFS
    bool isPartitioned = false; ///< Media has multiple partitions
    bool isWriteProtected = false;
    quint64 totalCapacity = 0;  ///< Total capacity in bytes
    quint64 remainingCapacity = 0; ///< Remaining capacity in bytes
    quint32 blockSize = DEFAULT_BLOCK_SIZE;

    QString formatString() const;
};

/**
 * @brief Log page data for drive diagnostics
 */
struct LIBQLTFS_EXPORT TapeLogData {
    quint64 totalBytesWritten = 0;
    quint64 totalBytesRead = 0;
    quint64 totalWriteErrors = 0;
    quint64 totalReadErrors = 0;
    quint64 lifetimeBytesWritten = 0;
    quint64 lifetimeBytesRead = 0;
    int temperatureCelsius = 0;
    int humidityPercent = 0;
    quint32 loadCount = 0;
    quint32 cleaningRequired = 0;

    bool isValid() const;
};

/**
 * @brief Progress callback type for long operations
 */
using TapeProgressCallback = std::function<void(qint64 current, qint64 total, const QString &status)>;

/**
 * @brief High-level tape device interface
 *
 * Provides object-oriented access to tape drive operations including
 * positioning, reading, writing, and status monitoring.
 */
class LIBQLTFS_EXPORT TapeDevice : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString devicePath READ devicePath CONSTANT)
    Q_PROPERTY(bool isOpen READ isOpen NOTIFY openChanged)
    Q_PROPERTY(TapeStatus status READ status NOTIFY statusChanged)

public:
    /**
     * @brief Constructor
     * @param devicePath Path to tape device
     * @param parent Parent object
     */
    explicit TapeDevice(const QString &devicePath, QObject *parent = nullptr);

    /**
     * @brief Constructor with device info
     * @param deviceInfo Device information from enumerator
     * @param parent Parent object
     */
    explicit TapeDevice(const TapeDeviceInfo &deviceInfo, QObject *parent = nullptr);

    ~TapeDevice() override;

    // Disable copy
    TapeDevice(const TapeDevice &) = delete;
    TapeDevice &operator=(const TapeDevice &) = delete;

    // === Device Control ===

    /**
     * @brief Open device for operations
     * @return true if device opened successfully
     */
    bool open();

    /**
     * @brief Close device
     */
    void close();

    /**
     * @brief Check if device is open
     */
    bool isOpen() const;

    /**
     * @brief Get device path
     */
    QString devicePath() const;

    /**
     * @brief Get device information
     */
    TapeDeviceInfo deviceInfo() const;

    /**
     * @brief Set operation timeout in seconds
     */
    void setTimeout(int seconds);

    /**
     * @brief Get current timeout
     */
    int timeout() const;

    // === Status ===

    /**
     * @brief Get current tape status
     */
    TapeStatus status() const;

    /**
     * @brief Refresh device status
     * @return true if status was retrieved successfully
     */
    bool refreshStatus();

    /**
     * @brief Test if device is ready
     */
    bool testReady();

    /**
     * @brief Get media information
     */
    TapeMediaInfo mediaInfo() const;

    /**
     * @brief Refresh media information
     */
    bool refreshMediaInfo();

    /**
     * @brief Get drive log data
     */
    TapeLogData logData() const;

    /**
     * @brief Refresh log data
     */
    bool refreshLogData();

    /**
     * @brief Get block limits
     */
    BlockLimits blockLimits() const;

    /**
     * @brief Get current tape position
     */
    TapePosition position() const;

    // === Media Control ===

    /**
     * @brief Load media (close door/load cassette)
     * @param wait Wait for completion
     */
    bool load(bool wait = true);

    /**
     * @brief Unload/eject media
     * @param wait Wait for completion
     */
    bool unload(bool wait = true);

    /**
     * @brief Rewind to beginning of tape
     * @param wait Wait for completion
     */
    bool rewind(bool wait = true);

    /**
     * @brief Lock media to prevent ejection
     */
    bool lockMedia();

    /**
     * @brief Unlock media to allow ejection
     */
    bool unlockMedia();

    // === Positioning ===

    /**
     * @brief Locate to specific block
     * @param partition Partition number (0=index, 1=data)
     * @param blockNumber Block number within partition
     */
    bool locate(quint8 partition, quint64 blockNumber);

    /**
     * @brief Space forward/backward by blocks
     * @param count Number of blocks (negative for backward)
     */
    bool spaceBlocks(qint32 count);

    /**
     * @brief Space forward/backward by filemarks
     * @param count Number of filemarks (negative for backward)
     */
    bool spaceFilemarks(qint32 count);

    /**
     * @brief Space to end of data
     */
    bool spaceToEndOfData();

    /**
     * @brief Seek to beginning of partition
     * @param partition Partition number
     */
    bool seekToBeginning(quint8 partition = 0);

    /**
     * @brief Seek to end of partition
     * @param partition Partition number
     */
    bool seekToEnd(quint8 partition = 1);

    // === Read/Write ===

    /**
     * @brief Read one block from tape
     * @param data Buffer to receive data
     * @param maxSize Maximum size to read
     * @return Number of bytes read, or -1 on error
     */
    qint64 readBlock(QByteArray &data, quint32 maxSize);

    /**
     * @brief Write one block to tape
     * @param data Data to write
     * @return Number of bytes written, or -1 on error
     */
    qint64 writeBlock(const QByteArray &data);

    /**
     * @brief Read multiple blocks
     * @param data Buffer to receive data
     * @param blockCount Number of blocks to read
     * @param blockSize Size of each block
     * @return Number of bytes read, or -1 on error
     */
    qint64 readBlocks(QByteArray &data, quint32 blockCount, quint32 blockSize);

    /**
     * @brief Write multiple blocks
     * @param data Data to write (should be multiple of blockSize)
     * @param blockSize Size of each block
     * @return Number of bytes written, or -1 on error
     */
    qint64 writeBlocks(const QByteArray &data, quint32 blockSize);

    /**
     * @brief Write filemark(s)
     * @param count Number of filemarks
     */
    bool writeFilemark(quint32 count = 1);

    /**
     * @brief Set block size for fixed-block mode
     */
    bool setBlockSize(quint32 blockSize);

    /**
     * @brief Get current block size
     */
    quint32 blockSize() const;

    /**
     * @brief Enable or disable hardware compression
     */
    bool setCompression(bool enabled);

    /**
     * @brief Check if compression is enabled
     */
    bool compressionEnabled() const;

    // === LTFS Operations ===

    /**
     * @brief Read LTFS label from tape
     * @param partition Partition to read from
     * @return Label, or invalid label on error
     */
    LtfsLabel readLabel(PartitionLabel partition = PartitionLabel::IndexPartition);

    /**
     * @brief Write LTFS label to tape
     * @param label Label to write
     * @param partition Partition to write to
     */
    bool writeLabel(const LtfsLabel &label, PartitionLabel partition = PartitionLabel::IndexPartition);

    /**
     * @brief Check if tape is LTFS formatted
     */
    bool isLtfsFormatted();

    /**
     * @brief Read MAM (Medium Auxiliary Memory) attributes
     * @return Key-value map of MAM attributes
     */
    QMap<quint16, QByteArray> readMamAttributes();

    /**
     * @brief Write MAM attribute
     * @param attributeId Attribute ID
     * @param data Attribute data
     */
    bool writeMamAttribute(quint16 attributeId, const QByteArray &data);

    /**
     * @brief Enable write at end of data
     *
     * LTFS requires allowing overwrite at current position.
     */
    bool allowOverwrite();

    // === Format ===

    /**
     * @brief Format tape for LTFS
     * @param label Volume label
     * @param callback Progress callback
     * @return true if format succeeded
     */
    bool formatForLtfs(const LtfsLabel &label, TapeProgressCallback callback = nullptr);

    /**
     * @brief Quick erase (write EOD at beginning)
     */
    bool quickErase();

    /**
     * @brief Long erase (full tape erase)
     * @param callback Progress callback
     */
    bool longErase(TapeProgressCallback callback = nullptr);

    // === Error Handling ===

    /**
     * @brief Get last error message
     */
    QString lastError() const;

    /**
     * @brief Get last SCSI sense data
     */
    ScsiSenseData lastSenseData() const;

    /**
     * @brief Clear error state
     */
    void clearError();

    // === Raw SCSI Access ===

    /**
     * @brief Get underlying SCSI command interface
     *
     * For advanced operations not covered by high-level API.
     */
    ScsiCommand *scsiCommand() const;

signals:
    /**
     * @brief Emitted when device open state changes
     */
    void openChanged(bool open);

    /**
     * @brief Emitted when device status changes
     */
    void statusChanged(TapeStatus status);

    /**
     * @brief Emitted when media is inserted or removed
     */
    void mediaChanged(bool present);

    /**
     * @brief Emitted on error
     */
    void errorOccurred(const QString &message);

    /**
     * @brief Emitted during long operations
     */
    void progressChanged(qint64 current, qint64 total, const QString &status);

private:
    class Private;
    Private *d;

    void setStatus(TapeStatus status);
    void setError(const QString &message);
    bool checkOpen(const char *operation);
};

} // namespace qltfs
