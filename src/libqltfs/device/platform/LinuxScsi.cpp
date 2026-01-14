/**
 * QLTOTapeMan - Qt-based LTO Tape Manager
 * Linux SCSI Implementation
 *
 * Copyright (c) 2026 Jeffrey ZHU (zhxsh1225@gmail.com)
 * https://github.com/Gypsop/QLTOTapeMan
 */

#include "LinuxScsi.h"

#if defined(Q_OS_LINUX)

#include <QDebug>

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <scsi/sg.h>
#include <scsi/scsi.h>

namespace qltfs {

// =============================================================================
// Constructor / Destructor
// =============================================================================

LinuxScsi::LinuxScsi(const QString& devicePath)
    : m_devicePath(devicePath)
    , m_fd(-1)
    , m_lastErrno(0)
    , m_sgIoSupported(false)
{
    memset(m_senseBuffer, 0, sizeof(m_senseBuffer));
}

LinuxScsi::~LinuxScsi()
{
    close();
}

LinuxScsi::LinuxScsi(LinuxScsi&& other) noexcept
    : m_devicePath(std::move(other.m_devicePath))
    , m_fd(other.m_fd)
    , m_lastErrno(other.m_lastErrno)
    , m_sgIoSupported(other.m_sgIoSupported)
{
    memcpy(m_senseBuffer, other.m_senseBuffer, sizeof(m_senseBuffer));
    other.m_fd = -1;
}

LinuxScsi& LinuxScsi::operator=(LinuxScsi&& other) noexcept
{
    if (this != &other) {
        close();
        m_devicePath = std::move(other.m_devicePath);
        m_fd = other.m_fd;
        m_lastErrno = other.m_lastErrno;
        m_sgIoSupported = other.m_sgIoSupported;
        memcpy(m_senseBuffer, other.m_senseBuffer, sizeof(m_senseBuffer));
        other.m_fd = -1;
    }
    return *this;
}

// =============================================================================
// Device Open / Close
// =============================================================================

bool LinuxScsi::open()
{
    if (isOpen()) {
        return true;
    }

    QByteArray pathBytes = m_devicePath.toLocal8Bit();
    m_fd = ::open(pathBytes.constData(), O_RDWR | O_NONBLOCK);

    if (m_fd < 0) {
        m_lastErrno = errno;
        qWarning() << "Failed to open device" << m_devicePath 
                   << "Error:" << formatErrno(m_lastErrno);
        return false;
    }

    // Check if SG_IO is supported
    m_sgIoSupported = checkDeviceCapabilities();
    
    m_lastErrno = 0;
    return true;
}

void LinuxScsi::close()
{
    if (m_fd >= 0) {
        ::close(m_fd);
        m_fd = -1;
    }
}

bool LinuxScsi::isOpen() const
{
    return m_fd >= 0;
}

bool LinuxScsi::checkDeviceCapabilities()
{
    // Check for SG_IO support by getting the SG version
    int version = 0;
    if (ioctl(m_fd, SG_GET_VERSION_NUM, &version) < 0) {
        // SG_IO might still work on tape devices even if this fails
        return true;
    }
    
    // Version 3.0.0 or later is preferred
    return version >= 30000;
}

// =============================================================================
// Command Execution
// =============================================================================

LinuxScsiResult LinuxScsi::executeCommand(
    const uint8_t* cdb,
    size_t cdbLength,
    uint8_t* data,
    size_t dataLength,
    LinuxDataDirection direction,
    int timeoutSecs,
    uint8_t* senseData,
    size_t senseLength)
{
    LinuxScsiResult result;
    result.success = false;

    if (!isOpen()) {
        result.errorMessage = QStringLiteral("Device not open");
        return result;
    }

    if (cdbLength > 16) {
        result.errorMessage = QStringLiteral("CDB too long (max 16 bytes)");
        return result;
    }

    // Prepare sg_io_hdr structure
    struct sg_io_hdr io_hdr;
    memset(&io_hdr, 0, sizeof(io_hdr));
    
    uint8_t localSenseBuffer[64] = {};
    
    io_hdr.interface_id = 'S';  // Always 'S' for SCSI
    io_hdr.cmd_len = static_cast<unsigned char>(cdbLength);
    io_hdr.cmdp = const_cast<unsigned char*>(cdb);
    io_hdr.sbp = localSenseBuffer;
    io_hdr.mx_sb_len = sizeof(localSenseBuffer);
    io_hdr.timeout = timeoutSecs * 1000;  // Convert to milliseconds

    // Set data direction
    switch (direction) {
    case LinuxDataDirection::NoData:
        io_hdr.dxfer_direction = SG_DXFER_NONE;
        io_hdr.dxferp = nullptr;
        io_hdr.dxfer_len = 0;
        break;
    case LinuxDataDirection::DataIn:
        io_hdr.dxfer_direction = SG_DXFER_FROM_DEV;
        io_hdr.dxferp = data;
        io_hdr.dxfer_len = static_cast<unsigned int>(dataLength);
        break;
    case LinuxDataDirection::DataOut:
        io_hdr.dxfer_direction = SG_DXFER_TO_DEV;
        io_hdr.dxferp = const_cast<uint8_t*>(data);
        io_hdr.dxfer_len = static_cast<unsigned int>(dataLength);
        break;
    }

    // Execute the command
    if (ioctl(m_fd, SG_IO, &io_hdr) < 0) {
        m_lastErrno = errno;
        result.errorMessage = QStringLiteral("SG_IO ioctl failed: ") + formatErrno(m_lastErrno);
        return result;
    }

    // Check for driver/host errors
    if (io_hdr.host_status != 0) {
        result.errorMessage = QStringLiteral("Host error: %1").arg(io_hdr.host_status);
        return result;
    }
    
    if (io_hdr.driver_status != 0 && (io_hdr.driver_status & 0x0F) != 0x08) {
        // 0x08 is DRIVER_SENSE which is normal when sense data is present
        result.errorMessage = QStringLiteral("Driver error: %1").arg(io_hdr.driver_status);
        return result;
    }

    // Check SCSI status
    result.scsiStatus = io_hdr.status;
    result.dataTransferred = io_hdr.dxfer_len - io_hdr.resid;

    if (io_hdr.status == 0) {
        // GOOD status
        result.success = true;
    } else if (io_hdr.status == 0x02) {
        // CHECK CONDITION - parse sense data
        parseSenseData(localSenseBuffer, sizeof(localSenseBuffer), result);
        
        // Copy sense data to caller if requested
        if (senseData && senseLength > 0) {
            size_t copyLen = qMin(senseLength, sizeof(localSenseBuffer));
            memcpy(senseData, localSenseBuffer, copyLen);
        }
    } else if (io_hdr.status == 0x08) {
        // BUSY
        result.errorMessage = QStringLiteral("Device busy");
    } else if (io_hdr.status == 0x18) {
        // RESERVATION CONFLICT
        result.errorMessage = QStringLiteral("Reservation conflict");
    }

    // Store sense data internally
    memcpy(m_senseBuffer, localSenseBuffer, sizeof(m_senseBuffer));

    return result;
}

void LinuxScsi::parseSenseData(const uint8_t* senseData, size_t length, LinuxScsiResult& result)
{
    if (!senseData || length < 14) {
        result.errorMessage = QStringLiteral("Invalid sense data");
        return;
    }

    // Parse fixed format sense data (70h/71h)
    uint8_t responseCode = senseData[0] & 0x7F;
    
    if (responseCode == 0x70 || responseCode == 0x71) {
        result.senseKey = senseData[2] & 0x0F;
        result.asc = senseData[12];
        result.ascq = senseData[13];
    }
    // Parse descriptor format sense data (72h/73h)
    else if (responseCode == 0x72 || responseCode == 0x73) {
        result.senseKey = senseData[1] & 0x0F;
        result.asc = senseData[2];
        result.ascq = senseData[3];
    }

    // Generate error message from sense data
    result.errorMessage = QStringLiteral("SCSI Error: Sense=%1 ASC=%2 ASCQ=%3")
        .arg(result.senseKey, 2, 16, QLatin1Char('0'))
        .arg(result.asc, 2, 16, QLatin1Char('0'))
        .arg(result.ascq, 2, 16, QLatin1Char('0'));
}

QString LinuxScsi::formatErrno(int errnoValue)
{
    return QString::fromLocal8Bit(strerror(errnoValue));
}

// =============================================================================
// SCSI Tape Commands
// =============================================================================

LinuxScsiResult LinuxScsi::testUnitReady()
{
    uint8_t cdb[6] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }; // TEST UNIT READY
    return executeCommand(cdb, sizeof(cdb), nullptr, 0, LinuxDataDirection::NoData);
}

LinuxScsiResult LinuxScsi::inquiry(uint8_t* inquiryData, size_t length)
{
    uint8_t cdb[6] = {
        0x12,                              // INQUIRY
        0x00,
        0x00,
        static_cast<uint8_t>((length >> 8) & 0xFF),
        static_cast<uint8_t>(length & 0xFF),
        0x00
    };
    return executeCommand(cdb, sizeof(cdb), inquiryData, length, LinuxDataDirection::DataIn);
}

LinuxScsiResult LinuxScsi::readBlockLimits(uint32_t& minBlockSize, uint32_t& maxBlockSize)
{
    uint8_t buffer[6] = {};
    uint8_t cdb[6] = { 0x05, 0x00, 0x00, 0x00, 0x00, 0x00 }; // READ BLOCK LIMITS
    
    LinuxScsiResult result = executeCommand(cdb, sizeof(cdb), buffer, sizeof(buffer), LinuxDataDirection::DataIn);
    
    if (result.success) {
        maxBlockSize = (static_cast<uint32_t>(buffer[1]) << 16) |
                       (static_cast<uint32_t>(buffer[2]) << 8) |
                       static_cast<uint32_t>(buffer[3]);
        minBlockSize = (static_cast<uint32_t>(buffer[4]) << 8) |
                       static_cast<uint32_t>(buffer[5]);
    }
    
    return result;
}

LinuxScsiResult LinuxScsi::requestSense(uint8_t* senseBuffer, size_t length)
{
    uint8_t cdb[6] = {
        0x03,                              // REQUEST SENSE
        0x00,
        0x00,
        0x00,
        static_cast<uint8_t>(length & 0xFF),
        0x00
    };
    return executeCommand(cdb, sizeof(cdb), senseBuffer, length, LinuxDataDirection::DataIn);
}

LinuxScsiResult LinuxScsi::read6(uint8_t* buffer, size_t length, bool fixed, size_t transferLength)
{
    uint8_t cdb[6] = {
        0x08,                              // READ(6)
        static_cast<uint8_t>(fixed ? 0x01 : 0x00),
        static_cast<uint8_t>((transferLength >> 16) & 0xFF),
        static_cast<uint8_t>((transferLength >> 8) & 0xFF),
        static_cast<uint8_t>(transferLength & 0xFF),
        0x00
    };
    return executeCommand(cdb, sizeof(cdb), buffer, length, LinuxDataDirection::DataIn, 300);
}

LinuxScsiResult LinuxScsi::write6(const uint8_t* buffer, size_t length, bool fixed, size_t transferLength)
{
    uint8_t cdb[6] = {
        0x0A,                              // WRITE(6)
        static_cast<uint8_t>(fixed ? 0x01 : 0x00),
        static_cast<uint8_t>((transferLength >> 16) & 0xFF),
        static_cast<uint8_t>((transferLength >> 8) & 0xFF),
        static_cast<uint8_t>(transferLength & 0xFF),
        0x00
    };
    return executeCommand(cdb, sizeof(cdb), const_cast<uint8_t*>(buffer), length, LinuxDataDirection::DataOut, 300);
}

LinuxScsiResult LinuxScsi::readPosition(LinuxTapePosition& position)
{
    uint8_t buffer[20] = {};
    uint8_t cdb[10] = {
        0x34,                              // READ POSITION
        0x00,                              // Short form
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };
    
    LinuxScsiResult result = executeCommand(cdb, sizeof(cdb), buffer, sizeof(buffer), LinuxDataDirection::DataIn);
    
    if (result.success) {
        position.partition = buffer[1];
        position.blockPosition = (static_cast<uint32_t>(buffer[4]) << 24) |
                                 (static_cast<uint32_t>(buffer[5]) << 16) |
                                 (static_cast<uint32_t>(buffer[6]) << 8) |
                                 static_cast<uint32_t>(buffer[7]);
        position.filePosition = (static_cast<uint32_t>(buffer[8]) << 24) |
                                (static_cast<uint32_t>(buffer[9]) << 16) |
                                (static_cast<uint32_t>(buffer[10]) << 8) |
                                static_cast<uint32_t>(buffer[11]);
        position.bop = (buffer[0] & 0x80) != 0;
        position.eop = (buffer[0] & 0x40) != 0;
    }
    
    return result;
}

LinuxScsiResult LinuxScsi::locate10(uint32_t blockAddress, uint8_t partition)
{
    uint8_t cdb[10] = {
        0x2B,                              // LOCATE(10)
        static_cast<uint8_t>(partition != 0 ? 0x02 : 0x00), // CP bit
        0x00,
        static_cast<uint8_t>((blockAddress >> 24) & 0xFF),
        static_cast<uint8_t>((blockAddress >> 16) & 0xFF),
        static_cast<uint8_t>((blockAddress >> 8) & 0xFF),
        static_cast<uint8_t>(blockAddress & 0xFF),
        0x00,
        partition,
        0x00
    };
    return executeCommand(cdb, sizeof(cdb), nullptr, 0, LinuxDataDirection::NoData, 600);
}

LinuxScsiResult LinuxScsi::locate16(uint64_t blockAddress, uint8_t partition)
{
    uint8_t cdb[16] = {
        0x92,                              // LOCATE(16)
        static_cast<uint8_t>(partition != 0 ? 0x02 : 0x00), // CP bit
        0x00,
        partition,
        static_cast<uint8_t>((blockAddress >> 56) & 0xFF),
        static_cast<uint8_t>((blockAddress >> 48) & 0xFF),
        static_cast<uint8_t>((blockAddress >> 40) & 0xFF),
        static_cast<uint8_t>((blockAddress >> 32) & 0xFF),
        static_cast<uint8_t>((blockAddress >> 24) & 0xFF),
        static_cast<uint8_t>((blockAddress >> 16) & 0xFF),
        static_cast<uint8_t>((blockAddress >> 8) & 0xFF),
        static_cast<uint8_t>(blockAddress & 0xFF),
        0x00, 0x00, 0x00, 0x00
    };
    return executeCommand(cdb, sizeof(cdb), nullptr, 0, LinuxDataDirection::NoData, 600);
}

LinuxScsiResult LinuxScsi::rewind()
{
    uint8_t cdb[6] = { 0x01, 0x00, 0x00, 0x00, 0x00, 0x00 }; // REWIND
    return executeCommand(cdb, sizeof(cdb), nullptr, 0, LinuxDataDirection::NoData, 1800);
}

LinuxScsiResult LinuxScsi::space(LinuxSpaceCode code, int32_t count)
{
    uint8_t cdb[6] = {
        0x11,                              // SPACE
        static_cast<uint8_t>(code),
        static_cast<uint8_t>((count >> 16) & 0xFF),
        static_cast<uint8_t>((count >> 8) & 0xFF),
        static_cast<uint8_t>(count & 0xFF),
        0x00
    };
    return executeCommand(cdb, sizeof(cdb), nullptr, 0, LinuxDataDirection::NoData, 1800);
}

LinuxScsiResult LinuxScsi::writeFilemarks(uint32_t count, bool setmark)
{
    uint8_t cdb[6] = {
        0x10,                              // WRITE FILEMARKS
        static_cast<uint8_t>(setmark ? 0x02 : 0x00),
        static_cast<uint8_t>((count >> 16) & 0xFF),
        static_cast<uint8_t>((count >> 8) & 0xFF),
        static_cast<uint8_t>(count & 0xFF),
        0x00
    };
    return executeCommand(cdb, sizeof(cdb), nullptr, 0, LinuxDataDirection::NoData, 300);
}

LinuxScsiResult LinuxScsi::loadUnload(bool load)
{
    uint8_t cdb[6] = {
        0x1B,                              // LOAD/UNLOAD
        0x00,
        0x00,
        0x00,
        static_cast<uint8_t>(load ? 0x01 : 0x00),
        0x00
    };
    return executeCommand(cdb, sizeof(cdb), nullptr, 0, LinuxDataDirection::NoData, 600);
}

LinuxScsiResult LinuxScsi::erase(bool longErase)
{
    uint8_t cdb[6] = {
        0x19,                              // ERASE
        static_cast<uint8_t>(longErase ? 0x01 : 0x00),
        0x00, 0x00, 0x00, 0x00
    };
    // Long erase can take hours
    int timeout = longErase ? 86400 : 3600; // 24h or 1h
    return executeCommand(cdb, sizeof(cdb), nullptr, 0, LinuxDataDirection::NoData, timeout);
}

LinuxScsiResult LinuxScsi::modeSense6(uint8_t pageCode, uint8_t* buffer, size_t length)
{
    uint8_t cdb[6] = {
        0x1A,                              // MODE SENSE(6)
        0x00,
        static_cast<uint8_t>(pageCode & 0x3F),
        0x00,
        static_cast<uint8_t>(length & 0xFF),
        0x00
    };
    return executeCommand(cdb, sizeof(cdb), buffer, length, LinuxDataDirection::DataIn);
}

LinuxScsiResult LinuxScsi::modeSense10(uint8_t pageCode, uint8_t* buffer, size_t length)
{
    uint8_t cdb[10] = {
        0x5A,                              // MODE SENSE(10)
        0x00,
        static_cast<uint8_t>(pageCode & 0x3F),
        0x00, 0x00, 0x00, 0x00,
        static_cast<uint8_t>((length >> 8) & 0xFF),
        static_cast<uint8_t>(length & 0xFF),
        0x00
    };
    return executeCommand(cdb, sizeof(cdb), buffer, length, LinuxDataDirection::DataIn);
}

LinuxScsiResult LinuxScsi::modeSelect6(const uint8_t* buffer, size_t length, bool savePage)
{
    uint8_t cdb[6] = {
        0x15,                              // MODE SELECT(6)
        static_cast<uint8_t>(0x10 | (savePage ? 0x01 : 0x00)), // PF bit + SP bit
        0x00,
        0x00,
        static_cast<uint8_t>(length & 0xFF),
        0x00
    };
    return executeCommand(cdb, sizeof(cdb), const_cast<uint8_t*>(buffer), length, LinuxDataDirection::DataOut);
}

LinuxScsiResult LinuxScsi::modeSelect10(const uint8_t* buffer, size_t length, bool savePage)
{
    uint8_t cdb[10] = {
        0x55,                              // MODE SELECT(10)
        static_cast<uint8_t>(0x10 | (savePage ? 0x01 : 0x00)), // PF bit + SP bit
        0x00, 0x00, 0x00, 0x00, 0x00,
        static_cast<uint8_t>((length >> 8) & 0xFF),
        static_cast<uint8_t>(length & 0xFF),
        0x00
    };
    return executeCommand(cdb, sizeof(cdb), const_cast<uint8_t*>(buffer), length, LinuxDataDirection::DataOut);
}

LinuxScsiResult LinuxScsi::logSense(uint8_t pageCode, uint8_t* buffer, size_t length)
{
    uint8_t cdb[10] = {
        0x4D,                              // LOG SENSE
        0x00,
        static_cast<uint8_t>(0x40 | (pageCode & 0x3F)), // PC=01 (cumulative values)
        0x00, 0x00, 0x00, 0x00,
        static_cast<uint8_t>((length >> 8) & 0xFF),
        static_cast<uint8_t>(length & 0xFF),
        0x00
    };
    return executeCommand(cdb, sizeof(cdb), buffer, length, LinuxDataDirection::DataIn);
}

LinuxScsiResult LinuxScsi::readAttribute(uint16_t attributeId, uint8_t* buffer, size_t length, uint8_t partition)
{
    uint8_t cdb[16] = {
        0x8C,                              // READ ATTRIBUTE
        0x00,                              // Service action = Attribute values
        0x00, 0x00, 0x00,
        partition,                         // Partition
        static_cast<uint8_t>((attributeId >> 8) & 0xFF),
        static_cast<uint8_t>(attributeId & 0xFF),
        0x00, 0x00,
        static_cast<uint8_t>((length >> 24) & 0xFF),
        static_cast<uint8_t>((length >> 16) & 0xFF),
        static_cast<uint8_t>((length >> 8) & 0xFF),
        static_cast<uint8_t>(length & 0xFF),
        0x00, 0x00
    };
    return executeCommand(cdb, sizeof(cdb), buffer, length, LinuxDataDirection::DataIn);
}

LinuxScsiResult LinuxScsi::writeAttribute(uint16_t attributeId, const uint8_t* buffer, size_t length, uint8_t partition)
{
    Q_UNUSED(attributeId) // Attribute ID is in the data buffer per SSC spec
    
    uint8_t cdb[16] = {
        0x8D,                              // WRITE ATTRIBUTE
        0x01,                              // WTC bit (write through cache)
        0x00, 0x00, 0x00,
        partition,
        0x00, 0x00, 0x00, 0x00,
        static_cast<uint8_t>((length >> 24) & 0xFF),
        static_cast<uint8_t>((length >> 16) & 0xFF),
        static_cast<uint8_t>((length >> 8) & 0xFF),
        static_cast<uint8_t>(length & 0xFF),
        0x00, 0x00
    };
    return executeCommand(cdb, sizeof(cdb), const_cast<uint8_t*>(buffer), length, LinuxDataDirection::DataOut);
}

LinuxScsiResult LinuxScsi::reportDensitySupport(uint8_t* buffer, size_t length, bool mediaInfo)
{
    uint8_t cdb[10] = {
        0x44,                              // REPORT DENSITY SUPPORT
        static_cast<uint8_t>(mediaInfo ? 0x01 : 0x00),
        0x00, 0x00, 0x00, 0x00, 0x00,
        static_cast<uint8_t>((length >> 8) & 0xFF),
        static_cast<uint8_t>(length & 0xFF),
        0x00
    };
    return executeCommand(cdb, sizeof(cdb), buffer, length, LinuxDataDirection::DataIn);
}

LinuxScsiResult LinuxScsi::setCapacity(uint64_t capacity)
{
    uint8_t cdb[10] = {
        0x0B,                              // SET CAPACITY
        0x00, 0x00,
        static_cast<uint8_t>((capacity >> 16) & 0xFF),
        static_cast<uint8_t>((capacity >> 8) & 0xFF),
        static_cast<uint8_t>(capacity & 0xFF),
        0x00, 0x00, 0x00, 0x00
    };
    return executeCommand(cdb, sizeof(cdb), nullptr, 0, LinuxDataDirection::NoData);
}

LinuxScsiResult LinuxScsi::preventAllowMediumRemoval(bool prevent)
{
    uint8_t cdb[6] = {
        0x1E,                              // PREVENT ALLOW MEDIUM REMOVAL
        0x00,
        0x00,
        0x00,
        static_cast<uint8_t>(prevent ? 0x01 : 0x00),
        0x00
    };
    return executeCommand(cdb, sizeof(cdb), nullptr, 0, LinuxDataDirection::NoData);
}

LinuxScsiResult LinuxScsi::reserveUnit()
{
    uint8_t cdb[6] = { 0x16, 0x00, 0x00, 0x00, 0x00, 0x00 }; // RESERVE UNIT
    return executeCommand(cdb, sizeof(cdb), nullptr, 0, LinuxDataDirection::NoData);
}

LinuxScsiResult LinuxScsi::releaseUnit()
{
    uint8_t cdb[6] = { 0x17, 0x00, 0x00, 0x00, 0x00, 0x00 }; // RELEASE UNIT
    return executeCommand(cdb, sizeof(cdb), nullptr, 0, LinuxDataDirection::NoData);
}

} // namespace qltfs

#endif // Q_OS_LINUX
