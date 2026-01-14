/**
 * QLTOTapeMan - Qt-based LTO Tape Manager
 * Windows Device Enumerator Implementation
 *
 * Copyright (c) 2026 Jeffrey ZHU (zhxsh1225@gmail.com)
 * https://github.com/Gypsop/QLTOTapeMan
 */

#include "WinDeviceEnumerator.h"

#ifdef Q_OS_WIN

#include "WinScsi.h"
#include <QDebug>
#include <QFile>

// Define the tape device GUID if not already defined
DEFINE_GUID(GUID_DEVINTERFACE_TAPE, 
    0x53f5630bL, 0xb6bf, 0x11d0, 0x94, 0xf2, 0x00, 0xa0, 0xc9, 0x1e, 0xfb, 0x8b);

// Define the medium changer device GUID
DEFINE_GUID(GUID_DEVINTERFACE_MEDIUMCHANGER,
    0x53f56310L, 0xb6bf, 0x11d0, 0x94, 0xf2, 0x00, 0xa0, 0xc9, 0x1e, 0xfb, 0x8b);

namespace qltfs {

// =============================================================================
// Constructor / Destructor
// =============================================================================

WinDeviceEnumerator::WinDeviceEnumerator()
    : m_cacheValid(false)
{
}

WinDeviceEnumerator::~WinDeviceEnumerator()
{
}

// =============================================================================
// Device Enumeration
// =============================================================================

QList<WinDeviceInfo> WinDeviceEnumerator::enumerateDevices()
{
    if (!m_cacheValid) {
        m_cachedDevices.clear();
        enumerateTapeDrives();
        enumerateTapeChangers();
        m_cacheValid = true;
    }
    return m_cachedDevices;
}

QList<WinDeviceInfo> WinDeviceEnumerator::enumerateDevices(WinDeviceType type)
{
    QList<WinDeviceInfo> allDevices = enumerateDevices();
    QList<WinDeviceInfo> filtered;
    
    for (const auto& device : allDevices) {
        if (device.type == type) {
            filtered.append(device);
        }
    }
    
    return filtered;
}

void WinDeviceEnumerator::refresh()
{
    m_cacheValid = false;
    enumerateDevices();
}

std::optional<WinDeviceInfo> WinDeviceEnumerator::getDeviceByPath(const QString& devicePath)
{
    for (const auto& device : enumerateDevices()) {
        if (device.devicePath == devicePath) {
            return device;
        }
    }
    return std::nullopt;
}

std::optional<WinDeviceInfo> WinDeviceEnumerator::getDeviceBySerial(const QString& serialNumber)
{
    for (const auto& device : enumerateDevices()) {
        if (device.serialNumber == serialNumber) {
            return device;
        }
    }
    return std::nullopt;
}

bool WinDeviceEnumerator::isDeviceReady(const QString& devicePath)
{
    WinScsi scsi(devicePath);
    if (!scsi.open()) {
        return false;
    }
    
    ScsiResult result = scsi.testUnitReady();
    scsi.close();
    
    return result.success;
}

// =============================================================================
// Internal Enumeration Methods
// =============================================================================

void WinDeviceEnumerator::enumerateTapeDrives()
{
    // Try \\.\Tape0 through \\.\Tape15
    for (int i = 0; i < 16; ++i) {
        QString devicePath = QStringLiteral("\\\\.\\Tape%1").arg(i);
        
        // Try to open the device
        HANDLE hDevice = CreateFileW(
            reinterpret_cast<LPCWSTR>(devicePath.utf16()),
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            nullptr
        );
        
        if (hDevice == INVALID_HANDLE_VALUE) {
            continue; // Device doesn't exist
        }
        
        CloseHandle(hDevice);
        
        // Get device information via SCSI inquiry
        WinDeviceInfo info = getWinDeviceInfoByInquiry(devicePath);
        if (!info.vendor.isEmpty() || !info.model.isEmpty()) {
            info.devicePath = devicePath;
            info.type = WinDeviceType::TapeDrive;
            info.deviceIndex = i;
            
            // Get additional status
            getTapeDriveStatus(devicePath, info);
            
            m_cachedDevices.append(info);
        }
    }
}

void WinDeviceEnumerator::enumerateTapeChangers()
{
    // Try \\.\Changer0 through \\.\Changer15
    for (int i = 0; i < 16; ++i) {
        QString devicePath = QStringLiteral("\\\\.\\Changer%1").arg(i);
        
        // Try to open the device
        HANDLE hDevice = CreateFileW(
            reinterpret_cast<LPCWSTR>(devicePath.utf16()),
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            nullptr
        );
        
        if (hDevice == INVALID_HANDLE_VALUE) {
            continue; // Device doesn't exist
        }
        
        CloseHandle(hDevice);
        
        // Get device information via SCSI inquiry
        WinDeviceInfo info = getWinDeviceInfoByInquiry(devicePath);
        if (!info.vendor.isEmpty() || !info.model.isEmpty()) {
            info.devicePath = devicePath;
            info.type = WinDeviceType::TapeChanger;
            info.deviceIndex = i;
            
            m_cachedDevices.append(info);
        }
    }
}

WinDeviceInfo WinDeviceEnumerator::getWinDeviceInfoByInquiry(const QString& devicePath)
{
    WinDeviceInfo info;
    
    WinScsi scsi(devicePath);
    if (!scsi.open()) {
        return info;
    }
    
    // Send INQUIRY command
    uint8_t inquiryData[96] = {};
    ScsiResult result = scsi.inquiry(inquiryData, sizeof(inquiryData));
    
    if (result.success) {
        parseInquiryData(inquiryData, sizeof(inquiryData), info);
    }
    
    scsi.close();
    return info;
}

void WinDeviceEnumerator::parseInquiryData(const uint8_t* inquiryData, size_t length, WinDeviceInfo& info)
{
    if (!inquiryData || length < 36) {
        return;
    }
    
    // Byte 0: Peripheral device type
    uint8_t deviceType = inquiryData[0] & 0x1F;
    switch (deviceType) {
        case 0x01: // Sequential-access device (tape)
            info.type = WinDeviceType::TapeDrive;
            break;
        case 0x08: // Medium changer
            info.type = WinDeviceType::TapeChanger;
            break;
        default:
            info.type = WinDeviceType::Unknown;
            break;
    }
    
    // Bytes 8-15: Vendor identification (8 chars, ASCII)
    info.vendor = QString::fromLatin1(reinterpret_cast<const char*>(&inquiryData[8]), 8).trimmed();
    
    // Bytes 16-31: Product identification (16 chars, ASCII)
    info.model = QString::fromLatin1(reinterpret_cast<const char*>(&inquiryData[16]), 16).trimmed();
    
    // Bytes 32-35: Product revision (4 chars, ASCII)
    info.revision = QString::fromLatin1(reinterpret_cast<const char*>(&inquiryData[32]), 4).trimmed();
    
    // Bytes 36-43: Vendor-specific (some drives put serial number here)
    if (length >= 44) {
        QString vendorSpecific = QString::fromLatin1(reinterpret_cast<const char*>(&inquiryData[36]), 8).trimmed();
        if (!vendorSpecific.isEmpty() && info.serialNumber.isEmpty()) {
            info.serialNumber = vendorSpecific;
        }
    }
    
    // Try to get serial number from VPD page 80h
    // This would require an additional INQUIRY with EVPD=1, Page=0x80
    // For simplicity, we get what we can from the standard inquiry
}

void WinDeviceEnumerator::getTapeDriveStatus(const QString& devicePath, WinDeviceInfo& info)
{
    WinScsi scsi(devicePath);
    if (!scsi.open()) {
        info.status = WinDeviceStatus::Error;
        return;
    }
    
    // Test Unit Ready
    ScsiResult turResult = scsi.testUnitReady();
    
    if (turResult.success) {
        info.status = WinDeviceStatus::Ready;
        info.hasMedium = true;
    } else {
        // Check sense data for more details
        if (turResult.senseKey == 0x02) { // NOT READY
            if (turResult.asc == 0x3A) { // MEDIUM NOT PRESENT
                info.status = WinDeviceStatus::Empty;
                info.hasMedium = false;
            } else if (turResult.asc == 0x04) { // NOT READY, BECOMING READY
                info.status = WinDeviceStatus::Busy;
            } else {
                info.status = WinDeviceStatus::NotReady;
            }
        } else if (turResult.senseKey == 0x06) { // UNIT ATTENTION
            info.status = WinDeviceStatus::Ready;
            info.hasMedium = true;
        } else {
            info.status = WinDeviceStatus::Error;
        }
    }
    
    // Get capacity information if medium is present
    if (info.hasMedium) {
        // Read Mode Sense page 0x31 (Tape Capacity)
        uint8_t modeData[64] = {};
        ScsiResult msResult = scsi.modeSense10(0x31, modeData, sizeof(modeData));
        
        if (msResult.success && msResult.dataTransferred >= 20) {
            // Parse capacity from mode page (manufacturer-specific format)
            // This is a simplified example - actual parsing depends on the drive
        }
    }
    
    scsi.close();
}

QString WinDeviceEnumerator::getDeviceDescription(const QString& devicePath)
{
    Q_UNUSED(devicePath)
    // This would use SetupAPI to get the device description
    // For now, return empty - the vendor/model from inquiry is more reliable
    return QString();
}

QString WinDeviceEnumerator::getSerialNumber(const QString& devicePath)
{
    WinScsi scsi(devicePath);
    if (!scsi.open()) {
        return QString();
    }
    
    // Send INQUIRY with EVPD=1, Page Code=0x80 (Unit Serial Number)
    uint8_t cdb[6] = {
        0x12,       // INQUIRY
        0x01,       // EVPD=1
        0x80,       // Page Code: Unit Serial Number
        0x00,
        0xFF,       // Allocation length
        0x00
    };
    
    uint8_t buffer[256] = {};
    ScsiResult result = scsi.executeCommand(
        cdb, sizeof(cdb),
        buffer, sizeof(buffer),
        ScsiCommand::DataDirection::DataIn
    );
    
    QString serialNumber;
    if (result.success && result.dataTransferred >= 4) {
        uint8_t pageLength = buffer[3];
        if (pageLength > 0 && 4 + pageLength <= result.dataTransferred) {
            serialNumber = QString::fromLatin1(
                reinterpret_cast<const char*>(&buffer[4]), 
                pageLength
            ).trimmed();
        }
    }
    
    scsi.close();
    return serialNumber;
}

} // namespace qltfs

#endif // Q_OS_WIN
