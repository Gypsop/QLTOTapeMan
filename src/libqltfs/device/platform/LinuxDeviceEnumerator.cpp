/**
 * QLTOTapeMan - Qt-based LTO Tape Manager
 * Linux Device Enumerator Implementation
 *
 * Copyright (c) 2026 Jeffrey ZHU (zhxsh1225@gmail.com)
 * https://github.com/Gypsop/QLTOTapeMan
 */

#include "LinuxDeviceEnumerator.h"

#if defined(Q_OS_LINUX)

#include "LinuxScsi.h"
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>

namespace qltfs {

// =============================================================================
// Constructor / Destructor
// =============================================================================

LinuxDeviceEnumerator::LinuxDeviceEnumerator()
    : m_cacheValid(false)
{
}

LinuxDeviceEnumerator::~LinuxDeviceEnumerator()
{
}

// =============================================================================
// Device Enumeration
// =============================================================================

QList<LinuxDeviceInfo> LinuxDeviceEnumerator::enumerateDevices()
{
    if (!m_cacheValid) {
        m_cachedDevices.clear();
        enumerateTapeDrives();
        enumerateTapeChangers();
        m_cacheValid = true;
    }
    return m_cachedDevices;
}

QList<LinuxDeviceInfo> LinuxDeviceEnumerator::enumerateDevices(LinuxDeviceType type)
{
    QList<LinuxDeviceInfo> allDevices = enumerateDevices();
    QList<LinuxDeviceInfo> filtered;
    
    for (const auto& device : allDevices) {
        if (device.type == type) {
            filtered.append(device);
        }
    }
    
    return filtered;
}

void LinuxDeviceEnumerator::refresh()
{
    m_cacheValid = false;
    enumerateDevices();
}

std::optional<LinuxDeviceInfo> LinuxDeviceEnumerator::getDeviceByPath(const QString& devicePath)
{
    for (const auto& device : enumerateDevices()) {
        if (device.devicePath == devicePath) {
            return device;
        }
    }
    return std::nullopt;
}

std::optional<LinuxDeviceInfo> LinuxDeviceEnumerator::getDeviceBySerial(const QString& serialNumber)
{
    for (const auto& device : enumerateDevices()) {
        if (device.serialNumber == serialNumber) {
            return device;
        }
    }
    return std::nullopt;
}

bool LinuxDeviceEnumerator::isDeviceReady(const QString& devicePath)
{
    LinuxScsi scsi(devicePath);
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

void LinuxDeviceEnumerator::enumerateTapeDrives()
{
    // First try sysfs enumeration (more reliable)
    enumerateViaSysfs();
    
    // If sysfs didn't find anything, try direct device probing
    if (m_cachedDevices.isEmpty()) {
        // Try /dev/nst0 through /dev/nst15 (non-rewinding)
        for (int i = 0; i < 16; ++i) {
            QString devicePath = QStringLiteral("/dev/nst%1").arg(i);
            
            if (!QFile::exists(devicePath)) {
                continue;
            }
            
            LinuxDeviceInfo info = getLinuxDeviceInfoByInquiry(devicePath);
            if (!info.vendor.isEmpty() || !info.model.isEmpty()) {
                info.devicePath = devicePath;
                info.type = LinuxDeviceType::TapeDrive;
                info.deviceIndex = i;
                
                // Get additional info
                getTapeDriveStatus(devicePath, info);
                
                // Try to get serial number
                if (info.serialNumber.isEmpty()) {
                    info.serialNumber = getSerialNumber(devicePath);
                }
                
                m_cachedDevices.append(info);
            }
        }
    }
}

void LinuxDeviceEnumerator::enumerateTapeChangers()
{
    enumerateChangersViaSysfs();
    
    // If sysfs didn't find anything, try direct probing
    // Check for medium changers - typically /dev/sch0, /dev/sg* with changer type
    for (int i = 0; i < 16; ++i) {
        QString devicePath = QStringLiteral("/dev/sch%1").arg(i);
        
        if (!QFile::exists(devicePath)) {
            continue;
        }
        
        LinuxDeviceInfo info = getLinuxDeviceInfoByInquiry(devicePath);
        if (!info.vendor.isEmpty() || !info.model.isEmpty()) {
            info.devicePath = devicePath;
            info.type = LinuxDeviceType::TapeChanger;
            info.deviceIndex = i;
            
            m_cachedDevices.append(info);
        }
    }
}

void LinuxDeviceEnumerator::enumerateViaSysfs()
{
    QDir sysfsDir(QStringLiteral("/sys/class/scsi_tape"));
    
    if (!sysfsDir.exists()) {
        return;
    }
    
    // Get all entries (nst0, nst1, st0, etc.)
    QStringList entries = sysfsDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    
    // Filter to only get non-rewinding devices (nst*)
    QRegularExpression nstRegex(QStringLiteral("^nst(\\d+)$"));
    
    for (const QString& entry : entries) {
        QRegularExpressionMatch match = nstRegex.match(entry);
        if (!match.hasMatch()) {
            continue;
        }
        
        int deviceIndex = match.captured(1).toInt();
        QString devicePath = QStringLiteral("/dev/%1").arg(entry);
        QString sysfsPath = sysfsDir.filePath(entry);
        
        LinuxDeviceInfo info;
        info.devicePath = devicePath;
        info.deviceIndex = deviceIndex;
        info.type = LinuxDeviceType::TapeDrive;
        
        // Read device link to get SCSI host/channel/id/lun
        QString deviceLink = QFile::symLinkTarget(sysfsPath + QStringLiteral("/device"));
        
        // Get vendor/model from sysfs if possible
        QString scsiDevicePath = sysfsPath + QStringLiteral("/device");
        if (QDir(scsiDevicePath).exists()) {
            info.vendor = readSysfsAttribute(scsiDevicePath + QStringLiteral("/vendor"));
            info.model = readSysfsAttribute(scsiDevicePath + QStringLiteral("/model"));
            info.revision = readSysfsAttribute(scsiDevicePath + QStringLiteral("/rev"));
        }
        
        // If sysfs doesn't have the info, use SCSI inquiry
        if (info.vendor.isEmpty() && info.model.isEmpty()) {
            LinuxDeviceInfo inquiryInfo = getLinuxDeviceInfoByInquiry(devicePath);
            if (!inquiryInfo.vendor.isEmpty()) {
                info.vendor = inquiryInfo.vendor;
                info.model = inquiryInfo.model;
                info.revision = inquiryInfo.revision;
            }
        }
        
        // Get serial number
        info.serialNumber = getSerialNumber(devicePath);
        
        // Get status
        getTapeDriveStatus(devicePath, info);
        
        m_cachedDevices.append(info);
    }
}

void LinuxDeviceEnumerator::enumerateChangersViaSysfs()
{
    QDir sysfsDir(QStringLiteral("/sys/class/scsi_changer"));
    
    if (!sysfsDir.exists()) {
        return;
    }
    
    QStringList entries = sysfsDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    
    QRegularExpression schRegex(QStringLiteral("^sch(\\d+)$"));
    
    for (const QString& entry : entries) {
        QRegularExpressionMatch match = schRegex.match(entry);
        if (!match.hasMatch()) {
            continue;
        }
        
        int deviceIndex = match.captured(1).toInt();
        QString devicePath = QStringLiteral("/dev/%1").arg(entry);
        QString sysfsPath = sysfsDir.filePath(entry);
        
        LinuxDeviceInfo info;
        info.devicePath = devicePath;
        info.deviceIndex = deviceIndex;
        info.type = LinuxDeviceType::TapeChanger;
        
        // Get vendor/model from sysfs
        QString scsiDevicePath = sysfsPath + QStringLiteral("/device");
        if (QDir(scsiDevicePath).exists()) {
            info.vendor = readSysfsAttribute(scsiDevicePath + QStringLiteral("/vendor"));
            info.model = readSysfsAttribute(scsiDevicePath + QStringLiteral("/model"));
            info.revision = readSysfsAttribute(scsiDevicePath + QStringLiteral("/rev"));
        }
        
        m_cachedDevices.append(info);
    }
}

LinuxDeviceInfo LinuxDeviceEnumerator::getLinuxDeviceInfoByInquiry(const QString& devicePath)
{
    LinuxDeviceInfo info;
    
    // For tape devices, we might need to use the sg device
    QString sgPath = getSgDevicePath(devicePath);
    QString pathToUse = sgPath.isEmpty() ? devicePath : sgPath;
    
    LinuxScsi scsi(pathToUse);
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

QString LinuxDeviceEnumerator::getSgDevicePath(const QString& tapeDevicePath)
{
    // Extract device name (e.g., "nst0" from "/dev/nst0")
    QString deviceName = tapeDevicePath.section(QLatin1Char('/'), -1);
    
    // Find the sysfs path
    QString sysfsPath = QStringLiteral("/sys/class/scsi_tape/%1/device").arg(deviceName);
    
    if (!QDir(sysfsPath).exists()) {
        return QString();
    }
    
    // Look for scsi_generic subdirectory
    QDir deviceDir(sysfsPath);
    QStringList sgDirs = deviceDir.entryList(QStringList() << QStringLiteral("scsi_generic"), QDir::Dirs);
    
    if (sgDirs.isEmpty()) {
        return QString();
    }
    
    // Get the sg device name
    QDir sgDir(sysfsPath + QStringLiteral("/scsi_generic"));
    QStringList sgDevices = sgDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    
    if (sgDevices.isEmpty()) {
        return QString();
    }
    
    return QStringLiteral("/dev/%1").arg(sgDevices.first());
}

void LinuxDeviceEnumerator::parseInquiryData(const uint8_t* inquiryData, size_t length, LinuxDeviceInfo& info)
{
    if (!inquiryData || length < 36) {
        return;
    }
    
    // Byte 0: Peripheral device type
    uint8_t deviceType = inquiryData[0] & 0x1F;
    switch (deviceType) {
        case 0x01: // Sequential-access device (tape)
            info.type = LinuxDeviceType::TapeDrive;
            break;
        case 0x08: // Medium changer
            info.type = LinuxDeviceType::TapeChanger;
            break;
        default:
            info.type = LinuxDeviceType::Unknown;
            break;
    }
    
    // Bytes 8-15: Vendor identification (8 chars, ASCII)
    info.vendor = QString::fromLatin1(reinterpret_cast<const char*>(&inquiryData[8]), 8).trimmed();
    
    // Bytes 16-31: Product identification (16 chars, ASCII)
    info.model = QString::fromLatin1(reinterpret_cast<const char*>(&inquiryData[16]), 16).trimmed();
    
    // Bytes 32-35: Product revision (4 chars, ASCII)
    info.revision = QString::fromLatin1(reinterpret_cast<const char*>(&inquiryData[32]), 4).trimmed();
}

QString LinuxDeviceEnumerator::getSerialNumber(const QString& devicePath)
{
    QString sgPath = getSgDevicePath(devicePath);
    QString pathToUse = sgPath.isEmpty() ? devicePath : sgPath;
    
    LinuxScsi scsi(pathToUse);
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
        if (pageLength > 0 && static_cast<size_t>(4 + pageLength) <= result.dataTransferred) {
            serialNumber = QString::fromLatin1(
                reinterpret_cast<const char*>(&buffer[4]), 
                pageLength
            ).trimmed();
        }
    }
    
    scsi.close();
    return serialNumber;
}

QString LinuxDeviceEnumerator::readSysfsAttribute(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString();
    }
    
    QTextStream stream(&file);
    QString value = stream.readLine().trimmed();
    file.close();
    
    return value;
}

void LinuxDeviceEnumerator::getTapeDriveStatus(const QString& devicePath, LinuxDeviceInfo& info)
{
    QString sgPath = getSgDevicePath(devicePath);
    QString pathToUse = sgPath.isEmpty() ? devicePath : sgPath;
    
    LinuxScsi scsi(pathToUse);
    if (!scsi.open()) {
        info.status = LinuxDeviceStatus::Error;
        return;
    }
    
    // Test Unit Ready
    ScsiResult turResult = scsi.testUnitReady();
    
    if (turResult.success) {
        info.status = LinuxDeviceStatus::Ready;
        info.hasMedium = true;
    } else {
        // Check sense data for more details
        if (turResult.senseKey == 0x02) { // NOT READY
            if (turResult.asc == 0x3A) { // MEDIUM NOT PRESENT
                info.status = LinuxDeviceStatus::Empty;
                info.hasMedium = false;
            } else if (turResult.asc == 0x04) { // NOT READY, BECOMING READY
                info.status = LinuxDeviceStatus::Busy;
            } else {
                info.status = LinuxDeviceStatus::NotReady;
            }
        } else if (turResult.senseKey == 0x06) { // UNIT ATTENTION
            // Unit attention is often seen after media change
            info.status = LinuxDeviceStatus::Ready;
            info.hasMedium = true;
        } else {
            info.status = LinuxDeviceStatus::Error;
        }
    }
    
    scsi.close();
}

QString LinuxDeviceEnumerator::findSysfsPath(const QString& devicePath)
{
    QString deviceName = devicePath.section(QLatin1Char('/'), -1);
    
    // Try scsi_tape class first
    QString tapePath = QStringLiteral("/sys/class/scsi_tape/%1").arg(deviceName);
    if (QDir(tapePath).exists()) {
        return tapePath;
    }
    
    // Try scsi_changer class
    QString changerPath = QStringLiteral("/sys/class/scsi_changer/%1").arg(deviceName);
    if (QDir(changerPath).exists()) {
        return changerPath;
    }
    
    return QString();
}

} // namespace qltfs

#endif // Q_OS_LINUX
