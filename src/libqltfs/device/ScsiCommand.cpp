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

#include "ScsiCommand.h"

#include <QDebug>

#ifdef Q_OS_WIN
#include <windows.h>
#include <winioctl.h>
#include <ntddscsi.h>
#elif defined(Q_OS_LINUX)
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <scsi/sg.h>
#include <scsi/scsi.h>
#endif

namespace qltfs {

// SCSI status codes
static constexpr quint8 SCSI_STATUS_GOOD = 0x00;
static constexpr quint8 SCSI_STATUS_CHECK_CONDITION = 0x02;
static constexpr quint8 SCSI_STATUS_BUSY = 0x08;
static constexpr quint8 SCSI_STATUS_RESERVATION_CONFLICT = 0x18;

// Default timeout in seconds
static constexpr int DEFAULT_TIMEOUT = 60;

// Maximum CDB length
static constexpr int MAX_CDB_LENGTH = 16;

// Sense data buffer size
static constexpr int SENSE_BUFFER_SIZE = 252;

// ============================================================================
// ScsiSenseData Implementation
// ============================================================================

bool ScsiSenseData::isError() const
{
    if (!valid) {
        return false;
    }

    switch (senseKey) {
    case ScsiSenseKey::NoSense:
    case ScsiSenseKey::RecoveredError:
    case ScsiSenseKey::Completed:
        return false;
    default:
        return true;
    }
}

QString ScsiSenseData::toString() const
{
    if (!valid) {
        return QStringLiteral("No sense data");
    }

    QString senseKeyStr;
    switch (senseKey) {
    case ScsiSenseKey::NoSense:        senseKeyStr = QStringLiteral("No Sense"); break;
    case ScsiSenseKey::RecoveredError: senseKeyStr = QStringLiteral("Recovered Error"); break;
    case ScsiSenseKey::NotReady:       senseKeyStr = QStringLiteral("Not Ready"); break;
    case ScsiSenseKey::MediumError:    senseKeyStr = QStringLiteral("Medium Error"); break;
    case ScsiSenseKey::HardwareError:  senseKeyStr = QStringLiteral("Hardware Error"); break;
    case ScsiSenseKey::IllegalRequest: senseKeyStr = QStringLiteral("Illegal Request"); break;
    case ScsiSenseKey::UnitAttention:  senseKeyStr = QStringLiteral("Unit Attention"); break;
    case ScsiSenseKey::DataProtect:    senseKeyStr = QStringLiteral("Data Protect"); break;
    case ScsiSenseKey::BlankCheck:     senseKeyStr = QStringLiteral("Blank Check"); break;
    case ScsiSenseKey::VendorSpecific: senseKeyStr = QStringLiteral("Vendor Specific"); break;
    case ScsiSenseKey::CopyAborted:    senseKeyStr = QStringLiteral("Copy Aborted"); break;
    case ScsiSenseKey::AbortedCommand: senseKeyStr = QStringLiteral("Aborted Command"); break;
    case ScsiSenseKey::VolumeOverflow: senseKeyStr = QStringLiteral("Volume Overflow"); break;
    case ScsiSenseKey::Miscompare:     senseKeyStr = QStringLiteral("Miscompare"); break;
    case ScsiSenseKey::Completed:      senseKeyStr = QStringLiteral("Completed"); break;
    }

    return QStringLiteral("Sense Key: %1, ASC: 0x%2, ASCQ: 0x%3")
        .arg(senseKeyStr)
        .arg(additionalSenseCode, 2, 16, QLatin1Char('0'))
        .arg(additionalSenseCodeQualifier, 2, 16, QLatin1Char('0'));
}

ScsiSenseData ScsiSenseData::fromRawData(const QByteArray &data)
{
    ScsiSenseData sense;
    sense.rawData = data;

    if (data.size() < 8) {
        return sense;
    }

    const quint8 *bytes = reinterpret_cast<const quint8 *>(data.constData());

    // Response code is in bits 0-6 of byte 0
    quint8 responseCode = bytes[0] & 0x7F;

    if (responseCode == 0x70 || responseCode == 0x71) {
        // Fixed format sense data
        sense.valid = (bytes[0] & 0x80) != 0;
        sense.senseKey = static_cast<ScsiSenseKey>(bytes[2] & 0x0F);

        // Information field (bytes 3-6)
        sense.information = (static_cast<quint32>(bytes[3]) << 24) |
                           (static_cast<quint32>(bytes[4]) << 16) |
                           (static_cast<quint32>(bytes[5]) << 8) |
                           static_cast<quint32>(bytes[6]);

        // Additional sense length is in byte 7
        int additionalLength = bytes[7];
        if (data.size() >= 13 && additionalLength >= 6) {
            sense.additionalSenseCode = bytes[12];
            sense.additionalSenseCodeQualifier = bytes[13];
        }

        sense.valid = true;
    } else if (responseCode == 0x72 || responseCode == 0x73) {
        // Descriptor format sense data
        sense.senseKey = static_cast<ScsiSenseKey>(bytes[1] & 0x0F);
        sense.additionalSenseCode = bytes[2];
        sense.additionalSenseCodeQualifier = bytes[3];
        sense.valid = true;
    }

    return sense;
}

// ============================================================================
// ScsiCommandResult Implementation
// ============================================================================

QString ScsiCommandResult::errorMessage() const
{
    if (success) {
        return QString();
    }

    QString msg;
    if (hostStatus != 0) {
        msg = QStringLiteral("Host error: %1").arg(hostStatus);
    } else if (driverStatus != 0) {
        msg = QStringLiteral("Driver error: %1").arg(driverStatus);
    } else if (scsiStatus != SCSI_STATUS_GOOD) {
        msg = QStringLiteral("SCSI status: 0x%1").arg(scsiStatus, 2, 16, QLatin1Char('0'));
        if (senseData.valid) {
            msg += QStringLiteral(" - ") + senseData.toString();
        }
    } else {
        msg = QStringLiteral("Unknown error");
    }

    return msg;
}

// ============================================================================
// ScsiCommand Private Implementation
// ============================================================================

class ScsiCommand::Private
{
public:
    QString devicePath;
    int timeoutSeconds = DEFAULT_TIMEOUT;

#ifdef Q_OS_WIN
    HANDLE hDevice = INVALID_HANDLE_VALUE;
#elif defined(Q_OS_LINUX)
    int fd = -1;
#endif

    bool isOpen() const
    {
#ifdef Q_OS_WIN
        return hDevice != INVALID_HANDLE_VALUE;
#elif defined(Q_OS_LINUX)
        return fd >= 0;
#else
        return false;
#endif
    }

    ScsiCommandResult execute(const QByteArray &cdb,
                              ScsiDataDirection direction,
                              QByteArray &data,
                              quint32 dataLength);
};

ScsiCommandResult ScsiCommand::Private::execute(const QByteArray &cdb,
                                                 ScsiDataDirection direction,
                                                 QByteArray &data,
                                                 quint32 dataLength)
{
    ScsiCommandResult result;

    if (!isOpen()) {
        result.success = false;
        return result;
    }

#ifdef Q_OS_WIN
    // Windows implementation using SCSI_PASS_THROUGH_DIRECT
    struct SPT_WITH_SENSE {
        SCSI_PASS_THROUGH_DIRECT spt;
        UCHAR senseBuffer[SENSE_BUFFER_SIZE];
    };

    SPT_WITH_SENSE sptd = {};
    sptd.spt.Length = sizeof(SCSI_PASS_THROUGH_DIRECT);
    sptd.spt.CdbLength = static_cast<UCHAR>(cdb.size());
    sptd.spt.SenseInfoLength = SENSE_BUFFER_SIZE;
    sptd.spt.SenseInfoOffset = offsetof(SPT_WITH_SENSE, senseBuffer);
    sptd.spt.TimeOutValue = timeoutSeconds;

    switch (direction) {
    case ScsiDataDirection::None:
        sptd.spt.DataIn = SCSI_IOCTL_DATA_UNSPECIFIED;
        break;
    case ScsiDataDirection::ToDevice:
        sptd.spt.DataIn = SCSI_IOCTL_DATA_OUT;
        break;
    case ScsiDataDirection::FromDevice:
        sptd.spt.DataIn = SCSI_IOCTL_DATA_IN;
        break;
    }

    // Set up data buffer
    if (direction == ScsiDataDirection::FromDevice) {
        data.resize(static_cast<int>(dataLength));
        data.fill(0);
    }

    if (direction != ScsiDataDirection::None && !data.isEmpty()) {
        sptd.spt.DataBuffer = data.data();
        sptd.spt.DataTransferLength = static_cast<ULONG>(data.size());
    } else {
        sptd.spt.DataBuffer = nullptr;
        sptd.spt.DataTransferLength = 0;
    }

    // Copy CDB
    memcpy(sptd.spt.Cdb, cdb.constData(), qMin(cdb.size(), MAX_CDB_LENGTH));

    DWORD bytesReturned = 0;
    BOOL ok = DeviceIoControl(
        hDevice,
        IOCTL_SCSI_PASS_THROUGH_DIRECT,
        &sptd,
        sizeof(sptd),
        &sptd,
        sizeof(sptd),
        &bytesReturned,
        nullptr
    );

    if (!ok) {
        result.success = false;
        result.hostStatus = static_cast<int>(GetLastError());
        return result;
    }

    result.scsiStatus = sptd.spt.ScsiStatus;
    result.bytesTransferred = sptd.spt.DataTransferLength;

    // Parse sense data if present
    if (sptd.spt.SenseInfoLength > 0) {
        QByteArray senseBytes(reinterpret_cast<const char *>(sptd.senseBuffer),
                             sptd.spt.SenseInfoLength);
        result.senseData = ScsiSenseData::fromRawData(senseBytes);
    }

    result.success = (result.scsiStatus == SCSI_STATUS_GOOD);
    if (direction == ScsiDataDirection::FromDevice) {
        result.data = data;
    }

#elif defined(Q_OS_LINUX)
    // Linux implementation using SG_IO ioctl
    sg_io_hdr_t io_hdr = {};
    unsigned char senseBuffer[SENSE_BUFFER_SIZE] = {0};

    io_hdr.interface_id = 'S';
    io_hdr.cmd_len = static_cast<unsigned char>(cdb.size());
    io_hdr.cmdp = reinterpret_cast<unsigned char *>(const_cast<char *>(cdb.constData()));
    io_hdr.mx_sb_len = SENSE_BUFFER_SIZE;
    io_hdr.sbp = senseBuffer;
    io_hdr.timeout = timeoutSeconds * 1000; // Convert to milliseconds

    switch (direction) {
    case ScsiDataDirection::None:
        io_hdr.dxfer_direction = SG_DXFER_NONE;
        break;
    case ScsiDataDirection::ToDevice:
        io_hdr.dxfer_direction = SG_DXFER_TO_DEV;
        break;
    case ScsiDataDirection::FromDevice:
        io_hdr.dxfer_direction = SG_DXFER_FROM_DEV;
        break;
    }

    // Set up data buffer
    if (direction == ScsiDataDirection::FromDevice) {
        data.resize(static_cast<int>(dataLength));
        data.fill(0);
    }

    if (direction != ScsiDataDirection::None && !data.isEmpty()) {
        io_hdr.dxferp = data.data();
        io_hdr.dxfer_len = static_cast<unsigned int>(data.size());
    }

    int ret = ioctl(fd, SG_IO, &io_hdr);
    if (ret < 0) {
        result.success = false;
        result.hostStatus = errno;
        return result;
    }

    result.hostStatus = io_hdr.host_status;
    result.driverStatus = io_hdr.driver_status;
    result.scsiStatus = io_hdr.status;
    result.residual = io_hdr.resid;
    result.bytesTransferred = dataLength - io_hdr.resid;

    // Parse sense data
    if (io_hdr.sb_len_wr > 0) {
        QByteArray senseBytes(reinterpret_cast<const char *>(senseBuffer),
                             io_hdr.sb_len_wr);
        result.senseData = ScsiSenseData::fromRawData(senseBytes);
    }

    result.success = (io_hdr.host_status == 0 &&
                     io_hdr.driver_status == 0 &&
                     result.scsiStatus == SCSI_STATUS_GOOD);

    if (direction == ScsiDataDirection::FromDevice) {
        data.resize(static_cast<int>(result.bytesTransferred));
        result.data = data;
    }

#else
    // Unsupported platform
    Q_UNUSED(cdb)
    Q_UNUSED(direction)
    Q_UNUSED(data)
    Q_UNUSED(dataLength)
    result.success = false;
#endif

    return result;
}

// ============================================================================
// ScsiCommand Implementation
// ============================================================================

ScsiCommand::ScsiCommand(const QString &devicePath)
    : d(new Private)
{
    d->devicePath = devicePath;
}

ScsiCommand::~ScsiCommand()
{
    close();
    delete d;
}

bool ScsiCommand::open()
{
    if (isOpen()) {
        return true;
    }

#ifdef Q_OS_WIN
    // Windows: Open device with GENERIC_READ | GENERIC_WRITE
    d->hDevice = CreateFileW(
        reinterpret_cast<LPCWSTR>(d->devicePath.utf16()),
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_EXISTING,
        0,
        nullptr
    );

    return d->hDevice != INVALID_HANDLE_VALUE;

#elif defined(Q_OS_LINUX)
    // Linux: Open device
    d->fd = ::open(d->devicePath.toLocal8Bit().constData(), O_RDWR | O_NONBLOCK);
    return d->fd >= 0;

#else
    return false;
#endif
}

void ScsiCommand::close()
{
#ifdef Q_OS_WIN
    if (d->hDevice != INVALID_HANDLE_VALUE) {
        CloseHandle(d->hDevice);
        d->hDevice = INVALID_HANDLE_VALUE;
    }
#elif defined(Q_OS_LINUX)
    if (d->fd >= 0) {
        ::close(d->fd);
        d->fd = -1;
    }
#endif
}

bool ScsiCommand::isOpen() const
{
    return d->isOpen();
}

QString ScsiCommand::devicePath() const
{
    return d->devicePath;
}

void ScsiCommand::setTimeout(int seconds)
{
    d->timeoutSeconds = seconds;
}

int ScsiCommand::timeout() const
{
    return d->timeoutSeconds;
}

ScsiCommandResult ScsiCommand::testUnitReady()
{
    QByteArray cdb(6, 0);
    cdb[0] = static_cast<char>(ScsiOpCode::TestUnitReady);

    QByteArray data;
    return d->execute(cdb, ScsiDataDirection::None, data, 0);
}

ScsiCommandResult ScsiCommand::inquiry(bool evpd, quint8 pageCode, quint16 allocationLength)
{
    QByteArray cdb(6, 0);
    cdb[0] = static_cast<char>(ScsiOpCode::Inquiry);
    cdb[1] = evpd ? 0x01 : 0x00;
    cdb[2] = static_cast<char>(pageCode);
    cdb[3] = static_cast<char>((allocationLength >> 8) & 0xFF);
    cdb[4] = static_cast<char>(allocationLength & 0xFF);

    QByteArray data;
    return d->execute(cdb, ScsiDataDirection::FromDevice, data, allocationLength);
}

ScsiCommandResult ScsiCommand::requestSense(quint8 allocationLength)
{
    QByteArray cdb(6, 0);
    cdb[0] = static_cast<char>(ScsiOpCode::RequestSense);
    cdb[4] = static_cast<char>(allocationLength);

    QByteArray data;
    return d->execute(cdb, ScsiDataDirection::FromDevice, data, allocationLength);
}

ScsiCommandResult ScsiCommand::readBlockLimits()
{
    QByteArray cdb(6, 0);
    cdb[0] = static_cast<char>(ScsiOpCode::ReadBlockLimits);

    QByteArray data;
    return d->execute(cdb, ScsiDataDirection::FromDevice, data, 6);
}

ScsiCommandResult ScsiCommand::modeSense10(quint8 pageCode, quint8 subPageCode, quint16 allocationLength)
{
    QByteArray cdb(10, 0);
    cdb[0] = static_cast<char>(ScsiOpCode::ModeSense10);
    cdb[2] = static_cast<char>(pageCode & 0x3F);  // Page code
    cdb[3] = static_cast<char>(subPageCode);
    cdb[7] = static_cast<char>((allocationLength >> 8) & 0xFF);
    cdb[8] = static_cast<char>(allocationLength & 0xFF);

    QByteArray data;
    return d->execute(cdb, ScsiDataDirection::FromDevice, data, allocationLength);
}

ScsiCommandResult ScsiCommand::modeSelect10(const QByteArray &data, bool savePages)
{
    QByteArray cdb(10, 0);
    cdb[0] = static_cast<char>(ScsiOpCode::ModeSelect10);
    cdb[1] = 0x10 | (savePages ? 0x01 : 0x00);  // PF=1, SP
    cdb[7] = static_cast<char>((data.size() >> 8) & 0xFF);
    cdb[8] = static_cast<char>(data.size() & 0xFF);

    QByteArray writeData = data;
    return d->execute(cdb, ScsiDataDirection::ToDevice, writeData, static_cast<quint32>(data.size()));
}

ScsiCommandResult ScsiCommand::logSense(quint8 pageCode, quint8 subPageCode, quint16 allocationLength)
{
    QByteArray cdb(10, 0);
    cdb[0] = static_cast<char>(ScsiOpCode::LogSense);
    cdb[2] = static_cast<char>((0x40 | (pageCode & 0x3F)));  // PC=01, Page Code
    cdb[3] = static_cast<char>(subPageCode);
    cdb[7] = static_cast<char>((allocationLength >> 8) & 0xFF);
    cdb[8] = static_cast<char>(allocationLength & 0xFF);

    QByteArray data;
    return d->execute(cdb, ScsiDataDirection::FromDevice, data, allocationLength);
}

ScsiCommandResult ScsiCommand::readPosition(quint8 serviceAction)
{
    QByteArray cdb(10, 0);
    cdb[0] = static_cast<char>(ScsiOpCode::ReadPosition);
    cdb[1] = static_cast<char>(serviceAction & 0x1F);

    quint16 allocationLength = (serviceAction == 0) ? 20 : 32;
    cdb[7] = static_cast<char>((allocationLength >> 8) & 0xFF);
    cdb[8] = static_cast<char>(allocationLength & 0xFF);

    QByteArray data;
    return d->execute(cdb, ScsiDataDirection::FromDevice, data, allocationLength);
}

ScsiCommandResult ScsiCommand::locate16(quint8 partition, quint64 blockNumber)
{
    QByteArray cdb(16, 0);
    cdb[0] = static_cast<char>(ScsiOpCode::Locate16);
    cdb[1] = 0x02;  // Logical position
    cdb[3] = static_cast<char>(partition);

    // Block number (bytes 4-11, big-endian)
    cdb[4] = static_cast<char>((blockNumber >> 56) & 0xFF);
    cdb[5] = static_cast<char>((blockNumber >> 48) & 0xFF);
    cdb[6] = static_cast<char>((blockNumber >> 40) & 0xFF);
    cdb[7] = static_cast<char>((blockNumber >> 32) & 0xFF);
    cdb[8] = static_cast<char>((blockNumber >> 24) & 0xFF);
    cdb[9] = static_cast<char>((blockNumber >> 16) & 0xFF);
    cdb[10] = static_cast<char>((blockNumber >> 8) & 0xFF);
    cdb[11] = static_cast<char>(blockNumber & 0xFF);

    QByteArray data;
    return d->execute(cdb, ScsiDataDirection::None, data, 0);
}

ScsiCommandResult ScsiCommand::rewind(bool immediate)
{
    QByteArray cdb(6, 0);
    cdb[0] = static_cast<char>(ScsiOpCode::Rewind);
    cdb[1] = immediate ? 0x01 : 0x00;

    QByteArray data;
    return d->execute(cdb, ScsiDataDirection::None, data, 0);
}

ScsiCommandResult ScsiCommand::loadUnload(bool load, bool immediate)
{
    QByteArray cdb(6, 0);
    cdb[0] = static_cast<char>(ScsiOpCode::LoadUnload);
    cdb[1] = immediate ? 0x01 : 0x00;
    cdb[4] = load ? 0x01 : 0x00;  // Load/Unload bit

    QByteArray data;
    return d->execute(cdb, ScsiDataDirection::None, data, 0);
}

ScsiCommandResult ScsiCommand::space(quint8 code, qint32 count)
{
    QByteArray cdb(6, 0);
    cdb[0] = static_cast<char>(ScsiOpCode::Space);
    cdb[1] = static_cast<char>(code & 0x07);

    // Count is a signed 24-bit value
    cdb[2] = static_cast<char>((count >> 16) & 0xFF);
    cdb[3] = static_cast<char>((count >> 8) & 0xFF);
    cdb[4] = static_cast<char>(count & 0xFF);

    QByteArray data;
    return d->execute(cdb, ScsiDataDirection::None, data, 0);
}

ScsiCommandResult ScsiCommand::read6(QByteArray &buffer, quint32 blockCount, bool fixedBlock)
{
    QByteArray cdb(6, 0);
    cdb[0] = static_cast<char>(ScsiOpCode::Read6);
    cdb[1] = fixedBlock ? 0x01 : 0x00;

    // Transfer length (24-bit)
    cdb[2] = static_cast<char>((blockCount >> 16) & 0xFF);
    cdb[3] = static_cast<char>((blockCount >> 8) & 0xFF);
    cdb[4] = static_cast<char>(blockCount & 0xFF);

    quint32 dataLength = static_cast<quint32>(buffer.size());
    return d->execute(cdb, ScsiDataDirection::FromDevice, buffer, dataLength);
}

ScsiCommandResult ScsiCommand::write6(const QByteArray &data, bool fixedBlock)
{
    QByteArray cdb(6, 0);
    cdb[0] = static_cast<char>(ScsiOpCode::Write6);
    cdb[1] = fixedBlock ? 0x01 : 0x00;

    // For fixed block mode, count is number of blocks
    // For variable block mode, count is byte count
    quint32 count = static_cast<quint32>(data.size());
    cdb[2] = static_cast<char>((count >> 16) & 0xFF);
    cdb[3] = static_cast<char>((count >> 8) & 0xFF);
    cdb[4] = static_cast<char>(count & 0xFF);

    QByteArray writeData = data;
    return d->execute(cdb, ScsiDataDirection::ToDevice, writeData, static_cast<quint32>(data.size()));
}

ScsiCommandResult ScsiCommand::writeFilemark(quint32 count, bool setMark, bool immediate)
{
    QByteArray cdb(6, 0);
    cdb[0] = static_cast<char>(ScsiOpCode::WriteFilemark);
    cdb[1] = (immediate ? 0x01 : 0x00) | (setMark ? 0x02 : 0x00);

    // Count (24-bit)
    cdb[2] = static_cast<char>((count >> 16) & 0xFF);
    cdb[3] = static_cast<char>((count >> 8) & 0xFF);
    cdb[4] = static_cast<char>(count & 0xFF);

    QByteArray data;
    return d->execute(cdb, ScsiDataDirection::None, data, 0);
}

ScsiCommandResult ScsiCommand::erase(bool longErase, bool immediate)
{
    QByteArray cdb(6, 0);
    cdb[0] = static_cast<char>(ScsiOpCode::Erase);
    cdb[1] = (immediate ? 0x01 : 0x00) | (longErase ? 0x02 : 0x00);

    QByteArray data;
    return d->execute(cdb, ScsiDataDirection::None, data, 0);
}

ScsiCommandResult ScsiCommand::readAttribute(quint8 serviceAction, quint16 attributeId, quint32 allocationLength)
{
    QByteArray cdb(16, 0);
    cdb[0] = static_cast<char>(ScsiOpCode::ReadAttribute);
    cdb[1] = static_cast<char>(serviceAction & 0x1F);

    // Attribute ID
    cdb[8] = static_cast<char>((attributeId >> 8) & 0xFF);
    cdb[9] = static_cast<char>(attributeId & 0xFF);

    // Allocation length
    cdb[10] = static_cast<char>((allocationLength >> 24) & 0xFF);
    cdb[11] = static_cast<char>((allocationLength >> 16) & 0xFF);
    cdb[12] = static_cast<char>((allocationLength >> 8) & 0xFF);
    cdb[13] = static_cast<char>(allocationLength & 0xFF);

    QByteArray data;
    return d->execute(cdb, ScsiDataDirection::FromDevice, data, allocationLength);
}

ScsiCommandResult ScsiCommand::writeAttribute(const QByteArray &data)
{
    QByteArray cdb(16, 0);
    cdb[0] = static_cast<char>(ScsiOpCode::WriteAttribute);
    cdb[1] = 0x01;  // Write through

    quint32 paramLength = static_cast<quint32>(data.size());
    cdb[10] = static_cast<char>((paramLength >> 24) & 0xFF);
    cdb[11] = static_cast<char>((paramLength >> 16) & 0xFF);
    cdb[12] = static_cast<char>((paramLength >> 8) & 0xFF);
    cdb[13] = static_cast<char>(paramLength & 0xFF);

    QByteArray writeData = data;
    return d->execute(cdb, ScsiDataDirection::ToDevice, writeData, paramLength);
}

ScsiCommandResult ScsiCommand::allowOverwrite(quint8 mode)
{
    QByteArray cdb(16, 0);
    cdb[0] = static_cast<char>(ScsiOpCode::AllowOverwrite);
    cdb[2] = static_cast<char>(mode);

    QByteArray data;
    return d->execute(cdb, ScsiDataDirection::None, data, 0);
}

ScsiCommandResult ScsiCommand::preventAllowMediumRemoval(bool prevent)
{
    QByteArray cdb(6, 0);
    cdb[0] = static_cast<char>(ScsiOpCode::PreventAllowMediumRemoval);
    cdb[4] = prevent ? 0x01 : 0x00;

    QByteArray data;
    return d->execute(cdb, ScsiDataDirection::None, data, 0);
}

ScsiCommandResult ScsiCommand::formatMedium(quint8 format, bool partition)
{
    QByteArray cdb(6, 0);
    cdb[0] = static_cast<char>(ScsiOpCode::FormatMedium);
    cdb[2] = static_cast<char>(format & 0x0F);
    if (partition) {
        cdb[2] |= 0x10;  // Format with partition
    }

    QByteArray data;
    return d->execute(cdb, ScsiDataDirection::None, data, 0);
}

ScsiCommandResult ScsiCommand::reportDensitySupport(bool mediaType, quint16 allocationLength)
{
    QByteArray cdb(10, 0);
    cdb[0] = static_cast<char>(ScsiOpCode::ReportDensitySupport);
    cdb[1] = mediaType ? 0x01 : 0x00;
    cdb[7] = static_cast<char>((allocationLength >> 8) & 0xFF);
    cdb[8] = static_cast<char>(allocationLength & 0xFF);

    QByteArray data;
    return d->execute(cdb, ScsiDataDirection::FromDevice, data, allocationLength);
}

ScsiCommandResult ScsiCommand::executeRaw(const QByteArray &cdb,
                                          ScsiDataDirection direction,
                                          QByteArray &data,
                                          quint32 dataLength)
{
    return d->execute(cdb, direction, data, dataLength);
}

} // namespace qltfs
