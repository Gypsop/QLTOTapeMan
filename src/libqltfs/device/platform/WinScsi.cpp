/**
 * QLTOTapeMan - Qt-based LTO Tape Manager
 * Windows SCSI Implementation
 *
 * Copyright (c) 2026 Jeffrey ZHU (zhxsh1225@gmail.com)
 * https://github.com/Gypsop/QLTOTapeMan
 */

#include "WinScsi.h"

#ifdef Q_OS_WIN

#include <QDebug>

namespace qltfs {

// =============================================================================
// Constructor / Destructor
// =============================================================================

WinScsi::WinScsi(const QString& devicePath)
    : m_devicePath(devicePath)
    , m_deviceHandle(INVALID_HANDLE_VALUE)
    , m_lastWinError(ERROR_SUCCESS)
{
    memset(m_senseBuffer, 0, sizeof(m_senseBuffer));
}

WinScsi::~WinScsi()
{
    close();
}

WinScsi::WinScsi(WinScsi&& other) noexcept
    : m_devicePath(std::move(other.m_devicePath))
    , m_deviceHandle(other.m_deviceHandle)
    , m_lastWinError(other.m_lastWinError)
{
    memcpy(m_senseBuffer, other.m_senseBuffer, sizeof(m_senseBuffer));
    other.m_deviceHandle = INVALID_HANDLE_VALUE;
}

WinScsi& WinScsi::operator=(WinScsi&& other) noexcept
{
    if (this != &other) {
        close();
        m_devicePath = std::move(other.m_devicePath);
        m_deviceHandle = other.m_deviceHandle;
        m_lastWinError = other.m_lastWinError;
        memcpy(m_senseBuffer, other.m_senseBuffer, sizeof(m_senseBuffer));
        other.m_deviceHandle = INVALID_HANDLE_VALUE;
    }
    return *this;
}

// =============================================================================
// Device Open / Close
// =============================================================================

bool WinScsi::open()
{
    if (isOpen()) {
        return true;
    }

    // Convert device path to Windows format
    QString winPath = m_devicePath;
    if (!winPath.startsWith(QStringLiteral("\\\\.\\"))) {
        winPath = QStringLiteral("\\\\.\\") + winPath;
    }

    m_deviceHandle = CreateFileW(
        reinterpret_cast<LPCWSTR>(winPath.utf16()),
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );

    if (m_deviceHandle == INVALID_HANDLE_VALUE) {
        m_lastWinError = GetLastError();
        qWarning() << "Failed to open device" << m_devicePath 
                   << "Error:" << formatWinError(m_lastWinError);
        return false;
    }

    m_lastWinError = ERROR_SUCCESS;
    return true;
}

void WinScsi::close()
{
    if (m_deviceHandle != INVALID_HANDLE_VALUE) {
        CloseHandle(m_deviceHandle);
        m_deviceHandle = INVALID_HANDLE_VALUE;
    }
}

bool WinScsi::isOpen() const
{
    return m_deviceHandle != INVALID_HANDLE_VALUE;
}

// =============================================================================
// Command Execution
// =============================================================================

WinScsiResult WinScsi::executeCommand(
    const uint8_t* cdb,
    size_t cdbLength,
    uint8_t* data,
    size_t dataLength,
    WinDataDirection direction,
    int timeoutSecs,
    uint8_t* senseData,
    size_t senseLength)
{
    WinScsiResult result;
    result.success = false;

    if (!isOpen()) {
        result.errorMessage = QStringLiteral("Device not open");
        return result;
    }

    if (cdbLength > 16) {
        result.errorMessage = QStringLiteral("CDB too long (max 16 bytes)");
        return result;
    }

    // Prepare SCSI_PASS_THROUGH_DIRECT structure with sense buffer
    struct {
        SCSI_PASS_THROUGH_DIRECT sptd;
        uint8_t sense[64];
    } sptdWithSense = {};

    if (!buildSptd(sptdWithSense.sptd, cdb, cdbLength, data, dataLength, direction, timeoutSecs)) {
        result.errorMessage = QStringLiteral("Failed to build SPTD structure");
        return result;
    }

    // Setup sense buffer
    sptdWithSense.sptd.SenseInfoLength = sizeof(sptdWithSense.sense);
    sptdWithSense.sptd.SenseInfoOffset = offsetof(decltype(sptdWithSense), sense);

    DWORD bytesReturned = 0;
    BOOL ioResult = DeviceIoControl(
        m_deviceHandle,
        IOCTL_SCSI_PASS_THROUGH_DIRECT,
        &sptdWithSense,
        sizeof(sptdWithSense),
        &sptdWithSense,
        sizeof(sptdWithSense),
        &bytesReturned,
        nullptr
    );

    if (!ioResult) {
        m_lastWinError = GetLastError();
        result.errorMessage = QStringLiteral("DeviceIoControl failed: ") + formatWinError(m_lastWinError);
        return result;
    }

    // Check SCSI status
    result.scsiStatus = sptdWithSense.sptd.ScsiStatus;
    result.dataTransferred = sptdWithSense.sptd.DataTransferLength;

    if (sptdWithSense.sptd.ScsiStatus == 0) {
        // GOOD status
        result.success = true;
    } else if (sptdWithSense.sptd.ScsiStatus == 2) {
        // CHECK CONDITION - parse sense data
        parseSenseData(sptdWithSense.sense, sizeof(sptdWithSense.sense), result);
        
        // Copy sense data to caller if requested
        if (senseData && senseLength > 0) {
            size_t copyLen = qMin(senseLength, sizeof(sptdWithSense.sense));
            memcpy(senseData, sptdWithSense.sense, copyLen);
        }
    }

    // Store sense data internally
    memcpy(m_senseBuffer, sptdWithSense.sense, sizeof(m_senseBuffer));

    return result;
}

bool WinScsi::buildSptd(
    SCSI_PASS_THROUGH_DIRECT& sptd,
    const uint8_t* cdb,
    size_t cdbLength,
    uint8_t* data,
    size_t dataLength,
    WinDataDirection direction,
    int timeoutSecs)
{
    memset(&sptd, 0, sizeof(sptd));
    
    sptd.Length = sizeof(SCSI_PASS_THROUGH_DIRECT);
    sptd.PathId = 0;
    sptd.TargetId = 0;
    sptd.Lun = 0;
    sptd.CdbLength = static_cast<UCHAR>(cdbLength);
    sptd.TimeOutValue = timeoutSecs;

    // Set data direction
    switch (direction) {
    case WinDataDirection::NoData:
        sptd.DataIn = SCSI_IOCTL_DATA_UNSPECIFIED;
        sptd.DataBuffer = nullptr;
        sptd.DataTransferLength = 0;
        break;
    case WinDataDirection::DataIn:
        sptd.DataIn = SCSI_IOCTL_DATA_IN;
        sptd.DataBuffer = data;
        sptd.DataTransferLength = static_cast<ULONG>(dataLength);
        break;
    case WinDataDirection::DataOut:
        sptd.DataIn = SCSI_IOCTL_DATA_OUT;
        sptd.DataBuffer = const_cast<uint8_t*>(data);
        sptd.DataTransferLength = static_cast<ULONG>(dataLength);
        break;
    }

    // Copy CDB
    memcpy(sptd.Cdb, cdb, cdbLength);

    return true;
}

void WinScsi::parseSenseData(const uint8_t* senseData, size_t length, WinScsiResult& result)
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

QString WinScsi::formatWinError(DWORD errorCode)
{
    wchar_t* msgBuffer = nullptr;
    
    DWORD size = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        errorCode,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPWSTR>(&msgBuffer),
        0,
        nullptr
    );

    QString result;
    if (size > 0 && msgBuffer) {
        result = QString::fromWCharArray(msgBuffer, size).trimmed();
        LocalFree(msgBuffer);
    } else {
        result = QStringLiteral("Unknown error (%1)").arg(errorCode);
    }
    
    return result;
}

// =============================================================================
// SCSI Tape Commands
// =============================================================================

WinScsiResult WinScsi::testUnitReady()
{
    uint8_t cdb[6] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }; // TEST UNIT READY
    return executeCommand(cdb, sizeof(cdb), nullptr, 0, WinDataDirection::NoData);
}

WinScsiResult WinScsi::inquiry(uint8_t* inquiryData, size_t length)
{
    uint8_t cdb[6] = {
        0x12,                              // INQUIRY
        0x00,
        0x00,
        static_cast<uint8_t>((length >> 8) & 0xFF),
        static_cast<uint8_t>(length & 0xFF),
        0x00
    };
    return executeCommand(cdb, sizeof(cdb), inquiryData, length, WinDataDirection::DataIn);
}

WinScsiResult WinScsi::readBlockLimits(uint32_t& minBlockSize, uint32_t& maxBlockSize)
{
    uint8_t buffer[6] = {};
    uint8_t cdb[6] = { 0x05, 0x00, 0x00, 0x00, 0x00, 0x00 }; // READ BLOCK LIMITS
    
    WinScsiResult result = executeCommand(cdb, sizeof(cdb), buffer, sizeof(buffer), WinDataDirection::DataIn);
    
    if (result.success) {
        maxBlockSize = (static_cast<uint32_t>(buffer[1]) << 16) |
                       (static_cast<uint32_t>(buffer[2]) << 8) |
                       static_cast<uint32_t>(buffer[3]);
        minBlockSize = (static_cast<uint32_t>(buffer[4]) << 8) |
                       static_cast<uint32_t>(buffer[5]);
    }
    
    return result;
}

WinScsiResult WinScsi::requestSense(uint8_t* senseBuffer, size_t length)
{
    uint8_t cdb[6] = {
        0x03,                              // REQUEST SENSE
        0x00,
        0x00,
        0x00,
        static_cast<uint8_t>(length & 0xFF),
        0x00
    };
    return executeCommand(cdb, sizeof(cdb), senseBuffer, length, WinDataDirection::DataIn);
}

WinScsiResult WinScsi::read6(uint8_t* buffer, size_t length, bool fixed, size_t transferLength)
{
    uint8_t cdb[6] = {
        0x08,                              // READ(6)
        static_cast<uint8_t>(fixed ? 0x01 : 0x00),
        static_cast<uint8_t>((transferLength >> 16) & 0xFF),
        static_cast<uint8_t>((transferLength >> 8) & 0xFF),
        static_cast<uint8_t>(transferLength & 0xFF),
        0x00
    };
    return executeCommand(cdb, sizeof(cdb), buffer, length, WinDataDirection::DataIn, 300);
}

WinScsiResult WinScsi::write6(const uint8_t* buffer, size_t length, bool fixed, size_t transferLength)
{
    uint8_t cdb[6] = {
        0x0A,                              // WRITE(6)
        static_cast<uint8_t>(fixed ? 0x01 : 0x00),
        static_cast<uint8_t>((transferLength >> 16) & 0xFF),
        static_cast<uint8_t>((transferLength >> 8) & 0xFF),
        static_cast<uint8_t>(transferLength & 0xFF),
        0x00
    };
    return executeCommand(cdb, sizeof(cdb), const_cast<uint8_t*>(buffer), length, WinDataDirection::DataOut, 300);
}

WinScsiResult WinScsi::readPosition(WinTapePosition& position)
{
    uint8_t buffer[20] = {};
    uint8_t cdb[10] = {
        0x34,                              // READ POSITION
        0x00,                              // Short form
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };
    
    WinScsiResult result = executeCommand(cdb, sizeof(cdb), buffer, sizeof(buffer), WinDataDirection::DataIn);
    
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

WinScsiResult WinScsi::locate10(uint32_t blockAddress, uint8_t partition)
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
    return executeCommand(cdb, sizeof(cdb), nullptr, 0, WinDataDirection::NoData, 600);
}

WinScsiResult WinScsi::locate16(uint64_t blockAddress, uint8_t partition)
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
    return executeCommand(cdb, sizeof(cdb), nullptr, 0, WinDataDirection::NoData, 600);
}

WinScsiResult WinScsi::rewind()
{
    uint8_t cdb[6] = { 0x01, 0x00, 0x00, 0x00, 0x00, 0x00 }; // REWIND
    return executeCommand(cdb, sizeof(cdb), nullptr, 0, WinDataDirection::NoData, 1800);
}

WinScsiResult WinScsi::space(WinSpaceCode code, int32_t count)
{
    uint8_t cdb[6] = {
        0x11,                              // SPACE
        static_cast<uint8_t>(code),
        static_cast<uint8_t>((count >> 16) & 0xFF),
        static_cast<uint8_t>((count >> 8) & 0xFF),
        static_cast<uint8_t>(count & 0xFF),
        0x00
    };
    return executeCommand(cdb, sizeof(cdb), nullptr, 0, WinDataDirection::NoData, 1800);
}

WinScsiResult WinScsi::writeFilemarks(uint32_t count, bool setmark)
{
    uint8_t cdb[6] = {
        0x10,                              // WRITE FILEMARKS
        static_cast<uint8_t>(setmark ? 0x02 : 0x00),
        static_cast<uint8_t>((count >> 16) & 0xFF),
        static_cast<uint8_t>((count >> 8) & 0xFF),
        static_cast<uint8_t>(count & 0xFF),
        0x00
    };
    return executeCommand(cdb, sizeof(cdb), nullptr, 0, WinDataDirection::NoData, 300);
}

WinScsiResult WinScsi::loadUnload(bool load)
{
    uint8_t cdb[6] = {
        0x1B,                              // LOAD/UNLOAD
        0x00,
        0x00,
        0x00,
        static_cast<uint8_t>(load ? 0x01 : 0x00),
        0x00
    };
    return executeCommand(cdb, sizeof(cdb), nullptr, 0, WinDataDirection::NoData, 600);
}

WinScsiResult WinScsi::erase(bool longErase)
{
    uint8_t cdb[6] = {
        0x19,                              // ERASE
        static_cast<uint8_t>(longErase ? 0x01 : 0x00),
        0x00, 0x00, 0x00, 0x00
    };
    // Long erase can take hours
    int timeout = longErase ? 86400 : 3600; // 24h or 1h
    return executeCommand(cdb, sizeof(cdb), nullptr, 0, WinDataDirection::NoData, timeout);
}

WinScsiResult WinScsi::modeSense6(uint8_t pageCode, uint8_t* buffer, size_t length)
{
    uint8_t cdb[6] = {
        0x1A,                              // MODE SENSE(6)
        0x00,
        static_cast<uint8_t>(pageCode & 0x3F),
        0x00,
        static_cast<uint8_t>(length & 0xFF),
        0x00
    };
    return executeCommand(cdb, sizeof(cdb), buffer, length, WinDataDirection::DataIn);
}

WinScsiResult WinScsi::modeSense10(uint8_t pageCode, uint8_t* buffer, size_t length)
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
    return executeCommand(cdb, sizeof(cdb), buffer, length, WinDataDirection::DataIn);
}

WinScsiResult WinScsi::modeSelect6(const uint8_t* buffer, size_t length, bool savePage)
{
    uint8_t cdb[6] = {
        0x15,                              // MODE SELECT(6)
        static_cast<uint8_t>(0x10 | (savePage ? 0x01 : 0x00)), // PF bit + SP bit
        0x00,
        0x00,
        static_cast<uint8_t>(length & 0xFF),
        0x00
    };
    return executeCommand(cdb, sizeof(cdb), const_cast<uint8_t*>(buffer), length, WinDataDirection::DataOut);
}

WinScsiResult WinScsi::modeSelect10(const uint8_t* buffer, size_t length, bool savePage)
{
    uint8_t cdb[10] = {
        0x55,                              // MODE SELECT(10)
        static_cast<uint8_t>(0x10 | (savePage ? 0x01 : 0x00)), // PF bit + SP bit
        0x00, 0x00, 0x00, 0x00, 0x00,
        static_cast<uint8_t>((length >> 8) & 0xFF),
        static_cast<uint8_t>(length & 0xFF),
        0x00
    };
    return executeCommand(cdb, sizeof(cdb), const_cast<uint8_t*>(buffer), length, WinDataDirection::DataOut);
}

WinScsiResult WinScsi::logSense(uint8_t pageCode, uint8_t* buffer, size_t length)
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
    return executeCommand(cdb, sizeof(cdb), buffer, length, WinDataDirection::DataIn);
}

WinScsiResult WinScsi::readAttribute(uint16_t attributeId, uint8_t* buffer, size_t length, uint8_t partition)
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
    return executeCommand(cdb, sizeof(cdb), buffer, length, WinDataDirection::DataIn);
}

WinScsiResult WinScsi::writeAttribute(uint16_t attributeId, const uint8_t* buffer, size_t length, uint8_t partition)
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
    return executeCommand(cdb, sizeof(cdb), const_cast<uint8_t*>(buffer), length, WinDataDirection::DataOut);
}

WinScsiResult WinScsi::reportDensitySupport(uint8_t* buffer, size_t length, bool mediaInfo)
{
    uint8_t cdb[10] = {
        0x44,                              // REPORT DENSITY SUPPORT
        static_cast<uint8_t>(mediaInfo ? 0x01 : 0x00),
        0x00, 0x00, 0x00, 0x00, 0x00,
        static_cast<uint8_t>((length >> 8) & 0xFF),
        static_cast<uint8_t>(length & 0xFF),
        0x00
    };
    return executeCommand(cdb, sizeof(cdb), buffer, length, WinDataDirection::DataIn);
}

WinScsiResult WinScsi::setCapacity(uint64_t capacity)
{
    uint8_t cdb[10] = {
        0x0B,                              // SET CAPACITY
        0x00, 0x00,
        static_cast<uint8_t>((capacity >> 16) & 0xFF),
        static_cast<uint8_t>((capacity >> 8) & 0xFF),
        static_cast<uint8_t>(capacity & 0xFF),
        0x00, 0x00, 0x00, 0x00
    };
    return executeCommand(cdb, sizeof(cdb), nullptr, 0, WinDataDirection::NoData);
}

WinScsiResult WinScsi::preventAllowMediumRemoval(bool prevent)
{
    uint8_t cdb[6] = {
        0x1E,                              // PREVENT ALLOW MEDIUM REMOVAL
        0x00,
        0x00,
        0x00,
        static_cast<uint8_t>(prevent ? 0x01 : 0x00),
        0x00
    };
    return executeCommand(cdb, sizeof(cdb), nullptr, 0, WinDataDirection::NoData);
}

WinScsiResult WinScsi::reserveUnit()
{
    uint8_t cdb[6] = { 0x16, 0x00, 0x00, 0x00, 0x00, 0x00 }; // RESERVE UNIT
    return executeCommand(cdb, sizeof(cdb), nullptr, 0, WinDataDirection::NoData);
}

WinScsiResult WinScsi::releaseUnit()
{
    uint8_t cdb[6] = { 0x17, 0x00, 0x00, 0x00, 0x00, 0x00 }; // RELEASE UNIT
    return executeCommand(cdb, sizeof(cdb), nullptr, 0, WinDataDirection::NoData);
}

} // namespace qltfs

#endif // Q_OS_WIN
