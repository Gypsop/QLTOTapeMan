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

#include "DeviceEnumerator.h"
#include "ScsiCommand.h"

#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QDebug>

#ifdef Q_OS_WIN
#include <windows.h>
#include <setupapi.h>
#include <cfgmgr32.h>
#include <ntddscsi.h>
#include <initguid.h>
#include <ntddstor.h>
// GUID for tape drives
DEFINE_GUID(GUID_DEVINTERFACE_TAPE, 0x53F5630B, 0xB6BF, 0x11D0, 0x94, 0xF2, 0x00, 0xA0, 0xC9, 0x1E, 0xFB, 0x8B);
// GUID for medium changers
DEFINE_GUID(GUID_DEVINTERFACE_MEDIUMCHANGER, 0x53F56310, 0xB6BF, 0x11D0, 0x94, 0xF2, 0x00, 0xA0, 0xC9, 0x1E, 0xFB, 0x8B);
#elif defined(Q_OS_LINUX)
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <scsi/scsi.h>
#include <scsi/sg.h>
#endif

namespace qltfs {

// SCSI device type codes
static constexpr quint8 SCSI_TYPE_DISK = 0x00;
static constexpr quint8 SCSI_TYPE_TAPE = 0x01;
static constexpr quint8 SCSI_TYPE_PRINTER = 0x02;
static constexpr quint8 SCSI_TYPE_PROCESSOR = 0x03;
static constexpr quint8 SCSI_TYPE_WORM = 0x04;
static constexpr quint8 SCSI_TYPE_CDROM = 0x05;
static constexpr quint8 SCSI_TYPE_SCANNER = 0x06;
static constexpr quint8 SCSI_TYPE_OPTICAL = 0x07;
static constexpr quint8 SCSI_TYPE_CHANGER = 0x08;
static constexpr quint8 SCSI_TYPE_COMM = 0x09;
static constexpr quint8 SCSI_TYPE_ENCLOSURE = 0x0D;

// ============================================================================
// TapeDeviceInfo Implementation
// ============================================================================

QString TapeDeviceInfo::displayName() const
{
    if (!vendor.isEmpty() && !product.isEmpty()) {
        return QStringLiteral("%1 %2 (%3)").arg(vendor.trimmed(), product.trimmed(), devicePath);
    }
    return devicePath;
}

bool TapeDeviceInfo::isValid() const
{
    return !devicePath.isEmpty() && type == DeviceType::TapeDrive;
}

// ============================================================================
// ChangerDeviceInfo Implementation
// ============================================================================

QString ChangerDeviceInfo::displayName() const
{
    if (!vendor.isEmpty() && !product.isEmpty()) {
        return QStringLiteral("%1 %2 (%3)").arg(vendor.trimmed(), product.trimmed(), devicePath);
    }
    return devicePath;
}

bool ChangerDeviceInfo::isValid() const
{
    return !devicePath.isEmpty();
}

// ============================================================================
// DeviceEnumerator Private Implementation
// ============================================================================

class DeviceEnumerator::Private
{
public:
    QList<TapeDeviceInfo> tapeDevices;
    QList<ChangerDeviceInfo> changerDevices;
    QString lastError;

    bool enumerateTapeDevices();
    bool enumerateChangerDevices();
    TapeDeviceInfo queryTapeDeviceInfo(const QString &devicePath, const QString &genericPath = QString());
    ChangerDeviceInfo queryChangerDeviceInfo(const QString &devicePath);

#ifdef Q_OS_WIN
    bool enumerateWindowsDevices(const GUID &interfaceGuid, bool isTape);
    QString getDevicePropertyString(HDEVINFO devInfo, PSP_DEVINFO_DATA devInfoData, DWORD property);
#elif defined(Q_OS_LINUX)
    bool enumerateLinuxTapeDevices();
    bool enumerateLinuxChangerDevices();
    QString readSysfsAttribute(const QString &path);
    QString findGenericDevice(const QString &tapeDevice);
#endif
};

bool DeviceEnumerator::Private::enumerateTapeDevices()
{
    tapeDevices.clear();

#ifdef Q_OS_WIN
    return enumerateWindowsDevices(GUID_DEVINTERFACE_TAPE, true);
#elif defined(Q_OS_LINUX)
    return enumerateLinuxTapeDevices();
#else
    lastError = QStringLiteral("Unsupported platform");
    return false;
#endif
}

bool DeviceEnumerator::Private::enumerateChangerDevices()
{
    changerDevices.clear();

#ifdef Q_OS_WIN
    return enumerateWindowsDevices(GUID_DEVINTERFACE_MEDIUMCHANGER, false);
#elif defined(Q_OS_LINUX)
    return enumerateLinuxChangerDevices();
#else
    lastError = QStringLiteral("Unsupported platform");
    return false;
#endif
}

TapeDeviceInfo DeviceEnumerator::Private::queryTapeDeviceInfo(const QString &devicePath, const QString &genericPath)
{
    TapeDeviceInfo info;
    info.devicePath = devicePath;
    info.genericPath = genericPath.isEmpty() ? devicePath : genericPath;

    // Use SCSI Inquiry to get device info
    ScsiCommand scsi(info.genericPath);
    if (!scsi.open()) {
        return info;
    }

    // Standard Inquiry
    auto result = scsi.inquiry(false, 0, 96);
    if (result.success && result.data.size() >= 36) {
        const quint8 *data = reinterpret_cast<const quint8 *>(result.data.constData());

        // Peripheral device type (byte 0, bits 0-4)
        quint8 deviceType = data[0] & 0x1F;
        info.type = DeviceEnumerator::scsiDeviceType(deviceType);

        // Vendor (bytes 8-15)
        info.vendor = QString::fromLatin1(result.data.mid(8, 8)).trimmed();

        // Product (bytes 16-31)
        info.product = QString::fromLatin1(result.data.mid(16, 16)).trimmed();

        // Revision (bytes 32-35)
        info.revision = QString::fromLatin1(result.data.mid(32, 4)).trimmed();
    }

    // Get serial number via VPD page 0x80
    result = scsi.inquiry(true, 0x80, 256);
    if (result.success && result.data.size() >= 4) {
        int length = static_cast<quint8>(result.data[3]);
        if (result.data.size() >= 4 + length) {
            info.serialNumber = QString::fromLatin1(result.data.mid(4, length)).trimmed();
        }
    }

    // Test if device is ready
    result = scsi.testUnitReady();
    info.isReady = result.success;

    scsi.close();
    return info;
}

ChangerDeviceInfo DeviceEnumerator::Private::queryChangerDeviceInfo(const QString &devicePath)
{
    ChangerDeviceInfo info;
    info.devicePath = devicePath;

    // Use SCSI Inquiry to get device info
    ScsiCommand scsi(devicePath);
    if (!scsi.open()) {
        return info;
    }

    // Standard Inquiry
    auto result = scsi.inquiry(false, 0, 96);
    if (result.success && result.data.size() >= 36) {
        // Vendor (bytes 8-15)
        info.vendor = QString::fromLatin1(result.data.mid(8, 8)).trimmed();

        // Product (bytes 16-31)
        info.product = QString::fromLatin1(result.data.mid(16, 16)).trimmed();

        // Revision (bytes 32-35)
        info.revision = QString::fromLatin1(result.data.mid(32, 4)).trimmed();
    }

    // Get serial number
    result = scsi.inquiry(true, 0x80, 256);
    if (result.success && result.data.size() >= 4) {
        int length = static_cast<quint8>(result.data[3]);
        if (result.data.size() >= 4 + length) {
            info.serialNumber = QString::fromLatin1(result.data.mid(4, length)).trimmed();
        }
    }

    // Get element status via Mode Sense (Element Address Assignment page 0x1D)
    result = scsi.modeSense10(0x1D, 0, 256);
    if (result.success && result.data.size() >= 24) {
        // Parse element counts from mode page
        // Format varies by vendor, this is a simplified parse
        const quint8 *data = reinterpret_cast<const quint8 *>(result.data.constData());

        // Skip mode parameter header (8 bytes for mode sense 10)
        int offset = 8;
        if (static_cast<int>(result.data.size()) > offset + 18) {
            // Medium transport (robot) first element address and count
            info.mediumTransportElements = (data[offset + 2] << 8) | data[offset + 3];

            // Storage elements
            info.storageElements = (data[offset + 6] << 8) | data[offset + 7];

            // Import/Export elements
            info.importExportElements = (data[offset + 10] << 8) | data[offset + 11];

            // Data transfer (drive) elements
            info.dataTransferElements = (data[offset + 14] << 8) | data[offset + 15];
        }
    }

    scsi.close();
    return info;
}

#ifdef Q_OS_WIN

bool DeviceEnumerator::Private::enumerateWindowsDevices(const GUID &interfaceGuid, bool isTape)
{
    HDEVINFO devInfo = SetupDiGetClassDevsW(
        &interfaceGuid,
        nullptr,
        nullptr,
        DIGCF_PRESENT | DIGCF_DEVICEINTERFACE
    );

    if (devInfo == INVALID_HANDLE_VALUE) {
        lastError = QStringLiteral("SetupDiGetClassDevs failed: %1").arg(GetLastError());
        return false;
    }

    SP_DEVICE_INTERFACE_DATA interfaceData;
    interfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

    for (DWORD i = 0; SetupDiEnumDeviceInterfaces(devInfo, nullptr, &interfaceGuid, i, &interfaceData); ++i) {
        // Get required buffer size
        DWORD requiredSize = 0;
        SetupDiGetDeviceInterfaceDetailW(devInfo, &interfaceData, nullptr, 0, &requiredSize, nullptr);

        if (requiredSize == 0) {
            continue;
        }

        // Allocate buffer
        auto *detailData = static_cast<PSP_DEVICE_INTERFACE_DETAIL_DATA_W>(malloc(requiredSize));
        if (!detailData) {
            continue;
        }

        detailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);

        SP_DEVINFO_DATA devInfoData;
        devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

        if (SetupDiGetDeviceInterfaceDetailW(devInfo, &interfaceData, detailData, requiredSize, nullptr, &devInfoData)) {
            QString devicePath = QString::fromWCharArray(detailData->DevicePath);

            if (isTape) {
                // Convert interface path to tape device path
                // Interface path: \\?\scsi#sequential...
                // We need: \\.\Tape0, \\.\Tape1, etc.

                // Try to find the tape number from registry or iterate
                for (int tapeNum = 0; tapeNum < 16; ++tapeNum) {
                    QString tapePath = QStringLiteral("\\\\.\\Tape%1").arg(tapeNum);

                    // Check if this tape device exists and matches
                    HANDLE hTest = CreateFileW(
                        reinterpret_cast<LPCWSTR>(tapePath.utf16()),
                        GENERIC_READ,
                        FILE_SHARE_READ | FILE_SHARE_WRITE,
                        nullptr,
                        OPEN_EXISTING,
                        0,
                        nullptr
                    );

                    if (hTest != INVALID_HANDLE_VALUE) {
                        CloseHandle(hTest);

                        // Query this device
                        TapeDeviceInfo info = queryTapeDeviceInfo(tapePath);
                        if (info.type == DeviceType::TapeDrive) {
                            // Check if we already have this device
                            bool found = false;
                            for (const auto &existing : tapeDevices) {
                                if (existing.devicePath == info.devicePath) {
                                    found = true;
                                    break;
                                }
                            }
                            if (!found) {
                                tapeDevices.append(info);
                            }
                        }
                    }
                }
            } else {
                // Medium changer
                // Try changer device paths
                for (int changerNum = 0; changerNum < 16; ++changerNum) {
                    QString changerPath = QStringLiteral("\\\\.\\Changer%1").arg(changerNum);

                    HANDLE hTest = CreateFileW(
                        reinterpret_cast<LPCWSTR>(changerPath.utf16()),
                        GENERIC_READ,
                        FILE_SHARE_READ | FILE_SHARE_WRITE,
                        nullptr,
                        OPEN_EXISTING,
                        0,
                        nullptr
                    );

                    if (hTest != INVALID_HANDLE_VALUE) {
                        CloseHandle(hTest);

                        ChangerDeviceInfo info = queryChangerDeviceInfo(changerPath);
                        if (info.isValid()) {
                            bool found = false;
                            for (const auto &existing : changerDevices) {
                                if (existing.devicePath == info.devicePath) {
                                    found = true;
                                    break;
                                }
                            }
                            if (!found) {
                                changerDevices.append(info);
                            }
                        }
                    }
                }
            }
        }

        free(detailData);
    }

    SetupDiDestroyDeviceInfoList(devInfo);
    return true;
}

QString DeviceEnumerator::Private::getDevicePropertyString(HDEVINFO devInfo, PSP_DEVINFO_DATA devInfoData, DWORD property)
{
    DWORD requiredSize = 0;
    SetupDiGetDeviceRegistryPropertyW(devInfo, devInfoData, property, nullptr, nullptr, 0, &requiredSize);

    if (requiredSize == 0) {
        return QString();
    }

    QByteArray buffer(static_cast<int>(requiredSize), 0);
    if (SetupDiGetDeviceRegistryPropertyW(devInfo, devInfoData, property, nullptr,
                                          reinterpret_cast<PBYTE>(buffer.data()), requiredSize, nullptr)) {
        return QString::fromWCharArray(reinterpret_cast<LPCWSTR>(buffer.constData()));
    }

    return QString();
}

#elif defined(Q_OS_LINUX)

bool DeviceEnumerator::Private::enumerateLinuxTapeDevices()
{
    // Enumerate /dev/st* and /dev/nst* devices
    QDir devDir(QStringLiteral("/dev"));

    QStringList filters;
    filters << QStringLiteral("st[0-9]*") << QStringLiteral("nst[0-9]*");

    QStringList devices = devDir.entryList(filters, QDir::System);

    // Prefer non-rewind devices (nst*)
    QStringList nstDevices;
    QStringList stDevices;

    for (const QString &dev : devices) {
        if (dev.startsWith(QLatin1String("nst"))) {
            nstDevices.append(dev);
        } else {
            stDevices.append(dev);
        }
    }

    // Process non-rewind devices first
    for (const QString &dev : nstDevices) {
        QString devicePath = QStringLiteral("/dev/") + dev;
        QString genericPath = findGenericDevice(devicePath);

        TapeDeviceInfo info = queryTapeDeviceInfo(devicePath, genericPath);
        if (info.type == DeviceType::TapeDrive) {
            tapeDevices.append(info);
        }
    }

    // Add rewind devices if no corresponding non-rewind found
    for (const QString &dev : stDevices) {
        QString devicePath = QStringLiteral("/dev/") + dev;

        // Check if we already have the non-rewind version
        QString nstDev = QStringLiteral("/dev/n") + dev;
        bool hasNst = false;
        for (const auto &existing : tapeDevices) {
            if (existing.devicePath == nstDev) {
                hasNst = true;
                break;
            }
        }

        if (!hasNst) {
            QString genericPath = findGenericDevice(devicePath);
            TapeDeviceInfo info = queryTapeDeviceInfo(devicePath, genericPath);
            if (info.type == DeviceType::TapeDrive) {
                tapeDevices.append(info);
            }
        }
    }

    return true;
}

bool DeviceEnumerator::Private::enumerateLinuxChangerDevices()
{
    // Enumerate /dev/sch* or /dev/sg* devices that are changers
    QDir devDir(QStringLiteral("/dev"));

    // First try /dev/sch* (dedicated changer devices)
    QStringList schDevices = devDir.entryList(QStringList() << QStringLiteral("sch[0-9]*"), QDir::System);
    for (const QString &dev : schDevices) {
        QString devicePath = QStringLiteral("/dev/") + dev;
        ChangerDeviceInfo info = queryChangerDeviceInfo(devicePath);
        if (info.isValid()) {
            changerDevices.append(info);
        }
    }

    // Also check /dev/sg* devices for changers
    QStringList sgDevices = devDir.entryList(QStringList() << QStringLiteral("sg[0-9]*"), QDir::System);
    for (const QString &dev : sgDevices) {
        QString devicePath = QStringLiteral("/dev/") + dev;

        // Query device type
        ScsiCommand scsi(devicePath);
        if (!scsi.open()) {
            continue;
        }

        auto result = scsi.inquiry(false, 0, 36);
        scsi.close();

        if (result.success && result.data.size() >= 1) {
            quint8 deviceType = static_cast<quint8>(result.data[0]) & 0x1F;
            if (deviceType == SCSI_TYPE_CHANGER) {
                ChangerDeviceInfo info = queryChangerDeviceInfo(devicePath);
                if (info.isValid()) {
                    // Check if already found via sch*
                    bool found = false;
                    for (const auto &existing : changerDevices) {
                        if (existing.serialNumber == info.serialNumber &&
                            !info.serialNumber.isEmpty()) {
                            found = true;
                            break;
                        }
                    }
                    if (!found) {
                        changerDevices.append(info);
                    }
                }
            }
        }
    }

    return true;
}

QString DeviceEnumerator::Private::readSysfsAttribute(const QString &path)
{
    QFile file(path);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString::fromUtf8(file.readLine()).trimmed();
    }
    return QString();
}

QString DeviceEnumerator::Private::findGenericDevice(const QString &tapeDevice)
{
    // Find the corresponding /dev/sg* device for a tape device
    // This is done by matching the SCSI host:channel:id:lun

    // Extract device number from tape device name
    QString baseName = tapeDevice.section(QLatin1Char('/'), -1);
    QString devNum;
    for (const QChar &c : baseName) {
        if (c.isDigit()) {
            devNum += c;
        }
    }

    if (devNum.isEmpty()) {
        return tapeDevice;
    }

    // Read sysfs to find the SCSI device path
    QString sysPath = QStringLiteral("/sys/class/scsi_tape/st%1/device").arg(devNum);
    QFileInfo linkInfo(sysPath);
    if (!linkInfo.exists()) {
        sysPath = QStringLiteral("/sys/class/scsi_tape/nst%1/device").arg(devNum);
        linkInfo.setFile(sysPath);
    }

    if (!linkInfo.exists()) {
        return tapeDevice;
    }

    QString devicePath = linkInfo.canonicalFilePath();

    // Now find the generic device in the same SCSI device directory
    QDir scsiDir(devicePath);
    QStringList filters;
    filters << QStringLiteral("scsi_generic");

    QStringList entries = scsiDir.entryList(filters, QDir::Dirs);
    if (!entries.isEmpty()) {
        QDir sgDir(devicePath + QStringLiteral("/scsi_generic"));
        QStringList sgEntries = sgDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        if (!sgEntries.isEmpty()) {
            return QStringLiteral("/dev/") + sgEntries.first();
        }
    }

    // Alternative: look for sg* directly under device
    entries = scsiDir.entryList(QStringList() << QStringLiteral("sg*"), QDir::Dirs);
    if (!entries.isEmpty()) {
        return QStringLiteral("/dev/") + entries.first();
    }

    return tapeDevice;
}

#endif

// ============================================================================
// DeviceEnumerator Implementation
// ============================================================================

DeviceEnumerator::DeviceEnumerator()
    : d(new Private)
{
}

DeviceEnumerator::~DeviceEnumerator()
{
    delete d;
}

bool DeviceEnumerator::refresh()
{
    d->lastError.clear();

    bool tapeOk = d->enumerateTapeDevices();
    bool changerOk = d->enumerateChangerDevices();

    return tapeOk && changerOk;
}

QList<TapeDeviceInfo> DeviceEnumerator::tapeDevices() const
{
    return d->tapeDevices;
}

QList<ChangerDeviceInfo> DeviceEnumerator::changerDevices() const
{
    return d->changerDevices;
}

TapeDeviceInfo DeviceEnumerator::findTapeDevice(const QString &path) const
{
    for (const auto &info : d->tapeDevices) {
        if (info.devicePath == path || info.genericPath == path) {
            return info;
        }
    }
    return TapeDeviceInfo();
}

ChangerDeviceInfo DeviceEnumerator::findChangerDevice(const QString &path) const
{
    for (const auto &info : d->changerDevices) {
        if (info.devicePath == path) {
            return info;
        }
    }
    return ChangerDeviceInfo();
}

QStringList DeviceEnumerator::tapeDevicePaths() const
{
    QStringList paths;
    for (const auto &info : d->tapeDevices) {
        paths.append(info.devicePath);
    }
    return paths;
}

QStringList DeviceEnumerator::changerDevicePaths() const
{
    QStringList paths;
    for (const auto &info : d->changerDevices) {
        paths.append(info.devicePath);
    }
    return paths;
}

QString DeviceEnumerator::lastError() const
{
    return d->lastError;
}

DeviceType DeviceEnumerator::scsiDeviceType(quint8 peripheralDeviceType)
{
    switch (peripheralDeviceType) {
    case SCSI_TYPE_DISK:      return DeviceType::DiskDrive;
    case SCSI_TYPE_TAPE:      return DeviceType::TapeDrive;
    case SCSI_TYPE_PRINTER:   return DeviceType::Printer;
    case SCSI_TYPE_PROCESSOR: return DeviceType::Processor;
    case SCSI_TYPE_WORM:      return DeviceType::Worm;
    case SCSI_TYPE_CDROM:     return DeviceType::CdDvd;
    case SCSI_TYPE_SCANNER:   return DeviceType::Scanner;
    case SCSI_TYPE_OPTICAL:   return DeviceType::OpticalMemory;
    case SCSI_TYPE_CHANGER:   return DeviceType::MediumChanger;
    case SCSI_TYPE_COMM:      return DeviceType::Communication;
    case SCSI_TYPE_ENCLOSURE: return DeviceType::EnclosureServices;
    default:                  return DeviceType::Unknown;
    }
}

QString DeviceEnumerator::deviceTypeName(DeviceType type)
{
    switch (type) {
    case DeviceType::Unknown:           return QStringLiteral("Unknown");
    case DeviceType::TapeDrive:         return QStringLiteral("Tape Drive");
    case DeviceType::MediumChanger:     return QStringLiteral("Medium Changer");
    case DeviceType::Printer:           return QStringLiteral("Printer");
    case DeviceType::Processor:         return QStringLiteral("Processor");
    case DeviceType::Worm:              return QStringLiteral("WORM");
    case DeviceType::CdDvd:             return QStringLiteral("CD/DVD");
    case DeviceType::Scanner:           return QStringLiteral("Scanner");
    case DeviceType::OpticalMemory:     return QStringLiteral("Optical Memory");
    case DeviceType::DiskDrive:         return QStringLiteral("Disk Drive");
    case DeviceType::Communication:     return QStringLiteral("Communication");
    case DeviceType::ArrayController:   return QStringLiteral("Array Controller");
    case DeviceType::EnclosureServices: return QStringLiteral("Enclosure Services");
    case DeviceType::SimplifiedDisk:    return QStringLiteral("Simplified Disk");
    case DeviceType::OpticalCardReader: return QStringLiteral("Optical Card Reader");
    case DeviceType::BridgeController:  return QStringLiteral("Bridge Controller");
    case DeviceType::ObjectBased:       return QStringLiteral("Object-Based Storage");
    case DeviceType::AutomationDrive:   return QStringLiteral("Automation/Drive Interface");
    case DeviceType::SecurityManager:   return QStringLiteral("Security Manager");
    case DeviceType::ZonedBlock:        return QStringLiteral("Zoned Block");
    case DeviceType::WellKnownLU:       return QStringLiteral("Well Known LU");
    }
    return QStringLiteral("Unknown");
}

bool DeviceEnumerator::isValidTapeDevicePath(const QString &path)
{
#ifdef Q_OS_WIN
    // Windows: \\.\Tape0, \\.\Tape1, etc.
    static QRegularExpression rx(QStringLiteral("^\\\\\\\\.\\\\Tape\\d+$"));
    return rx.match(path).hasMatch();
#elif defined(Q_OS_LINUX)
    // Linux: /dev/st*, /dev/nst*, /dev/sg*
    static QRegularExpression rx(QStringLiteral("^/dev/(n?st\\d+|sg\\d+)$"));
    return rx.match(path).hasMatch();
#else
    Q_UNUSED(path)
    return false;
#endif
}

QString DeviceEnumerator::defaultTapeDevicePath()
{
#ifdef Q_OS_WIN
    return QStringLiteral("\\\\.\\Tape0");
#elif defined(Q_OS_LINUX)
    return QStringLiteral("/dev/nst0");
#else
    return QString();
#endif
}

} // namespace qltfs
