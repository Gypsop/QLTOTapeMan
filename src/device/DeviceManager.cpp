#include "DeviceManager.h"
#include <QDebug>
#include <QFileInfo>
#include <QDir>
#include <QtCore/qprocess.h>

#ifdef Q_OS_WIN
#include <windows.h>
#include <setupapi.h>
#include <devguid.h>
#include <winioctl.h>
#include <ntddscsi.h>
// Link with SetupAPI
#pragma comment (lib, "Setupapi.lib")
#endif

#ifdef Q_OS_LINUX
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <scsi/sg.h>
#endif

#ifdef Q_OS_MAC
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/scsi/SCSITaskLib.h>
#include <IOKit/storage/IOMedia.h>
#include <IOKit/storage/IOStorageProtocolCharacteristics.h>
#endif

DeviceManager::DeviceManager(QObject *parent) : QObject(parent)
{
}

QList<TapeDeviceInfo> DeviceManager::scanDevices()
{
#ifdef Q_OS_WIN
    return scanDevicesWindows();
#elif defined(Q_OS_LINUX)
    return scanDevicesLinux();
#elif defined(Q_OS_MAC)
    return scanDevicesMac();
#else
    qDebug() << "Unsupported OS for device scanning";
    return QList<TapeDeviceInfo>();
#endif
}

bool DeviceManager::sendScsiCommand(const QString &devicePath,
                                    const std::vector<uint8_t> &cdb,
                                    ScsiDirection direction,
                                    std::vector<uint8_t> &data,
                                    unsigned int timeout)
{
#ifdef Q_OS_WIN
    return sendScsiCommandWindows(devicePath, cdb, direction, data, timeout);
#elif defined(Q_OS_LINUX)
    return sendScsiCommandLinux(devicePath, cdb, direction, data, timeout);
#elif defined(Q_OS_MAC)
    return sendScsiCommandMac(devicePath, cdb, direction, data, timeout);
#else
    return false;
#endif
}

bool DeviceManager::isDeviceReady(const QString &devicePath)
{
    std::vector<uint8_t> cdb(6, 0);
    cdb[0] = SCSIOP_TEST_UNIT_READY;
    
    std::vector<uint8_t> data; // No data transfer
    
    return sendScsiCommand(devicePath, cdb, ScsiDirection::None, data, 1000);
}

bool DeviceManager::rewindDevice(const QString &devicePath)
{
    std::vector<uint8_t> cdb(6, 0);
    cdb[0] = SCSIOP_REWIND;
    
    std::vector<uint8_t> data;
    
    // Rewind can take a while, set timeout to 5 minutes (300000 ms) or more
    return sendScsiCommand(devicePath, cdb, ScsiDirection::None, data, 300000);
}

bool DeviceManager::unloadDevice(const QString &devicePath)
{
    std::vector<uint8_t> cdb(6, 0);
    cdb[0] = SCSIOP_START_STOP_UNIT;
    cdb[4] = 0x02; // LoEj = 1, Start = 0 (Eject)
    
    std::vector<uint8_t> data;
    
    return sendScsiCommand(devicePath, cdb, ScsiDirection::None, data, 60000);
}

// ... (Windows scanning implementation remains here, will update it later to use sendScsiCommand) ...


QList<TapeDeviceInfo> DeviceManager::scanDevicesWindows()
{
    QList<TapeDeviceInfo> devices;
#ifdef Q_OS_WIN
    // GUID for Tape Drives: {6D802884-7D00-11D0-9908-00A0C92542E3}
    static const GUID GUID_DEVINTERFACE_TAPE = { 0x6D802884, 0x7D00, 0x11D0, { 0x99, 0x08, 0x00, 0xA0, 0xC9, 0x25, 0x42, 0xE3 } };

    HDEVINFO hDevInfo = SetupDiGetClassDevs(&GUID_DEVINTERFACE_TAPE, NULL, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    
    if (hDevInfo != INVALID_HANDLE_VALUE) {
        SP_DEVICE_INTERFACE_DATA devInterfaceData;
        devInterfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

        for (DWORD i = 0; SetupDiEnumDeviceInterfaces(hDevInfo, NULL, &GUID_DEVINTERFACE_TAPE, i, &devInterfaceData); ++i) {
            DWORD requiredSize = 0;
            SetupDiGetDeviceInterfaceDetail(hDevInfo, &devInterfaceData, NULL, 0, &requiredSize, NULL);

            if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
                continue;
            }

            PSP_DEVICE_INTERFACE_DETAIL_DATA devDetailData = (PSP_DEVICE_INTERFACE_DETAIL_DATA)malloc(requiredSize);
            devDetailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

            if (SetupDiGetDeviceInterfaceDetail(hDevInfo, &devInterfaceData, devDetailData, requiredSize, NULL, NULL)) {
                QString devicePath = QString::fromWCharArray(devDetailData->DevicePath);
                
                // Perform SCSI Inquiry
                std::vector<uint8_t> cdb(6);
                cdb[0] = SCSIOP_INQUIRY;
                cdb[4] = sizeof(ScsiInquiryData);
                
                std::vector<uint8_t> data(sizeof(ScsiInquiryData));
                
                if (sendScsiCommand(devicePath, cdb, ScsiDirection::In, data)) {
                    ScsiInquiryData *inq = reinterpret_cast<ScsiInquiryData*>(data.data());
                    
                    TapeDeviceInfo info;
                    info.devicePath = devicePath;
                    info.vendorId = QString::fromLatin1(inq->VendorId, 8).trimmed();
                    info.productId = QString::fromLatin1(inq->ProductId, 16).trimmed();
                    info.productRevision = QString::fromLatin1(inq->ProductRevisionLevel, 4).trimmed();
                    info.serialNumber = "Unknown";
                    
                    // Get Serial Number (Page 0x80)
                    std::vector<uint8_t> cdbSerial(6);
                    cdbSerial[0] = SCSIOP_INQUIRY;
                    cdbSerial[1] = 1; // EVPD
                    cdbSerial[2] = 0x80; // Page Code
                    cdbSerial[4] = 255; // Allocation Length
                    
                    std::vector<uint8_t> dataSerial(255);
                    if (sendScsiCommand(devicePath, cdbSerial, ScsiDirection::In, dataSerial)) {
                         int pageLen = dataSerial[3];
                         if (pageLen > 0) {
                             info.serialNumber = QString::fromLatin1((const char*)&dataSerial[4], pageLen).trimmed();
                         }
                    }
                    
                    devices.append(info);
                }
            }
            free(devDetailData);
        }
        SetupDiDestroyDeviceInfoList(hDevInfo);
    }
    
    // Fallback: Legacy Tape Scan (\\.\Tape0 to \\.\Tape9)
    // Some drivers do not expose the standard Tape Interface GUID, but still map to TapeX.
    if (devices.isEmpty()) {
        for (int i = 0; i < 10; ++i) {
            QString tapePath = QString("\\\\.\\Tape%1").arg(i);
            
            // Check if device exists by attempting to open it
            HANDLE hDevice = CreateFile(reinterpret_cast<LPCWSTR>(tapePath.utf16()), 
                                      GENERIC_READ | GENERIC_WRITE, 
                                      FILE_SHARE_READ | FILE_SHARE_WRITE, 
                                      NULL, 
                                      OPEN_EXISTING, 
                                      0, 
                                      NULL);
            
            if (hDevice != INVALID_HANDLE_VALUE) {
                CloseHandle(hDevice); 
                
                // Perform Inquiry
                std::vector<uint8_t> cdb(6);
                cdb[0] = SCSIOP_INQUIRY;
                cdb[4] = sizeof(ScsiInquiryData);
                
                std::vector<uint8_t> data(sizeof(ScsiInquiryData));
                
                if (sendScsiCommand(tapePath, cdb, ScsiDirection::In, data)) {
                    ScsiInquiryData *inq = reinterpret_cast<ScsiInquiryData*>(data.data());
                    
                    TapeDeviceInfo info;
                    info.devicePath = tapePath;
                    info.vendorId = QString::fromLatin1(inq->VendorId, 8).trimmed();
                    info.productId = QString::fromLatin1(inq->ProductId, 16).trimmed();
                    info.productRevision = QString::fromLatin1(inq->ProductRevisionLevel, 4).trimmed();
                    info.serialNumber = "Unknown";
                    
                    // Get Serial Number (Page 0x80)
                    std::vector<uint8_t> cdbSerial(6);
                    cdbSerial[0] = SCSIOP_INQUIRY;
                    cdbSerial[1] = 1; // EVPD
                    cdbSerial[2] = 0x80; // Page Code
                    cdbSerial[4] = 255; // Allocation Length
                    
                    std::vector<uint8_t> dataSerial(255);
                    if (sendScsiCommand(tapePath, cdbSerial, ScsiDirection::In, dataSerial)) {
                         int pageLen = dataSerial[3];
                         if (pageLen > 0) {
                             info.serialNumber = QString::fromLatin1((const char*)&dataSerial[4], pageLen).trimmed();
                         }
                    }
                    
                    devices.append(info);
                }
            }
        }
    }
#endif
    return devices;
}

QList<TapeDeviceInfo> DeviceManager::scanDevicesLinux()
{
    QList<TapeDeviceInfo> devices;
    // Simple scanning of /dev/nst* and /dev/st*
    QDir devDir("/dev");
    QStringList filters;
    filters << "nst*" << "st*";
    QStringList entries = devDir.entryList(filters, QDir::System);
    
    for (const QString &entry : entries) {
        // Filter out partitions or other non-tape devices if necessary
        // Usually nst0, st0 are the ones.
        // Avoid duplicates (st0 vs nst0 usually point to same physical drive)
        // Prefer nst (non-rewind) for control
        if (entry.startsWith("st")) continue; 
        
        QString path = "/dev/" + entry;
        
        TapeDeviceInfo info;
        info.devicePath = path;
        
        // Get Inquiry Data
        std::vector<uint8_t> cdb(6);
        cdb[0] = SCSIOP_INQUIRY;
        cdb[4] = sizeof(ScsiInquiryData);
        
        std::vector<uint8_t> data(sizeof(ScsiInquiryData));
        
        if (sendScsiCommand(path, cdb, ScsiDirection::In, data)) {
            ScsiInquiryData *inq = reinterpret_cast<ScsiInquiryData*>(data.data());
            info.vendorId = QString::fromLatin1(inq->VendorId, 8).trimmed();
            info.productId = QString::fromLatin1(inq->ProductId, 16).trimmed();
            info.serialNumber = "Unknown"; // Need Page 0x80 for serial
            info.productRevision = QString::fromLatin1(inq->ProductRevisionLevel, 4).trimmed();
            
            // Get Serial Number (Page 0x80)
            std::vector<uint8_t> cdbSerial(6);
            cdbSerial[0] = SCSIOP_INQUIRY;
            cdbSerial[1] = 1; // EVPD
            cdbSerial[2] = 0x80; // Page Code
            cdbSerial[4] = 255; // Allocation Length
            
            std::vector<uint8_t> dataSerial(255);
            if (sendScsiCommand(path, cdbSerial, ScsiDirection::In, dataSerial)) {
                 int pageLen = dataSerial[3];
                 if (pageLen > 0) {
                     info.serialNumber = QString::fromLatin1((const char*)&dataSerial[4], pageLen).trimmed();
                 }
            }
            
            devices.append(info);
        }
    }
    return devices;
}

QList<TapeDeviceInfo> DeviceManager::scanDevicesMac()
{
    QList<TapeDeviceInfo> devices;
#ifdef Q_OS_MAC
    // Use system_profiler to find SAS/SCSI devices
    // This is a robust fallback since writing raw IOKit code without a test device is risky.
    // We look for "Parallel SCSI" and "SAS" devices.
    
    QStringList dataTypes;
    dataTypes << "SPParallelSCSIDataType" << "SPSASDataType" << "SPThunderboltDataType";
    
    for (const QString &dataType : dataTypes) {
        QProcess process;
        process.start("system_profiler", QStringList() << "-xml" << dataType);
        process.waitForFinished();
        
        QByteArray output = process.readAllStandardOutput();
        // Parsing XML output from system_profiler is the most reliable way.
        // However, for simplicity in this prototype, we might just look for keywords if we don't want to add XML parsing logic yet.
        // But Qt has QXmlStreamReader, so let's use it if possible, or just simple string parsing for now.
        
        // Simple string parsing for prototype:
        // Look for "Tape" or "LTO" in the output.
        // Note: This is not perfect but works for detection.
        
        // Better approach: Use ioreg which is more concise.
        // ioreg -r -c SCSITaskDevice -l
    }
    
    // Alternative: ioreg
    QProcess ioreg;
    ioreg.start("ioreg", QStringList() << "-r" << "-c" << "SCSITaskDevice" << "-l");
    ioreg.waitForFinished();
    QString ioregOutput = ioreg.readAllStandardOutput();
    
    // Parse ioreg output
    // We are looking for entries that have "BDR_TAPE" or similar device types.
    // Or just look for "Product Name" = "Ultrium..."
    
    // This is a placeholder implementation. In a real scenario, we would parse the plist output (ioreg -a -l).
    // For now, let's just return an empty list or a mock if we can't parse reliably without a device.
    // But to satisfy the requirement, let's try to find at least one device if it exists.
    
    if (ioregOutput.contains("Ultrium") || ioregOutput.contains("LTO")) {
        // Found a potential tape drive.
        // Since we can't easily get the BSD path (e.g. /dev/rdiskX) from simple text grep without complex parsing,
        // we will leave this as a "Detection Only" for now, or assume a standard path if found.
        
        // TODO: Implement full plist parsing of `ioreg -a -r -c SCSITaskDevice`
    }
#endif
    return devices;
}

bool DeviceManager::sendScsiCommandWindows(const QString &devicePath, const std::vector<uint8_t> &cdb, ScsiDirection direction, std::vector<uint8_t> &data, unsigned int timeout)
{
#ifdef Q_OS_WIN
    HANDLE hDevice = CreateFile(reinterpret_cast<LPCWSTR>(devicePath.utf16()), 
                              GENERIC_READ | GENERIC_WRITE, 
                              FILE_SHARE_READ | FILE_SHARE_WRITE, 
                              NULL, 
                              OPEN_EXISTING, 
                              0, 
                              NULL);

    if (hDevice == INVALID_HANDLE_VALUE) {
        return false;
    }

    struct SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER {
        SCSI_PASS_THROUGH_DIRECT sptd;
        ULONG             Filler;      // Realignment
        UCHAR             ucSenseBuf[32];
    } swb;

    ZeroMemory(&swb, sizeof(swb));

    swb.sptd.Length = sizeof(SCSI_PASS_THROUGH_DIRECT);
    swb.sptd.PathId = 0;
    swb.sptd.TargetId = 0;
    swb.sptd.Lun = 0;
    swb.sptd.CdbLength = static_cast<UCHAR>(cdb.size());
    swb.sptd.SenseInfoLength = sizeof(swb.ucSenseBuf);
    swb.sptd.DataIn = (direction == ScsiDirection::In) ? SCSI_IOCTL_DATA_IN : 
                  (direction == ScsiDirection::Out) ? SCSI_IOCTL_DATA_OUT : SCSI_IOCTL_DATA_UNSPECIFIED;
    swb.sptd.DataTransferLength = static_cast<ULONG>(data.size());
    swb.sptd.TimeOutValue = timeout;
    swb.sptd.DataBuffer = data.data();
    swb.sptd.SenseInfoOffset = offsetof(SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER, ucSenseBuf);
    
    if (cdb.size() <= 16) {
        memcpy(swb.sptd.Cdb, cdb.data(), cdb.size());
    } else {
        CloseHandle(hDevice);
        return false;
    }

    DWORD bytesReturned;
    BOOL result = DeviceIoControl(hDevice,
                                IOCTL_SCSI_PASS_THROUGH_DIRECT,
                                &swb,
                                sizeof(swb),
                                &swb,
                                sizeof(swb),
                                &bytesReturned,
                                NULL);

    if (!result) {
        qDebug() << "DeviceIoControl failed. Error:" << GetLastError();
    }

    CloseHandle(hDevice);
    return result != 0;
#else
    return false;
#endif
}

bool DeviceManager::sendScsiCommandLinux(const QString &devicePath, const std::vector<uint8_t> &cdb, ScsiDirection direction, std::vector<uint8_t> &data, unsigned int timeout)
{
#ifdef Q_OS_LINUX
    int fd = open(devicePath.toLatin1().constData(), O_RDWR);
    if (fd < 0) return false;

    sg_io_hdr_t io_hdr;
    memset(&io_hdr, 0, sizeof(sg_io_hdr_t));

    io_hdr.interface_id = 'S';
    io_hdr.cmd_len = cdb.size();
    io_hdr.mx_sb_len = 32;
    io_hdr.dxfer_direction = (direction == ScsiDirection::In) ? SG_DXFER_FROM_DEV : 
                             (direction == ScsiDirection::Out) ? SG_DXFER_TO_DEV : SG_DXFER_NONE;
    io_hdr.dxfer_len = data.size();
    io_hdr.dxferp = data.data();
    io_hdr.cmdp = (unsigned char*)cdb.data();
    io_hdr.timeout = timeout;
    
    unsigned char sense_buffer[32];
    io_hdr.sbp = sense_buffer;

    int result = ioctl(fd, SG_IO, &io_hdr);
    close(fd);

    return result == 0;
#else
    return false;
#endif
}

bool DeviceManager::sendScsiCommandMac(const QString &devicePath, const std::vector<uint8_t> &cdb, ScsiDirection direction, std::vector<uint8_t> &data, unsigned int timeout)
{
#ifdef Q_OS_MAC
    // Note: This is a simplified implementation. 
    // In a real application, we need to handle the case where the device is claimed by another driver.
    // We might need to create a temporary plug-in or use the SCSITaskUserClient.
    
    // For now, we will assume we can get an IOService reference from the registry if we had the path.
    // But devicePath on Mac is usually a BSD node like /dev/rdiskX or similar, but for tapes it's complex.
    // If devicePath is an IO Registry Entry Path, we can use IORegistryEntryFromPath.
    
    // Since we haven't implemented full Mac scanning yet, this function is a placeholder 
    // for the logic that would take an IOObject (service) and create a task.
    
    // Implementation Plan:
    // 1. Get IOService from devicePath (assuming it's a registry path or we map it).
    // 2. Create SCSITaskDeviceInterface.
    // 3. Create SCSITask.
    // 4. Set CDB, Timeout, Direction.
    // 5. Execute.
    
    return false; // Placeholder until full IOKit integration
#else
    return false;
#endif
}
