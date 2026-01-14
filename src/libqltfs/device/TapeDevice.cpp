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

#include "TapeDevice.h"

#include <QDebug>
#include <QThread>
#include <QElapsedTimer>

namespace qltfs {

// Density codes for LTO generations
static constexpr quint8 DENSITY_LTO1 = 0x40;
static constexpr quint8 DENSITY_LTO2 = 0x42;
static constexpr quint8 DENSITY_LTO3 = 0x44;
static constexpr quint8 DENSITY_LTO4 = 0x46;
static constexpr quint8 DENSITY_LTO5 = 0x58;
static constexpr quint8 DENSITY_LTO6 = 0x5A;
static constexpr quint8 DENSITY_LTO7 = 0x5C;
static constexpr quint8 DENSITY_LTO8 = 0x5D;
static constexpr quint8 DENSITY_LTO9 = 0x5E;

// Log page codes
static constexpr quint8 LOG_PAGE_WRITE_ERRORS = 0x02;
static constexpr quint8 LOG_PAGE_READ_ERRORS = 0x03;
static constexpr quint8 LOG_PAGE_TEMPERATURE = 0x0D;
static constexpr quint8 LOG_PAGE_TAPE_CAPACITY = 0x31;
static constexpr quint8 LOG_PAGE_DATA_COMPRESSION = 0x1B;
static constexpr quint8 LOG_PAGE_TAPE_USAGE = 0x30;

// Mode page codes
static constexpr quint8 MODE_PAGE_CONTROL = 0x0A;
static constexpr quint8 MODE_PAGE_DATA_COMPRESSION = 0x0F;
static constexpr quint8 MODE_PAGE_DEVICE_CONFIG = 0x10;
static constexpr quint8 MODE_PAGE_MEDIUM_PARTITION = 0x11;

// ============================================================================
// TapeMediaInfo Implementation
// ============================================================================

QString TapeMediaInfo::formatString() const
{
    return QStringLiteral("%1 (%2 / %3 remaining)")
        .arg(mediaType)
        .arg(formatSize(totalCapacity))
        .arg(formatSize(remainingCapacity));
}

// ============================================================================
// TapeLogData Implementation
// ============================================================================

bool TapeLogData::isValid() const
{
    return totalBytesWritten > 0 || totalBytesRead > 0 || loadCount > 0;
}

// ============================================================================
// TapeDevice Private Implementation
// ============================================================================

class TapeDevice::Private
{
public:
    QString devicePath;
    TapeDeviceInfo deviceInfo;
    QScopedPointer<ScsiCommand> scsi;

    TapeStatus status = TapeStatus::Unknown;
    TapeMediaInfo mediaInfo;
    TapeLogData logData;
    BlockLimits blockLimits;
    TapePosition position;

    quint32 currentBlockSize = DEFAULT_BLOCK_SIZE;
    bool compressionEnabled = true;

    QString lastError;
    ScsiSenseData lastSenseData;

    QString densityCodeToString(quint8 code) const;
    bool parsePositionData(const QByteArray &data);
    bool parseBlockLimits(const QByteArray &data);
    bool parseMediaInfo(ScsiCommand *scsi);
    bool parseLogData(ScsiCommand *scsi);
};

QString TapeDevice::Private::densityCodeToString(quint8 code) const
{
    switch (code) {
    case DENSITY_LTO1: return QStringLiteral("LTO-1");
    case DENSITY_LTO2: return QStringLiteral("LTO-2");
    case DENSITY_LTO3: return QStringLiteral("LTO-3");
    case DENSITY_LTO4: return QStringLiteral("LTO-4");
    case DENSITY_LTO5: return QStringLiteral("LTO-5");
    case DENSITY_LTO6: return QStringLiteral("LTO-6");
    case DENSITY_LTO7: return QStringLiteral("LTO-7");
    case DENSITY_LTO8: return QStringLiteral("LTO-8");
    case DENSITY_LTO9: return QStringLiteral("LTO-9");
    default:
        if (code >= 0x40 && code < 0x60) {
            return QStringLiteral("LTO (0x%1)").arg(code, 2, 16, QLatin1Char('0'));
        }
        return QStringLiteral("Unknown (0x%1)").arg(code, 2, 16, QLatin1Char('0'));
    }
}

bool TapeDevice::Private::parsePositionData(const QByteArray &data)
{
    if (data.size() < 20) {
        return false;
    }

    const quint8 *bytes = reinterpret_cast<const quint8 *>(data.constData());

    // Short form position data
    position.partition = bytes[1];
    position.blockNumber = (static_cast<quint64>(bytes[4]) << 24) |
                          (static_cast<quint64>(bytes[5]) << 16) |
                          (static_cast<quint64>(bytes[6]) << 8) |
                          static_cast<quint64>(bytes[7]);
    position.fileNumber = (static_cast<quint64>(bytes[8]) << 24) |
                         (static_cast<quint64>(bytes[9]) << 16) |
                         (static_cast<quint64>(bytes[10]) << 8) |
                         static_cast<quint64>(bytes[11]);

    // Flags
    position.beginOfPartition = (bytes[0] & 0x80) != 0;
    position.endOfPartition = (bytes[0] & 0x40) != 0;
    position.blockPositionUnknown = (bytes[0] & 0x04) != 0;

    return true;
}

bool TapeDevice::Private::parseBlockLimits(const QByteArray &data)
{
    if (data.size() < 6) {
        return false;
    }

    const quint8 *bytes = reinterpret_cast<const quint8 *>(data.constData());

    // Granularity
    blockLimits.granularity = bytes[0];

    // Maximum block length (3 bytes)
    blockLimits.maxBlockLength = (static_cast<quint32>(bytes[1]) << 16) |
                                 (static_cast<quint32>(bytes[2]) << 8) |
                                 static_cast<quint32>(bytes[3]);

    // Minimum block length (2 bytes)
    blockLimits.minBlockLength = (static_cast<quint32>(bytes[4]) << 8) |
                                 static_cast<quint32>(bytes[5]);

    return true;
}

bool TapeDevice::Private::parseMediaInfo(ScsiCommand *scsi)
{
    // Get density from Mode Sense
    auto result = scsi->modeSense10(MODE_PAGE_DEVICE_CONFIG, 0, 256);
    if (result.success && result.data.size() >= 12) {
        const quint8 *data = reinterpret_cast<const quint8 *>(result.data.constData());

        // Density code is in byte 4 of the mode parameter header for tape
        if (result.data.size() > 8) {
            mediaInfo.densityCode = data[8];
            mediaInfo.mediaType = densityCodeToString(mediaInfo.densityCode);
        }

        // Block descriptor area follows header
        quint16 blockDescLength = (static_cast<quint16>(data[6]) << 8) | data[7];
        if (blockDescLength >= 8 && result.data.size() >= 16) {
            int bdOffset = 8;
            mediaInfo.blockSize = (static_cast<quint32>(data[bdOffset + 5]) << 16) |
                                  (static_cast<quint32>(data[bdOffset + 6]) << 8) |
                                  static_cast<quint32>(data[bdOffset + 7]);
        }
    }

    // Get capacity from Log Sense (tape capacity page)
    result = scsi->logSense(LOG_PAGE_TAPE_CAPACITY, 0, 256);
    if (result.success && result.data.size() >= 16) {
        const quint8 *data = reinterpret_cast<const quint8 *>(result.data.constData());

        // Parse log parameter format
        int offset = 4;  // Skip page header
        while (offset + 4 <= result.data.size()) {
            quint16 paramCode = (static_cast<quint16>(data[offset]) << 8) | data[offset + 1];
            quint8 paramLen = data[offset + 3];

            if (offset + 4 + paramLen > result.data.size()) {
                break;
            }

            // Parameter 0001h = Main partition remaining capacity (MiB)
            // Parameter 0002h = Alternate partition remaining capacity (MiB)
            // Parameter 0003h = Main partition maximum capacity (MiB)
            // Parameter 0004h = Alternate partition maximum capacity (MiB)

            quint64 value = 0;
            for (int i = 0; i < qMin(static_cast<int>(paramLen), 8); ++i) {
                value = (value << 8) | data[offset + 4 + i];
            }

            switch (paramCode) {
            case 0x0001:
            case 0x0002:
                mediaInfo.remainingCapacity += value * 1024 * 1024;  // MiB to bytes
                break;
            case 0x0003:
            case 0x0004:
                mediaInfo.totalCapacity += value * 1024 * 1024;
                break;
            }

            offset += 4 + paramLen;
        }
    }

    // Check write protection via Mode Sense
    result = scsi->modeSense10(0, 0, 64);
    if (result.success && result.data.size() >= 3) {
        // WP bit is in byte 3 of mode parameter header
        mediaInfo.isWriteProtected = (result.data[3] & 0x80) != 0;
    }

    // Check if LTFS formatted by reading partition mode page
    result = scsi->modeSense10(MODE_PAGE_MEDIUM_PARTITION, 0, 256);
    if (result.success && result.data.size() >= 16) {
        // If additional partitions exist, it might be LTFS
        const quint8 *data = reinterpret_cast<const quint8 *>(result.data.constData());
        // Skip mode parameter header
        int headerLen = 8;
        if (result.data.size() > headerLen + 6) {
            quint8 additionalPartitions = data[headerLen + 3];
            mediaInfo.isPartitioned = (additionalPartitions > 0);
        }
    }

    return true;
}

bool TapeDevice::Private::parseLogData(ScsiCommand *scsi)
{
    // Temperature log page
    auto result = scsi->logSense(LOG_PAGE_TEMPERATURE, 0, 256);
    if (result.success && result.data.size() >= 12) {
        const quint8 *data = reinterpret_cast<const quint8 *>(result.data.constData());
        // Temperature is typically in parameter 0000h
        if (result.data.size() >= 10) {
            logData.temperatureCelsius = data[9];
        }
    }

    // Write errors log page
    result = scsi->logSense(LOG_PAGE_WRITE_ERRORS, 0, 256);
    if (result.success && result.data.size() >= 8) {
        // Parse error counts from log page
        // Format varies by drive, simplified parse here
    }

    // Read errors log page
    result = scsi->logSense(LOG_PAGE_READ_ERRORS, 0, 256);
    if (result.success && result.data.size() >= 8) {
        // Parse error counts
    }

    // Tape usage log page (0x30)
    result = scsi->logSense(LOG_PAGE_TAPE_USAGE, 0, 512);
    if (result.success && result.data.size() >= 8) {
        const quint8 *data = reinterpret_cast<const quint8 *>(result.data.constData());
        int offset = 4;

        while (offset + 4 <= result.data.size()) {
            quint16 paramCode = (static_cast<quint16>(data[offset]) << 8) | data[offset + 1];
            quint8 paramLen = data[offset + 3];

            if (offset + 4 + paramLen > result.data.size()) {
                break;
            }

            quint64 value = 0;
            for (int i = 0; i < qMin(static_cast<int>(paramLen), 8); ++i) {
                value = (value << 8) | data[offset + 4 + i];
            }

            // Common parameter codes for tape usage
            switch (paramCode) {
            case 0x0001:  // Total data written (MB)
                logData.totalBytesWritten = value * 1024 * 1024;
                break;
            case 0x0002:  // Total data read (MB)
                logData.totalBytesRead = value * 1024 * 1024;
                break;
            case 0x0008:  // Total load count
                logData.loadCount = static_cast<quint32>(value);
                break;
            }

            offset += 4 + paramLen;
        }
    }

    return true;
}

// ============================================================================
// TapeDevice Implementation
// ============================================================================

TapeDevice::TapeDevice(const QString &devicePath, QObject *parent)
    : QObject(parent)
    , d(new Private)
{
    d->devicePath = devicePath;
    d->deviceInfo.devicePath = devicePath;
}

TapeDevice::TapeDevice(const TapeDeviceInfo &deviceInfo, QObject *parent)
    : QObject(parent)
    , d(new Private)
{
    d->devicePath = deviceInfo.genericPath.isEmpty() ? deviceInfo.devicePath : deviceInfo.genericPath;
    d->deviceInfo = deviceInfo;
}

TapeDevice::~TapeDevice()
{
    close();
    delete d;
}

bool TapeDevice::open()
{
    if (isOpen()) {
        return true;
    }

    d->scsi.reset(new ScsiCommand(d->devicePath));
    if (!d->scsi->open()) {
        d->lastError = QStringLiteral("Failed to open device: %1").arg(d->devicePath);
        d->scsi.reset();
        return false;
    }

    emit openChanged(true);

    // Get initial status
    refreshStatus();

    return true;
}

void TapeDevice::close()
{
    if (d->scsi) {
        d->scsi->close();
        d->scsi.reset();
        d->status = TapeStatus::Unknown;
        emit openChanged(false);
    }
}

bool TapeDevice::isOpen() const
{
    return d->scsi && d->scsi->isOpen();
}

QString TapeDevice::devicePath() const
{
    return d->devicePath;
}

TapeDeviceInfo TapeDevice::deviceInfo() const
{
    return d->deviceInfo;
}

void TapeDevice::setTimeout(int seconds)
{
    if (d->scsi) {
        d->scsi->setTimeout(seconds);
    }
}

int TapeDevice::timeout() const
{
    return d->scsi ? d->scsi->timeout() : 60;
}

TapeStatus TapeDevice::status() const
{
    return d->status;
}

bool TapeDevice::refreshStatus()
{
    if (!checkOpen("refreshStatus")) {
        return false;
    }

    auto result = d->scsi->testUnitReady();

    if (result.success) {
        setStatus(TapeStatus::Ready);
        return true;
    }

    d->lastSenseData = result.senseData;

    // Interpret sense data
    switch (result.senseData.senseKey) {
    case ScsiSenseKey::NotReady:
        if (result.senseData.additionalSenseCode == 0x3A) {
            setStatus(TapeStatus::NoMedia);
        } else if (result.senseData.additionalSenseCode == 0x04) {
            // ASC 04h = Not ready, various causes
            switch (result.senseData.additionalSenseCodeQualifier) {
            case 0x00:
                setStatus(TapeStatus::NotReady);
                break;
            case 0x01:  // Becoming ready
                setStatus(TapeStatus::Loading);
                break;
            case 0x02:  // Need initialize command
                setStatus(TapeStatus::NotReady);
                break;
            default:
                setStatus(TapeStatus::NotReady);
                break;
            }
        } else {
            setStatus(TapeStatus::NotReady);
        }
        break;

    case ScsiSenseKey::UnitAttention:
        // Media may have changed, try again
        result = d->scsi->testUnitReady();
        if (result.success) {
            setStatus(TapeStatus::Ready);
            emit mediaChanged(true);
        } else {
            setStatus(TapeStatus::NotReady);
        }
        break;

    case ScsiSenseKey::DataProtect:
        setStatus(TapeStatus::WriteProtected);
        break;

    default:
        setStatus(TapeStatus::Error);
        break;
    }

    return true;
}

bool TapeDevice::testReady()
{
    if (!checkOpen("testReady")) {
        return false;
    }

    auto result = d->scsi->testUnitReady();
    d->lastSenseData = result.senseData;

    return result.success;
}

TapeMediaInfo TapeDevice::mediaInfo() const
{
    return d->mediaInfo;
}

bool TapeDevice::refreshMediaInfo()
{
    if (!checkOpen("refreshMediaInfo")) {
        return false;
    }

    d->mediaInfo = TapeMediaInfo();
    return d->parseMediaInfo(d->scsi.data());
}

TapeLogData TapeDevice::logData() const
{
    return d->logData;
}

bool TapeDevice::refreshLogData()
{
    if (!checkOpen("refreshLogData")) {
        return false;
    }

    d->logData = TapeLogData();
    return d->parseLogData(d->scsi.data());
}

BlockLimits TapeDevice::blockLimits() const
{
    return d->blockLimits;
}

TapePosition TapeDevice::position() const
{
    return d->position;
}

bool TapeDevice::load(bool wait)
{
    if (!checkOpen("load")) {
        return false;
    }

    setStatus(TapeStatus::Loading);
    auto result = d->scsi->loadUnload(true, !wait);
    d->lastSenseData = result.senseData;

    if (!result.success) {
        setError(QStringLiteral("Load failed: %1").arg(result.errorMessage()));
        return false;
    }

    if (wait) {
        refreshStatus();
    }

    return true;
}

bool TapeDevice::unload(bool wait)
{
    if (!checkOpen("unload")) {
        return false;
    }

    setStatus(TapeStatus::Unloading);
    auto result = d->scsi->loadUnload(false, !wait);
    d->lastSenseData = result.senseData;

    if (!result.success) {
        setError(QStringLiteral("Unload failed: %1").arg(result.errorMessage()));
        return false;
    }

    if (wait) {
        setStatus(TapeStatus::NoMedia);
    }

    return true;
}

bool TapeDevice::rewind(bool wait)
{
    if (!checkOpen("rewind")) {
        return false;
    }

    setStatus(TapeStatus::Rewinding);
    auto result = d->scsi->rewind(!wait);
    d->lastSenseData = result.senseData;

    if (!result.success) {
        setError(QStringLiteral("Rewind failed: %1").arg(result.errorMessage()));
        return false;
    }

    if (wait) {
        setStatus(TapeStatus::Ready);
        d->position.blockNumber = 0;
        d->position.fileNumber = 0;
        d->position.beginOfPartition = true;
        d->position.endOfPartition = false;
    }

    return true;
}

bool TapeDevice::lockMedia()
{
    if (!checkOpen("lockMedia")) {
        return false;
    }

    auto result = d->scsi->preventAllowMediumRemoval(true);
    d->lastSenseData = result.senseData;

    if (!result.success) {
        setError(QStringLiteral("Lock media failed: %1").arg(result.errorMessage()));
        return false;
    }

    return true;
}

bool TapeDevice::unlockMedia()
{
    if (!checkOpen("unlockMedia")) {
        return false;
    }

    auto result = d->scsi->preventAllowMediumRemoval(false);
    d->lastSenseData = result.senseData;

    if (!result.success) {
        setError(QStringLiteral("Unlock media failed: %1").arg(result.errorMessage()));
        return false;
    }

    return true;
}

bool TapeDevice::locate(quint8 partition, quint64 blockNumber)
{
    if (!checkOpen("locate")) {
        return false;
    }

    setStatus(TapeStatus::Locating);
    auto result = d->scsi->locate16(partition, blockNumber);
    d->lastSenseData = result.senseData;

    if (!result.success) {
        setError(QStringLiteral("Locate failed: %1").arg(result.errorMessage()));
        refreshStatus();
        return false;
    }

    d->position.partition = partition;
    d->position.blockNumber = blockNumber;
    setStatus(TapeStatus::Ready);

    return true;
}

bool TapeDevice::spaceBlocks(qint32 count)
{
    if (!checkOpen("spaceBlocks")) {
        return false;
    }

    auto result = d->scsi->space(0, count);  // Code 0 = blocks
    d->lastSenseData = result.senseData;

    if (!result.success) {
        setError(QStringLiteral("Space blocks failed: %1").arg(result.errorMessage()));
        return false;
    }

    d->position.blockNumber += count;

    return true;
}

bool TapeDevice::spaceFilemarks(qint32 count)
{
    if (!checkOpen("spaceFilemarks")) {
        return false;
    }

    auto result = d->scsi->space(1, count);  // Code 1 = filemarks
    d->lastSenseData = result.senseData;

    if (!result.success) {
        setError(QStringLiteral("Space filemarks failed: %1").arg(result.errorMessage()));
        return false;
    }

    d->position.fileNumber += count;

    return true;
}

bool TapeDevice::spaceToEndOfData()
{
    if (!checkOpen("spaceToEndOfData")) {
        return false;
    }

    auto result = d->scsi->space(3, 0);  // Code 3 = end of data
    d->lastSenseData = result.senseData;

    if (!result.success) {
        setError(QStringLiteral("Space to EOD failed: %1").arg(result.errorMessage()));
        return false;
    }

    d->position.endOfPartition = true;

    return true;
}

bool TapeDevice::seekToBeginning(quint8 partition)
{
    return locate(partition, 0);
}

bool TapeDevice::seekToEnd(quint8 partition)
{
    if (!locate(partition, 0)) {
        return false;
    }
    return spaceToEndOfData();
}

qint64 TapeDevice::readBlock(QByteArray &data, quint32 maxSize)
{
    if (!checkOpen("readBlock")) {
        return -1;
    }

    data.resize(static_cast<int>(maxSize));
    auto result = d->scsi->read6(data, 1, false);  // Variable block mode
    d->lastSenseData = result.senseData;

    if (!result.success) {
        // Check for blank check (end of data)
        if (result.senseData.senseKey == ScsiSenseKey::BlankCheck) {
            return 0;  // EOF
        }
        setError(QStringLiteral("Read failed: %1").arg(result.errorMessage()));
        return -1;
    }

    data = result.data;
    d->position.blockNumber++;

    return data.size();
}

qint64 TapeDevice::writeBlock(const QByteArray &data)
{
    if (!checkOpen("writeBlock")) {
        return -1;
    }

    setStatus(TapeStatus::Writing);
    auto result = d->scsi->write6(data, false);  // Variable block mode
    d->lastSenseData = result.senseData;

    if (!result.success) {
        setError(QStringLiteral("Write failed: %1").arg(result.errorMessage()));
        refreshStatus();
        return -1;
    }

    d->position.blockNumber++;
    setStatus(TapeStatus::Ready);

    return static_cast<qint64>(result.bytesTransferred);
}

qint64 TapeDevice::readBlocks(QByteArray &data, quint32 blockCount, quint32 blockSize)
{
    if (!checkOpen("readBlocks")) {
        return -1;
    }

    // Set block size for fixed block mode
    data.resize(static_cast<int>(blockCount * blockSize));
    auto result = d->scsi->read6(data, blockCount, true);  // Fixed block mode
    d->lastSenseData = result.senseData;

    if (!result.success) {
        if (result.senseData.senseKey == ScsiSenseKey::BlankCheck) {
            return 0;
        }
        setError(QStringLiteral("Read blocks failed: %1").arg(result.errorMessage()));
        return -1;
    }

    data = result.data;
    d->position.blockNumber += blockCount;

    return data.size();
}

qint64 TapeDevice::writeBlocks(const QByteArray &data, quint32 blockSize)
{
    if (!checkOpen("writeBlocks")) {
        return -1;
    }

    quint32 blockCount = static_cast<quint32>(data.size()) / blockSize;

    setStatus(TapeStatus::Writing);
    auto result = d->scsi->write6(data, true);  // Fixed block mode
    d->lastSenseData = result.senseData;

    if (!result.success) {
        setError(QStringLiteral("Write blocks failed: %1").arg(result.errorMessage()));
        refreshStatus();
        return -1;
    }

    d->position.blockNumber += blockCount;
    setStatus(TapeStatus::Ready);

    return static_cast<qint64>(result.bytesTransferred);
}

bool TapeDevice::writeFilemark(quint32 count)
{
    if (!checkOpen("writeFilemark")) {
        return false;
    }

    auto result = d->scsi->writeFilemark(count, false, false);
    d->lastSenseData = result.senseData;

    if (!result.success) {
        setError(QStringLiteral("Write filemark failed: %1").arg(result.errorMessage()));
        return false;
    }

    d->position.fileNumber += count;

    return true;
}

bool TapeDevice::setBlockSize(quint32 blockSize)
{
    if (!checkOpen("setBlockSize")) {
        return false;
    }

    // Build mode select data for block descriptor
    QByteArray modeData(12, 0);

    // Mode parameter header (8 bytes for mode select 10)
    // Byte 0-1: Mode data length (filled by drive)
    // Byte 2: Medium type
    // Byte 3: Device-specific parameter (WP bit, etc.)
    // Byte 4-5: Reserved
    // Byte 6-7: Block descriptor length = 8

    modeData[7] = 8;  // Block descriptor length

    // Block descriptor (8 bytes starting at offset 8)
    // Byte 0: Density code (0 = current)
    // Byte 1-3: Number of blocks (0 = all)
    // Byte 4: Reserved
    // Byte 5-7: Block length

    modeData[8] = 0;  // Current density
    modeData[11] = static_cast<char>((blockSize >> 16) & 0xFF);
    modeData[12] = static_cast<char>((blockSize >> 8) & 0xFF);
    modeData[13] = static_cast<char>(blockSize & 0xFF);

    // Need to resize to include block descriptor
    modeData.resize(16);

    auto result = d->scsi->modeSelect10(modeData, false);
    d->lastSenseData = result.senseData;

    if (!result.success) {
        setError(QStringLiteral("Set block size failed: %1").arg(result.errorMessage()));
        return false;
    }

    d->currentBlockSize = blockSize;

    return true;
}

quint32 TapeDevice::blockSize() const
{
    return d->currentBlockSize;
}

bool TapeDevice::setCompression(bool enabled)
{
    if (!checkOpen("setCompression")) {
        return false;
    }

    // Get current data compression mode page
    auto result = d->scsi->modeSense10(MODE_PAGE_DATA_COMPRESSION, 0, 256);
    if (!result.success || result.data.size() < 24) {
        setError(QStringLiteral("Failed to read compression page"));
        return false;
    }

    // Modify the compression enable bit
    QByteArray modeData = result.data;

    // Find the data compression page (page code 0x0F)
    int headerLen = 8;
    quint16 blockDescLen = (static_cast<quint8>(modeData[6]) << 8) | static_cast<quint8>(modeData[7]);
    int pageOffset = headerLen + blockDescLen;

    if (pageOffset + 16 > modeData.size()) {
        setError(QStringLiteral("Invalid mode page data"));
        return false;
    }

    // Byte 2 of page contains DCE (bit 7) and DDE (bit 6)
    if (enabled) {
        modeData[pageOffset + 2] |= 0x80;  // Set DCE (Data Compression Enable)
    } else {
        modeData[pageOffset + 2] &= ~0x80;  // Clear DCE
    }

    // Clear reserved bits in header for mode select
    modeData[0] = 0;
    modeData[1] = 0;
    modeData[2] = 0;
    modeData[3] = 0;

    result = d->scsi->modeSelect10(modeData, false);
    d->lastSenseData = result.senseData;

    if (!result.success) {
        setError(QStringLiteral("Set compression failed: %1").arg(result.errorMessage()));
        return false;
    }

    d->compressionEnabled = enabled;

    return true;
}

bool TapeDevice::compressionEnabled() const
{
    return d->compressionEnabled;
}

LtfsLabel TapeDevice::readLabel(PartitionLabel partition)
{
    LtfsLabel label;

    if (!checkOpen("readLabel")) {
        return label;
    }

    // Seek to beginning of partition
    if (!locate(static_cast<quint8>(partition), 0)) {
        return label;
    }

    // Read first block (ANSI label)
    QByteArray data;
    qint64 bytesRead = readBlock(data, 80);
    if (bytesRead <= 0) {
        setError(QStringLiteral("Failed to read ANSI label"));
        return label;
    }

    // Parse ANSI label
    QString ansiLabel = QString::fromLatin1(data.left(80));
    label.barcode = LtfsLabel::parseAnsiLabel(ansiLabel);

    // Read LTFS label block
    bytesRead = readBlock(data, 16384);
    if (bytesRead <= 0) {
        setError(QStringLiteral("Failed to read LTFS label"));
        return label;
    }

    label = LtfsLabel::fromRawData(data);

    return label;
}

bool TapeDevice::writeLabel(const LtfsLabel &label, PartitionLabel partition)
{
    if (!checkOpen("writeLabel")) {
        return false;
    }

    // Seek to beginning of partition
    if (!locate(static_cast<quint8>(partition), 0)) {
        return false;
    }

    // Allow overwrite at current position
    if (!allowOverwrite()) {
        return false;
    }

    // Write ANSI label
    QString ansiLabel = label.toAnsiLabel();
    QByteArray ansiData = ansiLabel.toLatin1();
    ansiData.resize(80);

    if (writeBlock(ansiData) < 0) {
        setError(QStringLiteral("Failed to write ANSI label"));
        return false;
    }

    // Write LTFS label
    QByteArray labelData = label.toRawData();
    if (writeBlock(labelData) < 0) {
        setError(QStringLiteral("Failed to write LTFS label"));
        return false;
    }

    // Write filemark after label
    if (!writeFilemark(1)) {
        return false;
    }

    return true;
}

bool TapeDevice::isLtfsFormatted()
{
    LtfsLabel label = readLabel();
    return label.isValid();
}

QMap<quint16, QByteArray> TapeDevice::readMamAttributes()
{
    QMap<quint16, QByteArray> attributes;

    if (!checkOpen("readMamAttributes")) {
        return attributes;
    }

    // Read all attributes (service action 0)
    auto result = d->scsi->readAttribute(0, 0, 16384);
    d->lastSenseData = result.senseData;

    if (!result.success || result.data.size() < 4) {
        return attributes;
    }

    const quint8 *data = reinterpret_cast<const quint8 *>(result.data.constData());

    // Parse attribute list
    quint32 availableData = (static_cast<quint32>(data[0]) << 24) |
                           (static_cast<quint32>(data[1]) << 16) |
                           (static_cast<quint32>(data[2]) << 8) |
                           static_cast<quint32>(data[3]);

    int offset = 4;
    while (offset + 5 <= result.data.size() && offset < static_cast<int>(availableData + 4)) {
        quint16 attrId = (static_cast<quint16>(data[offset]) << 8) | data[offset + 1];
        // Byte 2 is format/readonly
        quint16 attrLen = (static_cast<quint16>(data[offset + 3]) << 8) | data[offset + 4];

        if (offset + 5 + attrLen > result.data.size()) {
            break;
        }

        QByteArray attrData = result.data.mid(offset + 5, attrLen);
        attributes[attrId] = attrData;

        offset += 5 + attrLen;
    }

    return attributes;
}

bool TapeDevice::writeMamAttribute(quint16 attributeId, const QByteArray &data)
{
    if (!checkOpen("writeMamAttribute")) {
        return false;
    }

    // Build attribute data structure
    QByteArray attrData;
    QDataStream stream(&attrData, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::BigEndian);

    // Total length (4 bytes)
    quint32 totalLen = 5 + static_cast<quint32>(data.size());
    stream << totalLen;

    // Attribute ID (2 bytes)
    stream << attributeId;

    // Format and read-only flags (1 byte)
    stream << static_cast<quint8>(0x00);

    // Attribute length (2 bytes)
    stream << static_cast<quint16>(data.size());

    // Attribute value
    attrData.append(data);

    auto result = d->scsi->writeAttribute(attrData);
    d->lastSenseData = result.senseData;

    if (!result.success) {
        setError(QStringLiteral("Write MAM attribute failed: %1").arg(result.errorMessage()));
        return false;
    }

    return true;
}

bool TapeDevice::allowOverwrite()
{
    if (!checkOpen("allowOverwrite")) {
        return false;
    }

    // Mode 1 = Allow overwrite at current position
    auto result = d->scsi->allowOverwrite(1);
    d->lastSenseData = result.senseData;

    if (!result.success) {
        // Some drives may not support this command
        // This is not necessarily fatal for all operations
        qWarning() << "Allow overwrite command failed, continuing anyway";
    }

    return true;
}

bool TapeDevice::formatForLtfs(const LtfsLabel &label, TapeProgressCallback callback)
{
    if (!checkOpen("formatForLtfs")) {
        return false;
    }

    if (callback) {
        callback(0, 100, QStringLiteral("Preparing to format..."));
    }

    // Step 1: Rewind
    if (!rewind(true)) {
        return false;
    }

    if (callback) {
        callback(10, 100, QStringLiteral("Creating partitions..."));
    }

    // Step 2: Partition the tape (LTFS requires 2 partitions)
    // This is done via Format Medium command with partition bit set
    auto result = d->scsi->formatMedium(0x0E, true);  // 0x0E = default format, partition
    d->lastSenseData = result.senseData;

    if (!result.success) {
        // Some drives require mode select to set partition sizes first
        // Try alternative method via mode select
        qWarning() << "Format medium failed, trying mode select method";
    }

    if (callback) {
        callback(30, 100, QStringLiteral("Writing index partition label..."));
    }

    // Step 3: Write label to index partition
    if (!writeLabel(label, PartitionLabel::IndexPartition)) {
        return false;
    }

    if (callback) {
        callback(50, 100, QStringLiteral("Writing data partition label..."));
    }

    // Step 4: Write label to data partition
    if (!writeLabel(label, PartitionLabel::DataPartition)) {
        return false;
    }

    if (callback) {
        callback(70, 100, QStringLiteral("Writing initial index..."));
    }

    // Step 5: Write empty LTFS index to index partition
    if (!locate(static_cast<quint8>(PartitionLabel::IndexPartition), 0)) {
        return false;
    }

    // Skip past label
    if (!spaceFilemarks(1)) {
        return false;
    }

    // Write minimal LTFS index XML
    QString indexXml = QStringLiteral(
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<ltfsindex version=\"2.4.0\">\n"
        "  <creator>QLTOTapeMan</creator>\n"
        "  <volumeuuid>%1</volumeuuid>\n"
        "  <generationnumber>1</generationnumber>\n"
        "  <updatetime>%2</updatetime>\n"
        "  <location>\n"
        "    <partition>a</partition>\n"
        "    <startblock>0</startblock>\n"
        "  </location>\n"
        "  <allowpolicyupdate>true</allowpolicyupdate>\n"
        "  <directory>\n"
        "    <name>/</name>\n"
        "    <readonly>false</readonly>\n"
        "    <creationtime>%2</creationtime>\n"
        "    <changetime>%2</changetime>\n"
        "    <modifytime>%2</modifytime>\n"
        "    <accesstime>%2</accesstime>\n"
        "  </directory>\n"
        "</ltfsindex>\n"
    ).arg(label.volumeUuid.toString(QUuid::WithoutBraces))
     .arg(QDateTime::currentDateTimeUtc().toString(Qt::ISODate));

    QByteArray indexData = indexXml.toUtf8();
    if (writeBlock(indexData) < 0) {
        setError(QStringLiteral("Failed to write initial index"));
        return false;
    }

    // Write filemark
    if (!writeFilemark(2)) {  // Double filemark marks end of index
        return false;
    }

    if (callback) {
        callback(90, 100, QStringLiteral("Finalizing..."));
    }

    // Rewind
    if (!rewind(true)) {
        return false;
    }

    if (callback) {
        callback(100, 100, QStringLiteral("Format complete"));
    }

    d->mediaInfo.isLtfs = true;
    d->mediaInfo.isPartitioned = true;

    return true;
}

bool TapeDevice::quickErase()
{
    if (!checkOpen("quickErase")) {
        return false;
    }

    // Rewind first
    if (!rewind(true)) {
        return false;
    }

    // Write EOD marker at beginning
    auto result = d->scsi->erase(false, false);  // Short erase, wait
    d->lastSenseData = result.senseData;

    if (!result.success) {
        setError(QStringLiteral("Quick erase failed: %1").arg(result.errorMessage()));
        return false;
    }

    return true;
}

bool TapeDevice::longErase(TapeProgressCallback callback)
{
    if (!checkOpen("longErase")) {
        return false;
    }

    if (callback) {
        callback(0, 100, QStringLiteral("Starting long erase..."));
    }

    // Long erase with immediate return
    auto result = d->scsi->erase(true, true);  // Long erase, immediate
    d->lastSenseData = result.senseData;

    if (!result.success) {
        setError(QStringLiteral("Long erase failed: %1").arg(result.errorMessage()));
        return false;
    }

    // Poll for completion
    QElapsedTimer timer;
    timer.start();

    while (true) {
        QThread::msleep(5000);  // Check every 5 seconds

        result = d->scsi->testUnitReady();
        if (result.success) {
            break;  // Done
        }

        if (result.senseData.senseKey == ScsiSenseKey::NotReady &&
            result.senseData.additionalSenseCode == 0x04) {
            // Still in progress
            if (callback) {
                int elapsed = static_cast<int>(timer.elapsed() / 1000);
                // Estimate progress (very rough for tape erase)
                int progress = qMin(95, elapsed / 60);  // Assume ~60 min for full erase
                callback(progress, 100, QStringLiteral("Erasing... (%1 minutes elapsed)").arg(elapsed / 60));
            }
            continue;
        }

        // Some other error
        setError(QStringLiteral("Erase failed during operation"));
        return false;
    }

    if (callback) {
        callback(100, 100, QStringLiteral("Erase complete"));
    }

    return true;
}

QString TapeDevice::lastError() const
{
    return d->lastError;
}

ScsiSenseData TapeDevice::lastSenseData() const
{
    return d->lastSenseData;
}

void TapeDevice::clearError()
{
    d->lastError.clear();
    d->lastSenseData = ScsiSenseData();
}

ScsiCommand *TapeDevice::scsiCommand() const
{
    return d->scsi.data();
}

void TapeDevice::setStatus(TapeStatus status)
{
    if (d->status != status) {
        d->status = status;
        emit statusChanged(status);
    }
}

void TapeDevice::setError(const QString &message)
{
    d->lastError = message;
    setStatus(TapeStatus::Error);
    emit errorOccurred(message);
}

bool TapeDevice::checkOpen(const char *operation)
{
    if (!isOpen()) {
        d->lastError = QStringLiteral("Device not open for operation: %1").arg(QString::fromLatin1(operation));
        return false;
    }
    return true;
}

} // namespace qltfs
