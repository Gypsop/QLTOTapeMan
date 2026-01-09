#include "DeviceManager.h"
#include <QDebug>
#include <QFileInfo>
#include <QDir>
#include <QtCore/qprocess.h>
#include <utility>

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
#include <errno.h>
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
#ifdef Q_OS_WIN
    m_deviceHandle = INVALID_HANDLE_VALUE;
#elif defined(Q_OS_LINUX)
    m_deviceHandle = reinterpret_cast<void*>(-1);
#else
    m_deviceHandle = nullptr;
#endif
}

DeviceManager::HandleEntry* DeviceManager::getHandle(int handleId)
{
    auto it = m_handles.find(handleId);
    if (it == m_handles.end()) return nullptr;
    return &it.value();
}

int DeviceManager::openHandle(const QString &devicePath)
{
#ifdef Q_OS_WIN
    HANDLE hDevice = CreateFile(reinterpret_cast<LPCWSTR>(devicePath.utf16()), 
                              GENERIC_READ | GENERIC_WRITE, 
                              FILE_SHARE_READ | FILE_SHARE_WRITE, 
                              NULL, 
                              OPEN_EXISTING, 
                              0, 
                              NULL);
    if (hDevice == INVALID_HANDLE_VALUE) return -1;
    int id = m_nextHandleId++;
    HandleEntry e; e.path = devicePath; e.nativeHandle = hDevice;
    m_handles.insert(id, e);
    return id;
#elif defined(Q_OS_LINUX)
    int fd = ::open(devicePath.toLatin1().constData(), O_RDWR | O_NONBLOCK);
    if (fd < 0) return -1;
    int id = m_nextHandleId++;
    HandleEntry e; e.path = devicePath; e.nativeHandle = reinterpret_cast<void*>(static_cast<intptr_t>(fd));
    m_handles.insert(id, e);
    return id;
#else
    Q_UNUSED(devicePath);
    return -1;
#endif
}

void DeviceManager::closeHandle(int handleId)
{
    HandleEntry* h = getHandle(handleId);
    if (!h) return;
#ifdef Q_OS_WIN
    if (h->nativeHandle && h->nativeHandle != INVALID_HANDLE_VALUE) {
        CloseHandle(static_cast<HANDLE>(h->nativeHandle));
    }
#elif defined(Q_OS_LINUX)
    if (h->nativeHandle && reinterpret_cast<intptr_t>(h->nativeHandle) != -1) {
        ::close(static_cast<int>(reinterpret_cast<intptr_t>(h->nativeHandle)));
    }
#endif
    m_handles.remove(handleId);
}

bool DeviceManager::isHandleOpen(int handleId) const
{
    return m_handles.contains(handleId);
}

bool DeviceManager::openDevice(const QString &devicePath)
{
    closeDevice(); // Close existing if any

#ifdef Q_OS_WIN
    HANDLE hDevice = CreateFile(reinterpret_cast<LPCWSTR>(devicePath.utf16()), 
                              GENERIC_READ | GENERIC_WRITE, 
                              FILE_SHARE_READ | FILE_SHARE_WRITE, 
                              NULL, 
                              OPEN_EXISTING, 
                              0, 
                              NULL);
    
    if (hDevice != INVALID_HANDLE_VALUE) {
        m_deviceHandle = hDevice;
        m_currentDevicePath = devicePath;
        return true;
    }
#elif defined(Q_OS_LINUX)
    int fd = ::open(devicePath.toLatin1().constData(), O_RDWR | O_NONBLOCK);
    if (fd >= 0) {
        m_deviceHandle = reinterpret_cast<void*>(static_cast<intptr_t>(fd));
        m_currentDevicePath = devicePath;
        return true;
    }
#endif
    return false;
}

void DeviceManager::closeDevice()
{
#ifdef Q_OS_WIN
    if (m_deviceHandle != INVALID_HANDLE_VALUE) {
        CloseHandle(static_cast<HANDLE>(m_deviceHandle));
        m_deviceHandle = INVALID_HANDLE_VALUE;
    }
#elif defined(Q_OS_LINUX)
    if (m_deviceHandle && reinterpret_cast<intptr_t>(m_deviceHandle) != -1) {
        ::close(static_cast<int>(reinterpret_cast<intptr_t>(m_deviceHandle)));
        m_deviceHandle = reinterpret_cast<void*>(-1);
    }
#endif
    m_currentDevicePath.clear();
}

bool DeviceManager::isDeviceOpen() const
{
#ifdef Q_OS_WIN
    return m_deviceHandle != INVALID_HANDLE_VALUE;
#elif defined(Q_OS_LINUX)
    return m_deviceHandle && reinterpret_cast<intptr_t>(m_deviceHandle) != -1;
#else
    return m_deviceHandle != nullptr;
#endif
}

TapeStatus DeviceManager::getDeviceStatus(const QString &devicePath)
{
    TapeStatus status;
    status.statusMessage = "Unknown";

#ifdef Q_OS_WIN
    HANDLE hDevice = CreateFile(reinterpret_cast<LPCWSTR>(devicePath.utf16()), 
                              GENERIC_READ, // Read access for status
                              FILE_SHARE_READ | FILE_SHARE_WRITE, 
                              NULL, 
                              OPEN_EXISTING, 
                              0, 
                              NULL);

    if (hDevice == INVALID_HANDLE_VALUE) {
        status.statusMessage = "Failed to open device";
        return status;
    }

    // 1. Get Tape Status
    DWORD tapeStatus = GetTapeStatus(hDevice);
    status.isReady = (tapeStatus == NO_ERROR);
    
    switch (tapeStatus) {
        case NO_ERROR: status.statusMessage = "Ready"; status.isLoaded = true; break;
        case ERROR_NO_MEDIA_IN_DRIVE: status.statusMessage = "No Media"; status.isLoaded = false; break;
        case ERROR_BUS_RESET: status.statusMessage = "Bus Reset"; break;
        case ERROR_END_OF_MEDIA: status.statusMessage = "End of Media"; status.isLoaded = true; break;
        case ERROR_BEGINNING_OF_MEDIA: status.statusMessage = "Beginning of Media"; status.isLoaded = true; break;
        case ERROR_WRITE_PROTECT: status.statusMessage = "Write Protected"; status.isWriteProtected = true; status.isLoaded = true; break;
        case ERROR_DEVICE_REQUIRES_CLEANING: status.statusMessage = "Cleaning Required"; status.needsCleaning = true; status.isLoaded = true; break;
        case ERROR_MEDIA_CHANGED: status.statusMessage = "Media Changed"; status.isLoaded = true; break;
        case ERROR_DEVICE_NOT_PARTITIONED: status.statusMessage = "Not Partitioned"; status.isLoaded = true; break;
        default: status.statusMessage = QString("Error: %1").arg(tapeStatus); break;
    }

    // 2. Get Media Parameters (if loaded)
    if (status.isLoaded) {
        TAPE_GET_MEDIA_PARAMETERS mediaParams;
        DWORD size = sizeof(mediaParams);
        if (GetTapeParameters(hDevice, GET_TAPE_MEDIA_INFORMATION, &size, &mediaParams) == NO_ERROR) {
            status.capacityBytes = mediaParams.Capacity.QuadPart;
            status.remainingBytes = mediaParams.Remaining.QuadPart;
            status.blockSize = mediaParams.BlockSize;
            status.partitionCount = mediaParams.PartitionCount;
            status.isWriteProtected = mediaParams.WriteProtected;
        }
        
        // Get Position
        DWORD partition = 0;
        DWORD offsetLow = 0;
        DWORD offsetHigh = 0;
        if (GetTapePosition(hDevice, TAPE_ABSOLUTE_POSITION, &partition, &offsetLow, &offsetHigh) == NO_ERROR) {
            status.currentPartition = partition;
            status.currentBlock = ((uint64_t)offsetHigh << 32) | offsetLow;
        }
    }

    // 3. Get Drive Parameters
    TAPE_GET_DRIVE_PARAMETERS driveParams;
    DWORD size = sizeof(driveParams);
    if (GetTapeParameters(hDevice, GET_TAPE_DRIVE_INFORMATION, &size, &driveParams) == NO_ERROR) {
        status.compressionEnabled = driveParams.Compression;
        status.maxBlockSize = driveParams.MaximumBlockSize;
    }

    CloseHandle(hDevice);
#else
    // Non-Windows implementation using SCSI commands
    // We need to open the device to send SCSI commands
    // Note: This might interfere if the device is already open by another operation.
    // Ideally, we should check if it's the same device.
    
    bool wasOpen = isDeviceOpen();
    QString oldPath = m_currentDevicePath;
    
    if (wasOpen && oldPath == devicePath) {
        // Already open, just query
        TapePosition pos = readPosition();
        if (pos.valid) {
            status.currentPartition = pos.partition;
            status.currentBlock = pos.blockNumber;
            status.isReady = true;
            status.isLoaded = true;
            status.statusMessage = "Ready";
        }
        
        BlockLimits limits = readBlockLimits();
        if (limits.valid) {
            status.maxBlockSize = limits.maxBlockLength;
        }
    } else {
        // Try to open
        if (const_cast<DeviceManager*>(this)->openDevice(devicePath)) {
            status.isReady = true;
            status.isLoaded = true;
            status.statusMessage = "Ready";
            
            TapePosition pos = readPosition();
            if (pos.valid) {
                status.currentPartition = pos.partition;
                status.currentBlock = pos.blockNumber;
            }
            
            BlockLimits limits = readBlockLimits();
            if (limits.valid) {
                status.maxBlockSize = limits.maxBlockLength;
            }
            
            const_cast<DeviceManager*>(this)->closeDevice();
        } else {
            status.statusMessage = "Failed to open device";
        }
        
        // Restore previous state if needed (though openDevice closes previous)
        if (wasOpen) {
            const_cast<DeviceManager*>(this)->openDevice(oldPath);
        }
    }
#endif

    return status;
}

TapeStatus DeviceManager::getDeviceStatusHandle(int handleId, const QString &devicePath)
{
    Q_UNUSED(handleId);
    return getDeviceStatus(devicePath);
}

bool DeviceManager::synchronizeCache()
{
    if (!isDeviceOpen()) return false;
    
    // SYNCHRONIZE CACHE (10) - Opcode 0x35
    std::vector<uint8_t> cdb(10, 0);
    cdb[0] = 0x35;
    
    std::vector<uint8_t> data;
    return sendScsiCommand(m_currentDevicePath, cdb, ScsiDirection::None, data, 60000);
}

bool DeviceManager::createPartitionHandle(int handleId, uint8_t method, uint16_t sizeMB)
{
    std::vector<uint8_t> cdb(6);
    cdb[0] = 0x04;
    cdb[1] = 0x00;
    cdb[2] = 0x01;

    uint8_t transferLengthMsb = 0;
    uint8_t transferLengthLsb = 8;
    cdb[3] = transferLengthMsb;
    cdb[4] = transferLengthLsb;

    std::vector<uint8_t> data(8, 0);
    data[0] = 0x00;
    data[1] = method & 0x03;
    data[2] = (sizeMB >> 8) & 0xFF;
    data[3] = sizeMB & 0xFF;
    return sendScsiCommandHandle(handleId, cdb, ScsiDirection::Out, data, 60000);
}

bool DeviceManager::synchronizeCacheHandle(int handleId)
{
    std::vector<uint8_t> cdb(10, 0);
    cdb[0] = 0x35;
    std::vector<uint8_t> data;
    return sendScsiCommandHandle(handleId, cdb, ScsiDirection::None, data, 60000);
}

DeviceManager::ScsiWriteResult DeviceManager::writeScsiBlockHandle(int handleId, const QByteArray &data)
{
    ScsiWriteResult result;
    HandleEntry* h = getHandle(handleId);
    if (!h) {
        result.isError = true;
        result.errorMessage = "Invalid handle";
        return result;
    }

#ifdef Q_OS_WIN
    std::vector<uint8_t> cdb(6);
    cdb[0] = 0x0A;
    uint32_t len = data.size();
    cdb[2] = (len >> 16) & 0xFF;
    cdb[3] = (len >> 8) & 0xFF;
    cdb[4] = len & 0xFF;

    struct SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER {
        SCSI_PASS_THROUGH_DIRECT sptd;
        ULONG             Filler;
        UCHAR             ucSenseBuf[64];
    } swb;

    ZeroMemory(&swb, sizeof(swb));

    swb.sptd.Length = sizeof(SCSI_PASS_THROUGH_DIRECT);
    swb.sptd.CdbLength = 6;
    swb.sptd.SenseInfoLength = sizeof(swb.ucSenseBuf);
    swb.sptd.DataIn = SCSI_IOCTL_DATA_OUT;
    swb.sptd.DataTransferLength = len;
    swb.sptd.TimeOutValue = 60;
    swb.sptd.DataBuffer = const_cast<char*>(data.data());
    swb.sptd.SenseInfoOffset = offsetof(SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER, ucSenseBuf);
    memcpy(swb.sptd.Cdb, cdb.data(), 6);

    DWORD bytesReturned;
    BOOL ioResult = DeviceIoControl(static_cast<HANDLE>(h->nativeHandle),
                                IOCTL_SCSI_PASS_THROUGH_DIRECT,
                                &swb,
                                sizeof(swb),
                                &swb,
                                sizeof(swb),
                                &bytesReturned,
                                NULL);

    if (!ioResult) {
        result.isError = true;
        result.errorMessage = QString("Write failed. Error: %1").arg(GetLastError());
        return result;
    }

    if (swb.sptd.ScsiStatus == SCSISTAT_CHECK_CONDITION) {
        ScsiSenseData* sense = reinterpret_cast<ScsiSenseData*>(swb.ucSenseBuf);
        if (sense->isValid()) {
            if (sense->isEOM()) result.isEOM = true;
            uint8_t key = sense->senseKey();
            if (key != 0x00 && key != 0x01) {
                 result.isError = true;
                 result.errorMessage = QString("SCSI Write Error. Key: %1, ASC: %2, ASCQ: %3")
                                        .arg(key, 2, 16, QChar('0'))
                                        .arg(sense->ASC, 2, 16, QChar('0'))
                                        .arg(sense->ASCQ, 2, 16, QChar('0'));
            }
        }
    } else if (swb.sptd.ScsiStatus != SCSISTAT_GOOD) {
        result.isError = true;
        result.errorMessage = QString("SCSI Status Error: %1").arg(swb.sptd.ScsiStatus);
    }

    return result;
#elif defined(Q_OS_LINUX)
    int fd = static_cast<int>(reinterpret_cast<intptr_t>(h->nativeHandle));
    if (fd < 0) {
        result.isError = true;
        result.errorMessage = "Invalid device handle";
        return result;
    }

    std::vector<uint8_t> cdb(6);
    cdb[0] = SCSIOP_WRITE_6;
    uint32_t len = data.size();
    cdb[2] = (len >> 16) & 0xFF;
    cdb[3] = (len >> 8) & 0xFF;
    cdb[4] = len & 0xFF;

    sg_io_hdr_t io_hdr;
    memset(&io_hdr, 0, sizeof(io_hdr));
    io_hdr.interface_id = 'S';
    io_hdr.cmd_len = cdb.size();
    io_hdr.mx_sb_len = 64;
    io_hdr.dxfer_direction = SG_DXFER_TO_DEV;
    io_hdr.dxfer_len = data.size();
    io_hdr.dxferp = const_cast<char*>(data.data());
    io_hdr.cmdp = cdb.data();
    unsigned char sense_buffer[64] = {0};
    io_hdr.sbp = sense_buffer;
    io_hdr.timeout = 60000;

    if (ioctl(fd, SG_IO, &io_hdr) < 0) {
        result.isError = true;
        result.errorMessage = QString("SG_IO write failed: %1").arg(errno);
        return result;
    }

    if (io_hdr.status == SCSISTAT_CHECK_CONDITION) {
        ScsiSenseData* sense = reinterpret_cast<ScsiSenseData*>(sense_buffer);
        if (sense->isValid()) {
            if (sense->isEOM()) result.isEOM = true;
            uint8_t key = sense->senseKey();
            if (key != 0x00 && key != 0x01) {
                result.isError = true;
                result.errorMessage = QString("SCSI Write Error. Key: %1, ASC: %2, ASCQ: %3")
                                        .arg(key, 2, 16, QChar('0'))
                                        .arg(sense->ASC, 2, 16, QChar('0'))
                                        .arg(sense->ASCQ, 2, 16, QChar('0'));
            }
        }
    } else if (io_hdr.status != SCSISTAT_GOOD) {
        result.isError = true;
        result.errorMessage = QString("SCSI Status Error: %1").arg(io_hdr.status);
    }

    return result;
#else
    result.isError = true;
    result.errorMessage = "Not implemented for this OS";
    return result;
#endif
}

DeviceManager::ScsiWriteResult DeviceManager::writeScsiBlock(const QByteArray &data)
{
    ScsiWriteResult result;
    if (!isDeviceOpen()) {
        result.isError = true;
        result.errorMessage = "Device not open";
        return result;
    }

#ifdef Q_OS_WIN
    std::vector<uint8_t> cdb(6);
    cdb[0] = 0x0A; // WRITE(6)
    
    uint32_t len = data.size();
    // Variable block mode: Transfer Length is bytes
    cdb[2] = (len >> 16) & 0xFF;
    cdb[3] = (len >> 8) & 0xFF;
    cdb[4] = len & 0xFF;

    struct SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER {
        SCSI_PASS_THROUGH_DIRECT sptd;
        ULONG             Filler;
        UCHAR             ucSenseBuf[64];
    } swb;

    ZeroMemory(&swb, sizeof(swb));

    swb.sptd.Length = sizeof(SCSI_PASS_THROUGH_DIRECT);
    swb.sptd.CdbLength = 6;
    swb.sptd.SenseInfoLength = sizeof(swb.ucSenseBuf);
    swb.sptd.DataIn = SCSI_IOCTL_DATA_OUT;
    swb.sptd.DataTransferLength = len;
    swb.sptd.TimeOutValue = 60; // 60 seconds for write
    swb.sptd.DataBuffer = const_cast<char*>(data.data());
    swb.sptd.SenseInfoOffset = offsetof(SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER, ucSenseBuf);
    
    memcpy(swb.sptd.Cdb, cdb.data(), 6);

    DWORD bytesReturned;
    BOOL ioResult = DeviceIoControl(static_cast<HANDLE>(m_deviceHandle),
                                IOCTL_SCSI_PASS_THROUGH_DIRECT,
                                &swb,
                                sizeof(swb),
                                &swb,
                                sizeof(swb),
                                &bytesReturned,
                                NULL);
    
    if (!ioResult) {
        result.isError = true;
        result.errorMessage = QString("Write failed. Error: %1").arg(GetLastError());
        return result;
    }
    
    if (swb.sptd.ScsiStatus == SCSISTAT_CHECK_CONDITION) {
        ScsiSenseData* sense = reinterpret_cast<ScsiSenseData*>(swb.ucSenseBuf);
        if (sense->isValid()) {
            if (sense->isEOM()) {
                result.isEOM = true;
            }
            
            uint8_t key = sense->senseKey();
            // Key 0 (No Sense) and 1 (Recovered Error) are not fatal
            if (key != 0x00 && key != 0x01) {
                 result.isError = true;
                 result.errorMessage = QString("SCSI Write Error. Key: %1, ASC: %2, ASCQ: %3")
                                        .arg(key, 2, 16, QChar('0'))
                                        .arg(sense->ASC, 2, 16, QChar('0'))
                                        .arg(sense->ASCQ, 2, 16, QChar('0'));
            }
        }
    } else if (swb.sptd.ScsiStatus != SCSISTAT_GOOD) {
        result.isError = true;
        result.errorMessage = QString("SCSI Status Error: %1").arg(swb.sptd.ScsiStatus);
    }
    
    return result;
#elif defined(Q_OS_LINUX)
    int fd = static_cast<int>(reinterpret_cast<intptr_t>(m_deviceHandle));
    if (fd < 0) {
        result.isError = true;
        result.errorMessage = "Invalid device handle";
        return result;
    }

    std::vector<uint8_t> cdb(6);
    cdb[0] = SCSIOP_WRITE_6;
    uint32_t len = data.size();
    cdb[2] = (len >> 16) & 0xFF;
    cdb[3] = (len >> 8) & 0xFF;
    cdb[4] = len & 0xFF;

    sg_io_hdr_t io_hdr;
    memset(&io_hdr, 0, sizeof(io_hdr));
    io_hdr.interface_id = 'S';
    io_hdr.cmd_len = cdb.size();
    io_hdr.mx_sb_len = 64;
    io_hdr.dxfer_direction = SG_DXFER_TO_DEV;
    io_hdr.dxfer_len = data.size();
    io_hdr.dxferp = const_cast<char*>(data.data());
    io_hdr.cmdp = cdb.data();
    unsigned char sense_buffer[64] = {0};
    io_hdr.sbp = sense_buffer;
    io_hdr.timeout = 60000; // 60s

    if (ioctl(fd, SG_IO, &io_hdr) < 0) {
        result.isError = true;
        result.errorMessage = QString("SG_IO write failed: %1").arg(errno);
        return result;
    }

    if (io_hdr.status == SCSISTAT_CHECK_CONDITION) {
        ScsiSenseData* sense = reinterpret_cast<ScsiSenseData*>(sense_buffer);
        if (sense->isValid()) {
            if (sense->isEOM()) result.isEOM = true;
            uint8_t key = sense->senseKey();
            if (key != 0x00 && key != 0x01) {
                result.isError = true;
                result.errorMessage = QString("SCSI Write Error. Key: %1, ASC: %2, ASCQ: %3")
                                        .arg(key, 2, 16, QChar('0'))
                                        .arg(sense->ASC, 2, 16, QChar('0'))
                                        .arg(sense->ASCQ, 2, 16, QChar('0'));
            }
        }
    } else if (io_hdr.status != SCSISTAT_GOOD) {
        result.isError = true;
        result.errorMessage = QString("SCSI Status Error: %1").arg(io_hdr.status);
    }

    return result;
#else
    result.isError = true;
    result.errorMessage = "Not implemented for this OS";
    return result;
#endif
}

DeviceManager::ScsiReadResult DeviceManager::readScsiBlockHandle(int handleId, uint32_t length)
{
    ScsiReadResult result;
    HandleEntry* h = getHandle(handleId);
    if (!h) {
        result.isError = true;
        result.errorMessage = "Invalid handle";
        return result;
    }

#ifdef Q_OS_WIN
    std::vector<uint8_t> cdb(6);
    cdb[0] = 0x08;
    cdb[2] = (length >> 16) & 0xFF;
    cdb[3] = (length >> 8) & 0xFF;
    cdb[4] = length & 0xFF;

    QByteArray data(length, 0);

    struct SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER {
        SCSI_PASS_THROUGH_DIRECT sptd;
        ULONG             Filler;
        UCHAR             ucSenseBuf[64];
    } swb;

    ZeroMemory(&swb, sizeof(swb));

    swb.sptd.Length = sizeof(SCSI_PASS_THROUGH_DIRECT);
    swb.sptd.CdbLength = 6;
    swb.sptd.SenseInfoLength = sizeof(swb.ucSenseBuf);
    swb.sptd.DataIn = SCSI_IOCTL_DATA_IN;
    swb.sptd.DataTransferLength = length;
    swb.sptd.TimeOutValue = 60;
    swb.sptd.DataBuffer = data.data();
    swb.sptd.SenseInfoOffset = offsetof(SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER, ucSenseBuf);
    memcpy(swb.sptd.Cdb, cdb.data(), 6);

    DWORD bytesReturned;
    BOOL ioResult = DeviceIoControl(static_cast<HANDLE>(h->nativeHandle),
                                IOCTL_SCSI_PASS_THROUGH_DIRECT,
                                &swb,
                                sizeof(swb),
                                &swb,
                                sizeof(swb),
                                &bytesReturned,
                                NULL);

    if (!ioResult) {
        result.isError = true;
        result.errorMessage = QString("DeviceIoControl failed. Error: %1").arg(GetLastError());
        return result;
    }

    if (swb.sptd.ScsiStatus == SCSISTAT_CHECK_CONDITION) {
        ScsiSenseData* sense = reinterpret_cast<ScsiSenseData*>(swb.ucSenseBuf);
        if (sense->isValid()) {
            if (sense->isFileMark()) result.isFileMark = true;
            if (sense->isEOM()) result.isEOM = true;
            uint8_t key = sense->senseKey();
            if (key == 0x08) result.isEOD = true;
            if (sense->isILI()) {
                uint32_t info = (sense->Information[0] << 24) | (sense->Information[1] << 16) | 
                                (sense->Information[2] << 8) | sense->Information[3];
                if (info > 0 && info <= length) {
                    data.resize(length - info);
                }
            }

            if (key != 0x00 && key != 0x02 && !result.isFileMark && !result.isEOM && !result.isEOD && !sense->isILI()) {
                 result.isError = true;
                 result.errorMessage = QString("SCSI Error. Sense Key: %1, ASC: %2, ASCQ: %3")
                                        .arg(key, 2, 16, QChar('0'))
                                        .arg(sense->ASC, 2, 16, QChar('0'))
                                        .arg(sense->ASCQ, 2, 16, QChar('0'));
            }
        }
    } else if (swb.sptd.ScsiStatus != SCSISTAT_GOOD) {
        result.isError = true;
        result.errorMessage = QString("SCSI Status Error: %1").arg(swb.sptd.ScsiStatus);
    }

    if (result.isFileMark || result.isEOD) {
        data.clear();
    }
    if (!result.isError) result.data = data;
    return result;
#elif defined(Q_OS_LINUX)
    int fd = static_cast<int>(reinterpret_cast<intptr_t>(h->nativeHandle));
    if (fd < 0) {
        result.isError = true;
        result.errorMessage = "Invalid device handle";
        return result;
    }

    std::vector<uint8_t> cdb(6);
    cdb[0] = SCSIOP_READ_6;
    cdb[2] = (length >> 16) & 0xFF;
    cdb[3] = (length >> 8) & 0xFF;
    cdb[4] = length & 0xFF;

    QByteArray data(length, 0);

    sg_io_hdr_t io_hdr;
    memset(&io_hdr, 0, sizeof(io_hdr));
    io_hdr.interface_id = 'S';
    io_hdr.cmd_len = cdb.size();
    io_hdr.mx_sb_len = 64;
    io_hdr.dxfer_direction = SG_DXFER_FROM_DEV;
    io_hdr.dxfer_len = data.size();
    io_hdr.dxferp = data.data();
    io_hdr.cmdp = cdb.data();
    unsigned char sense_buffer[64] = {0};
    io_hdr.sbp = sense_buffer;
    io_hdr.timeout = 60000;

    if (ioctl(fd, SG_IO, &io_hdr) < 0) {
        result.isError = true;
        result.errorMessage = QString("SG_IO read failed: %1").arg(errno);
        return result;
    }

    if (io_hdr.status == SCSISTAT_CHECK_CONDITION) {
        ScsiSenseData* sense = reinterpret_cast<ScsiSenseData*>(sense_buffer);
        if (sense->isValid()) {
            if (sense->isFileMark()) result.isFileMark = true;
            if (sense->isEOM()) result.isEOM = true;
            uint8_t key = sense->senseKey();
            if (key == 0x08) result.isEOD = true;
            if (sense->isILI()) {
                uint32_t info = (sense->Information[0] << 24) | (sense->Information[1] << 16) |
                                (sense->Information[2] << 8) | sense->Information[3];
                if (info > 0 && info <= (uint32_t)data.size()) {
                    data.resize(data.size() - info);
                }
            }
            if (key != 0x00 && key != 0x02 && !result.isFileMark && !result.isEOM && !result.isEOD && !sense->isILI()) {
                result.isError = true;
                result.errorMessage = QString("SCSI Error. Sense Key: %1, ASC: %2, ASCQ: %3")
                                        .arg(key, 2, 16, QChar('0'))
                                        .arg(sense->ASC, 2, 16, QChar('0'))
                                        .arg(sense->ASCQ, 2, 16, QChar('0'));
            }
        }
    } else if (io_hdr.status != SCSISTAT_GOOD) {
        result.isError = true;
        result.errorMessage = QString("SCSI Status Error: %1").arg(io_hdr.status);
    }

    if (result.isFileMark || result.isEOD) {
        data.clear();
    }
    if (!result.isError) result.data = data;
    return result;
#else
    result.isError = true;
    result.errorMessage = "Not implemented for this OS";
    return result;
#endif
}

DeviceManager::ScsiReadResult DeviceManager::readScsiBlock(uint32_t length)
{
    ScsiReadResult result;
    if (!isDeviceOpen()) {
        result.isError = true;
        result.errorMessage = "Device not open";
        return result;
    }

#ifdef Q_OS_WIN
    std::vector<uint8_t> cdb(6);
    cdb[0] = 0x08; // READ(6)
    
    cdb[2] = (length >> 16) & 0xFF;
    cdb[3] = (length >> 8) & 0xFF;
    cdb[4] = length & 0xFF;

    QByteArray data(length, 0);

    struct SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER {
        SCSI_PASS_THROUGH_DIRECT sptd;
        ULONG             Filler;
        UCHAR             ucSenseBuf[64];
    } swb;

    ZeroMemory(&swb, sizeof(swb));

    swb.sptd.Length = sizeof(SCSI_PASS_THROUGH_DIRECT);
    swb.sptd.CdbLength = 6;
    swb.sptd.SenseInfoLength = sizeof(swb.ucSenseBuf);
    swb.sptd.DataIn = SCSI_IOCTL_DATA_IN;
    swb.sptd.DataTransferLength = length;
    swb.sptd.TimeOutValue = 60;
    swb.sptd.DataBuffer = data.data();
    swb.sptd.SenseInfoOffset = offsetof(SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER, ucSenseBuf);
    
    memcpy(swb.sptd.Cdb, cdb.data(), 6);

    DWORD bytesReturned;
    BOOL ioResult = DeviceIoControl(static_cast<HANDLE>(m_deviceHandle),
                                IOCTL_SCSI_PASS_THROUGH_DIRECT,
                                &swb,
                                sizeof(swb),
                                &swb,
                                sizeof(swb),
                                &bytesReturned,
                                NULL);
    
    if (!ioResult) {
        result.isError = true;
        result.errorMessage = QString("DeviceIoControl failed. Error: %1").arg(GetLastError());
        return result;
    }

    if (swb.sptd.ScsiStatus == SCSISTAT_CHECK_CONDITION) {
        ScsiSenseData* sense = reinterpret_cast<ScsiSenseData*>(swb.ucSenseBuf);
        if (sense->isValid()) {
            if (sense->isFileMark()) result.isFileMark = true;
            if (sense->isEOM()) result.isEOM = true;
            
            uint8_t key = sense->senseKey();
            if (key == 0x08) result.isEOD = true; // BLANK CHECK
            
            // ILI (Incorrect Length Indicator)
            if (sense->isILI()) {
                uint32_t info = (sense->Information[0] << 24) | (sense->Information[1] << 16) | 
                                (sense->Information[2] << 8) | sense->Information[3];
                if (info > 0 && info <= length) {
                    data.resize(length - info);
                }
            }

            if (key != 0x00 && key != 0x02 && !result.isFileMark && !result.isEOM && !result.isEOD && !sense->isILI()) {
                 result.isError = true;
                 result.errorMessage = QString("SCSI Error. Sense Key: %1, ASC: %2, ASCQ: %3")
                                        .arg(key, 2, 16, QChar('0'))
                                        .arg(sense->ASC, 2, 16, QChar('0'))
                                        .arg(sense->ASCQ, 2, 16, QChar('0'));
            }
        }
    } else if (swb.sptd.ScsiStatus != SCSISTAT_GOOD) {
        result.isError = true;
        result.errorMessage = QString("SCSI Status Error: %1").arg(swb.sptd.ScsiStatus);
    }
    
    if (result.isFileMark || result.isEOD) {
        data.clear();
    }
    
    if (!result.isError) {
        result.data = data;
    }
    return result;
#elif defined(Q_OS_LINUX)
    int fd = static_cast<int>(reinterpret_cast<intptr_t>(m_deviceHandle));
    if (fd < 0) {
        result.isError = true;
        result.errorMessage = "Invalid device handle";
        return result;
    }

    std::vector<uint8_t> cdb(6);
    cdb[0] = SCSIOP_READ_6;
    cdb[2] = (length >> 16) & 0xFF;
    cdb[3] = (length >> 8) & 0xFF;
    cdb[4] = length & 0xFF;

    QByteArray data(length, 0);
    sg_io_hdr_t io_hdr;
    memset(&io_hdr, 0, sizeof(io_hdr));
    io_hdr.interface_id = 'S';
    io_hdr.cmd_len = cdb.size();
    io_hdr.mx_sb_len = 64;
    io_hdr.dxfer_direction = SG_DXFER_FROM_DEV;
    io_hdr.dxfer_len = length;
    io_hdr.dxferp = data.data();
    io_hdr.cmdp = cdb.data();
    unsigned char sense_buffer[64] = {0};
    io_hdr.sbp = sense_buffer;
    io_hdr.timeout = 60000; // 60s

    if (ioctl(fd, SG_IO, &io_hdr) < 0) {
        result.isError = true;
        result.errorMessage = QString("SG_IO read failed: %1").arg(errno);
        return result;
    }

    if (io_hdr.status == SCSISTAT_CHECK_CONDITION) {
        ScsiSenseData* sense = reinterpret_cast<ScsiSenseData*>(sense_buffer);
        if (sense->isValid()) {
            if (sense->isFileMark()) result.isFileMark = true;
            if (sense->isEOM()) result.isEOM = true;
            uint8_t key = sense->senseKey();
            if (key == 0x08) result.isEOD = true; // Blank check
            if (sense->isILI()) {
                uint32_t info = (sense->Information[0] << 24) | (sense->Information[1] << 16) |
                                (sense->Information[2] << 8) | sense->Information[3];
                if (info > 0 && info <= length) {
                    data.resize(length - info);
                }
            }
            if (key != 0x00 && key != 0x02 && !result.isFileMark && !result.isEOM && !result.isEOD && !sense->isILI()) {
                result.isError = true;
                result.errorMessage = QString("SCSI Error. Sense Key: %1, ASC: %2, ASCQ: %3")
                                        .arg(key, 2, 16, QChar('0'))
                                        .arg(sense->ASC, 2, 16, QChar('0'))
                                        .arg(sense->ASCQ, 2, 16, QChar('0'));
            }
        }
    } else if (io_hdr.status != SCSISTAT_GOOD) {
        result.isError = true;
        result.errorMessage = QString("SCSI Status Error: %1").arg(io_hdr.status);
    }

    if (result.isFileMark || result.isEOD) {
        data.clear();
    }
    if (!result.isError) {
        result.data = data;
    }
    return result;
#else
    result.isError = true;
    result.errorMessage = "Not implemented on this OS";
    return result;
#endif
}

bool DeviceManager::writeFileMark(uint8_t count)
{
    if (!isDeviceOpen()) return false;
    
    // WRITE FILEMARKS (6) - Opcode 0x10
    std::vector<uint8_t> cdb(6);
    cdb[0] = 0x10;
    cdb[2] = (count >> 16) & 0xFF;
    cdb[3] = (count >> 8) & 0xFF;
    cdb[4] = count & 0xFF;
    
    std::vector<uint8_t> data; // No data
    
    return sendScsiCommand(m_currentDevicePath, cdb, ScsiDirection::None, data, 60000);
}

DeviceManager::BlockLimits DeviceManager::readBlockLimits()
{
    BlockLimits limits;
    if (!isDeviceOpen()) return limits;

    // READ BLOCK LIMITS (05h)
    std::vector<uint8_t> cdb(6, 0);
    cdb[0] = SCSIOP_READ_BLOCK_LIMITS;
    
    std::vector<uint8_t> data(6, 0);
    
    if (sendScsiCommand(m_currentDevicePath, cdb, ScsiDirection::In, data, 5000)) {
        // Byte 0: Reserved
        // Byte 1-3: Max Block Length
        limits.maxBlockLength = (data[1] << 16) | (data[2] << 8) | data[3];
        // Byte 4-5: Min Block Length
        limits.minBlockLength = (data[4] << 8) | data[5];
        limits.valid = true;
    }
    
    return limits;
}

DeviceManager::BlockLimits DeviceManager::readBlockLimitsHandle(int handleId)
{
    BlockLimits limits;
    std::vector<uint8_t> cdb(6, 0);
    cdb[0] = SCSIOP_READ_BLOCK_LIMITS;

    std::vector<uint8_t> data(6, 0);

    if (sendScsiCommandHandle(handleId, cdb, ScsiDirection::In, data, 5000)) {
        limits.maxBlockLength = (data[1] << 16) | (data[2] << 8) | data[3];
        limits.minBlockLength = (data[4] << 8) | data[5];
        limits.valid = true;
    }

    return limits;
}

DeviceManager::TapePosition DeviceManager::readPosition()
{
    TapePosition pos;
    if (!isDeviceOpen()) return pos;

    // READ POSITION (34h) - Short Form
    std::vector<uint8_t> cdb(10, 0);
    cdb[0] = SCSIOP_READ_POSITION;
    
    // Allocation Length = 20 bytes
    std::vector<uint8_t> data(20, 0);
    
    if (sendScsiCommand(m_currentDevicePath, cdb, ScsiDirection::In, data, 5000)) {
        // Byte 0: Flags
        pos.bop = (data[0] & 0x80) != 0;
        pos.eop = (data[0] & 0x40) != 0;
        // BPU (Block Position Unknown) = Bit 2
        bool bpu = (data[0] & 0x04) != 0;
        
        if (!bpu) {
            // Byte 1: Partition Number
            pos.partition = data[1];
            
            // Byte 4-7: First Block Location
            pos.blockNumber = ((uint32_t)data[4] << 24) | ((uint32_t)data[5] << 16) | 
                              ((uint32_t)data[6] << 8) | (uint32_t)data[7];
            
            pos.valid = true;
        }
    }
    
    return pos;
}

DeviceManager::TapePosition DeviceManager::readPositionHandle(int handleId)
{
    TapePosition pos;
    std::vector<uint8_t> cdb(10, 0);
    cdb[0] = SCSIOP_READ_POSITION;

    std::vector<uint8_t> data(20, 0);
    if (sendScsiCommandHandle(handleId, cdb, ScsiDirection::In, data, 5000)) {
        pos.bop = (data[0] & 0x80) != 0;
        pos.eop = (data[0] & 0x40) != 0;
        bool bpu = (data[0] & 0x04) != 0;
        if (!bpu) {
            pos.partition = data[1];
            pos.blockNumber = ((uint32_t)data[4] << 24) | ((uint32_t)data[5] << 16) |
                              ((uint32_t)data[6] << 8) | (uint32_t)data[7];
            pos.valid = true;
        }
    }
    return pos;
}

bool DeviceManager::setBlockSize(uint32_t blockSize)
{
    if (!isDeviceOpen()) return false;

    // MODE SELECT (6) - Opcode 0x15
    // We need to set the Block Descriptor Length and the Block Size
    
    std::vector<uint8_t> cdb(6);
    cdb[0] = 0x15; // MODE SELECT(6)
    cdb[1] = 0x10; // PF=1 (Page Format)
    cdb[4] = 12;   // Parameter List Length (Header(4) + BlockDescriptor(8))

    // Parameter List
    // Header: 4 bytes
    // Block Descriptor: 8 bytes
    std::vector<uint8_t> data(12, 0);
    
    // Header
    data[0] = 0; // Mode Data Length (Reserved)
    data[1] = 0; // Medium Type (0=Default)
    data[2] = 0x10; // Device Specific Parameter (Speed=0, Buffered Mode=1)
    data[3] = 8; // Block Descriptor Length
    
    // Block Descriptor
    // Bytes 0-3: Density Code (0=Default) and Number of Blocks (0=All)
    // Bytes 4: Reserved
    // Bytes 5-7: Block Length
    data[5] = (blockSize >> 16) & 0xFF;
    data[6] = (blockSize >> 8) & 0xFF;
    data[7] = blockSize & 0xFF;

    return sendScsiCommand(m_currentDevicePath, cdb, ScsiDirection::Out, data, 5000);
}

bool DeviceManager::setBlockSizeHandle(int handleId, uint32_t blockSize)
{
    std::vector<uint8_t> cdb(6);
    cdb[0] = 0x15;
    cdb[1] = 0x10;
    cdb[4] = 12;

    std::vector<uint8_t> data(12, 0);
    data[2] = 0x10;
    data[3] = 8;
    data[5] = (blockSize >> 16) & 0xFF;
    data[6] = (blockSize >> 8) & 0xFF;
    data[7] = blockSize & 0xFF;

    return sendScsiCommandHandle(handleId, cdb, ScsiDirection::Out, data, 5000);
}

bool DeviceManager::writeSetMark(uint8_t count)
{
    if (!isDeviceOpen()) return false;
    
    // WRITE FILEMARKS (6) - Opcode 0x10
    // WSmk = 1 (Bit 1 of Byte 1)
    std::vector<uint8_t> cdb(6);
    cdb[0] = 0x10;
    cdb[1] = 0x02; // WSmk=1
    cdb[2] = (count >> 16) & 0xFF;
    cdb[3] = (count >> 8) & 0xFF;
    cdb[4] = count & 0xFF;
    
    std::vector<uint8_t> data;
    return sendScsiCommand(m_currentDevicePath, cdb, ScsiDirection::None, data, 60000);
}

bool DeviceManager::writeSetMarkHandle(int handleId, uint8_t count)
{
    std::vector<uint8_t> cdb(6);
    cdb[0] = 0x10;
    cdb[1] = 0x02;
    cdb[2] = (count >> 16) & 0xFF;
    cdb[3] = (count >> 8) & 0xFF;
    cdb[4] = count & 0xFF;
    std::vector<uint8_t> data;
    return sendScsiCommandHandle(handleId, cdb, ScsiDirection::None, data, 60000);
}

bool DeviceManager::writeFileMarkHandle(int handleId, uint8_t count)
{
    std::vector<uint8_t> cdb(6);
    cdb[0] = 0x10;
    cdb[2] = (count >> 16) & 0xFF;
    cdb[3] = (count >> 8) & 0xFF;
    cdb[4] = count & 0xFF;
    std::vector<uint8_t> data;
    return sendScsiCommandHandle(handleId, cdb, ScsiDirection::None, data, 60000);
}

bool DeviceManager::eraseTape(bool longErase)
{
    if (!isDeviceOpen()) return false;
    
    // ERASE (6) - Opcode 0x19
    std::vector<uint8_t> cdb(6);
    cdb[0] = 0x19;
    if (longErase) {
        cdb[1] = 0x01; // Long bit = 1
    }
    
    std::vector<uint8_t> data;
    // Long erase can take HOURS. Short erase is fast.
    unsigned int timeout = longErase ? 14400000 : 300000; // 4 hours or 5 mins
    
    return sendScsiCommand(m_currentDevicePath, cdb, ScsiDirection::None, data, timeout);
}

bool DeviceManager::eraseTapeHandle(int handleId, bool longErase)
{
    std::vector<uint8_t> cdb(6);
    cdb[0] = 0x19;
    if (longErase) cdb[1] = 0x01;
    std::vector<uint8_t> data;
    unsigned int timeout = longErase ? 14400000 : 300000;
    return sendScsiCommandHandle(handleId, cdb, ScsiDirection::None, data, timeout);
}

bool DeviceManager::createPartition(uint8_t method, uint16_t sizeMB)
{
    if (!isDeviceOpen()) return false;
    
    // FORMAT MEDIUM (Opcode 0x04) is used to partition LTO tapes
    // But usually we use MODE SELECT to set the partition mode page first?
    // Actually, LTO-5+ uses FORMAT MEDIUM with specific data.
    
    // However, the reference code uses TAPE_INITIATOR_PARTITIONS which maps to CreateTapePartition (WinAPI)
    // or specific SCSI commands.
    // Let's implement the SCSI way: FORMAT MEDIUM (04h)
    
    std::vector<uint8_t> cdb(6);
    cdb[0] = 0x04; // FORMAT MEDIUM
    cdb[1] = 0x00; // Verify=0, Immed=0
    cdb[2] = 0x01; // Format=1 (Partition)
    
    // Transfer Length = 0 (Default) or specific if we need to send data?
    // For LTO partitioning, we usually need to send a parameter list.
    // Let's look at how mkltfs does it or standard SCSI.
    // Standard: FORMAT MEDIUM with Transfer Length > 0 and data containing Partition Header.
    
    // Simplified for now: We will use the Medium Partition Page (11h) via MODE SELECT
    // followed by FORMAT MEDIUM.
    
    // 1. MODE SELECT to set Medium Partition Page (11h)
    std::vector<uint8_t> modeCdb(10);
    modeCdb[0] = 0x55; // MODE SELECT(10)
    modeCdb[1] = 0x10; // PF=1
    
    // Mode Parameter Header(8) + Block Descriptor(0) + Page(11h)
    // Page 11h size is usually 8 + 2*PartitionCount bytes.
    // LTO-5+ supports 2 partitions.
    
    int paramLen = 8 + 10; // Header + Page(10 bytes for 2 partitions)
    modeCdb[7] = (paramLen >> 8) & 0xFF;
    modeCdb[8] = paramLen & 0xFF;
    
    std::vector<uint8_t> modeData(paramLen, 0);
    
    // Mode Parameter Header (10)
    // Bytes 0-1: Mode Data Length (Reserved)
    // Byte 2: Medium Type
    // Byte 3: Device Specific
    // Bytes 6-7: Block Descriptor Length (0)
    
    // Medium Partition Page (11h) starts at offset 8
    int pageOffset = 8;
    modeData[pageOffset] = 0x11; // Page Code
    modeData[pageOffset+1] = 0x08; // Page Length (8 bytes)
    modeData[pageOffset+2] = 1; // Maximum Additional Partitions (1 means 2 partitions total)
    modeData[pageOffset+3] = 0; // Additional Partitions Defined (0 for now, we are setting it up)
    modeData[pageOffset+4] = 0x03; // FDP=0, SDP=0, IDP=1 (Initiator Defined Partition)
    if (method == 0) modeData[pageOffset+4] = 0x03; // IDP
    
    // For this iteration, let's implement a basic 2-partition setup (LTFS style)
    // Partition 0: Data, Partition 1: Index
    // Note: LTO partitions are 0 and 1. 
    // If IDP=1, we specify size of Partition 1.
    
    modeData[pageOffset+4] = 0x30; // IDP=1, PSUM=1 (MB)
    modeData[pageOffset+5] = 1; // Medium Format Recognition (01h = Format Partition)
    
    // Partition 0 Size (Rest)
    modeData[pageOffset+6] = 0xFF; 
    modeData[pageOffset+7] = 0xFF;
    
    // Partition 1 Size (Index) - sizeMB
    modeData[pageOffset+8] = (sizeMB >> 8) & 0xFF;
    modeData[pageOffset+9] = sizeMB & 0xFF;
    
    if (!sendScsiCommand(m_currentDevicePath, modeCdb, ScsiDirection::Out, modeData, 5000)) {
        return false;
    }
    
    // 2. FORMAT MEDIUM
    cdb[0] = 0x04;
    cdb[1] = 0x00; // Immed=0
    cdb[2] = 0x01; // Format=1 (Use Mode Page 11h)
    
    std::vector<uint8_t> noData;
    return sendScsiCommand(m_currentDevicePath, cdb, ScsiDirection::None, noData, 300000); // 5 mins
}

bool DeviceManager::space(int32_t count, uint8_t code)
{
    if (!isDeviceOpen()) return false;
    
    // SPACE (6) - Opcode 0x11
    std::vector<uint8_t> cdb(6);
    cdb[0] = 0x11;
    cdb[1] = code & 0x0F; // Code (0=Blocks, 1=Filemarks, 3=End of Data)
    
    // Count is 24-bit signed integer in CDB(6)
    // If count is negative, it's 2's complement
    cdb[2] = (count >> 16) & 0xFF;
    cdb[3] = (count >> 8) & 0xFF;
    cdb[4] = count & 0xFF;
    
    std::vector<uint8_t> data; // No data
    
    // Space can take time, especially for large counts or filemarks
    // Default to 5 minutes
    return sendScsiCommand(m_currentDevicePath, cdb, ScsiDirection::None, data, 300000);
}

bool DeviceManager::spaceHandle(int handleId, int32_t count, uint8_t code)
{
    std::vector<uint8_t> cdb(6);
    cdb[0] = 0x11;
    cdb[1] = code & 0x0F;
    cdb[2] = (count >> 16) & 0xFF;
    cdb[3] = (count >> 8) & 0xFF;
    cdb[4] = count & 0xFF;
    std::vector<uint8_t> data;
    return sendScsiCommandHandle(handleId, cdb, ScsiDirection::None, data, 300000);
}

bool DeviceManager::locate(uint64_t blockAddress, uint32_t partition)
{
    if (!isDeviceOpen()) return false;
    
    // Try LOCATE(16) first (Opcode 0x92)
    std::vector<uint8_t> cdb(16, 0);
    cdb[0] = 0x92;
    cdb[1] = 0x02; // CP=1 (Change Partition), BT=0
    cdb[2] = partition;
    
    cdb[4] = (blockAddress >> 56) & 0xFF;
    cdb[5] = (blockAddress >> 48) & 0xFF;
    cdb[6] = (blockAddress >> 40) & 0xFF;
    cdb[7] = (blockAddress >> 32) & 0xFF;
    cdb[8] = (blockAddress >> 24) & 0xFF;
    cdb[9] = (blockAddress >> 16) & 0xFF;
    cdb[10] = (blockAddress >> 8) & 0xFF;
    cdb[11] = blockAddress & 0xFF;
    
    std::vector<uint8_t> data;
    if (sendScsiCommand(m_currentDevicePath, cdb, ScsiDirection::None, data, 60000)) {
        return true;
    }
    
    // Fallback to LOCATE(10) if address fits in 32 bits
    if (blockAddress > 0xFFFFFFFF) return false;
    
    std::vector<uint8_t> cdb10(10, 0);
    cdb10[0] = 0x2B;
    cdb10[1] = 0x02; // CP=1, BT=0
    cdb10[2] = (blockAddress >> 24) & 0xFF;
    cdb10[3] = (blockAddress >> 16) & 0xFF;
    cdb10[4] = (blockAddress >> 8) & 0xFF;
    cdb10[5] = blockAddress & 0xFF;
    cdb10[8] = partition; 
    
    return sendScsiCommand(m_currentDevicePath, cdb10, ScsiDirection::None, data, 60000);
}

bool DeviceManager::locateHandle(int handleId, uint64_t blockAddress, uint32_t partition)
{
    std::vector<uint8_t> cdb(16, 0);
    cdb[0] = 0x92;
    cdb[1] = 0x02;
    cdb[2] = partition;
    cdb[4] = (blockAddress >> 56) & 0xFF;
    cdb[5] = (blockAddress >> 48) & 0xFF;
    cdb[6] = (blockAddress >> 40) & 0xFF;
    cdb[7] = (blockAddress >> 32) & 0xFF;
    cdb[8] = (blockAddress >> 24) & 0xFF;
    cdb[9] = (blockAddress >> 16) & 0xFF;
    cdb[10] = (blockAddress >> 8) & 0xFF;
    cdb[11] = blockAddress & 0xFF;

    std::vector<uint8_t> data;
    if (sendScsiCommandHandle(handleId, cdb, ScsiDirection::None, data, 60000)) {
        return true;
    }
    if (blockAddress > 0xFFFFFFFF) return false;

    std::vector<uint8_t> cdb10(10, 0);
    cdb10[0] = 0x2B;
    cdb10[1] = 0x02;
    cdb10[2] = (blockAddress >> 24) & 0xFF;
    cdb10[3] = (blockAddress >> 16) & 0xFF;
    cdb10[4] = (blockAddress >> 8) & 0xFF;
    cdb10[5] = blockAddress & 0xFF;
    cdb10[8] = partition;
    return sendScsiCommandHandle(handleId, cdb10, ScsiDirection::None, data, 60000);
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

bool DeviceManager::sendScsiCommandHandle(int handleId,
                                    const std::vector<uint8_t> &cdb,
                                    ScsiDirection direction,
                                    std::vector<uint8_t> &data,
                                    unsigned int timeout)
{
    HandleEntry* h = getHandle(handleId);
    if (!h) return false;

#ifdef Q_OS_WIN
    return sendScsiCommandWindows(h->path, cdb, direction, data, timeout, h->nativeHandle);
#elif defined(Q_OS_LINUX)
    return sendScsiCommandLinux(h->path, cdb, direction, data, timeout, h->nativeHandle);
#elif defined(Q_OS_MAC)
    return sendScsiCommandMac(h->path, cdb, direction, data, timeout, h->nativeHandle);
#else
    Q_UNUSED(cdb);
    Q_UNUSED(direction);
    Q_UNUSED(data);
    Q_UNUSED(timeout);
    return false;
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

bool DeviceManager::loadDevice(const QString &devicePath)
{
    std::vector<uint8_t> cdb(6, 0);
    cdb[0] = SCSIOP_START_STOP_UNIT;
    cdb[4] = 0x03; // LoEj = 1, Start = 1 (Load)
    
    std::vector<uint8_t> data;
    
    return sendScsiCommand(devicePath, cdb, ScsiDirection::None, data, 300000); // Load can take time
}

bool DeviceManager::setMediaRemovalPrevention(const QString &devicePath, bool prevent)
{
    std::vector<uint8_t> cdb(6, 0);
    cdb[0] = 0x1E; // PREVENT ALLOW MEDIUM REMOVAL
    cdb[4] = prevent ? 0x01 : 0x00;
    
    std::vector<uint8_t> data;
    return sendScsiCommand(devicePath, cdb, ScsiDirection::None, data, 5000);
}

QByteArray DeviceManager::getMAMAttribute(const QString &devicePath, uint16_t attributeId)
{
    std::vector<uint8_t> cdb(16, 0);
    cdb[0] = 0x8C; // READ ATTRIBUTE
    cdb[1] = 0x00; // Service Action: Attribute Values
    cdb[8] = (attributeId >> 8) & 0xFF;
    cdb[9] = attributeId & 0xFF;
    
    uint32_t allocLen = 1024; // Start with 1KB
    cdb[10] = (allocLen >> 24) & 0xFF;
    cdb[11] = (allocLen >> 16) & 0xFF;
    cdb[12] = (allocLen >> 8) & 0xFF;
    cdb[13] = allocLen & 0xFF;
    
    std::vector<uint8_t> data(allocLen, 0);
    
    if (!sendScsiCommand(devicePath, cdb, ScsiDirection::In, data, 5000)) {
        return QByteArray();
    }
    
    if (data.size() < 4) return QByteArray();
    
    uint32_t dataLength = (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
    if (dataLength == 0) return QByteArray();
    
    // The data follows at offset 4.
    // Attribute Header: ID(2) + Format(1) + Length(2) = 5 bytes.
    if (data.size() < 4 + 5) return QByteArray();
    
    uint16_t id = (data[4] << 8) | data[5];
    if (id != attributeId) return QByteArray(); 
    
    uint16_t attrLen = (data[7] << 8) | data[8];
    
    if (data.size() < 4 + 5 + attrLen) return QByteArray();
    
    return QByteArray((const char*)&data[9], attrLen);
}

bool DeviceManager::setMAMAttribute(const QString &devicePath, uint16_t attributeId, const QByteArray &value)
{
    // WRITE ATTRIBUTE (8Dh)
    std::vector<uint8_t> cdb(16, 0);
    cdb[0] = 0x8D;
    cdb[1] = 0x00; // Write Attribute Values
    
    // Parameter List
    // Header (4 bytes) + Attribute (5 bytes + Value Length)
    uint32_t paramLen = 4 + 5 + value.size();
    
    cdb[10] = (paramLen >> 24) & 0xFF;
    cdb[11] = (paramLen >> 16) & 0xFF;
    cdb[12] = (paramLen >> 8) & 0xFF;
    cdb[13] = paramLen & 0xFF;
    
    std::vector<uint8_t> data(paramLen, 0);
    
    // Header: Data Length (Total length of attributes)
    uint32_t attrListLen = 5 + value.size();
    data[0] = (attrListLen >> 24) & 0xFF;
    data[1] = (attrListLen >> 16) & 0xFF;
    data[2] = (attrListLen >> 8) & 0xFF;
    data[3] = attrListLen & 0xFF;
    
    // Attribute
    data[4] = (attributeId >> 8) & 0xFF;
    data[5] = attributeId & 0xFF;
    data[6] = 0x00; // Format: Binary (00h) - or 01h (ASCII) depending on attribute?
                    // For generic raw write, Binary is safest. 
                    // LTFSCopyGUI allows specifying format. 
                    // We'll assume Binary for now or let caller encode.
    
    uint16_t valLen = value.size();
    data[7] = (valLen >> 8) & 0xFF;
    data[8] = valLen & 0xFF;
    
    memcpy(&data[9], value.constData(), valLen);
    
    std::vector<uint8_t> response;
    return sendScsiCommand(devicePath, cdb, ScsiDirection::Out, data, 5000);
}

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
    
    for (const QString &entry : std::as_const(entries)) {
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
    
    for (const QString &dataType : std::as_const(dataTypes)) {
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
        
        // Note: Full plist parsing of `ioreg -a -r -c SCSITaskDevice` is required to reliably extract BSD paths.
        // For this version, we rely on manual configuration or standard paths if auto-detection is limited.
    }
#endif
    return devices;
}

bool DeviceManager::sendScsiCommandWindows(const QString &devicePath, const std::vector<uint8_t> &cdb, ScsiDirection direction, std::vector<uint8_t> &data, unsigned int timeout, void* overrideHandle)
{
#ifdef Q_OS_WIN
    HANDLE hDevice = overrideHandle ? static_cast<HANDLE>(overrideHandle) : INVALID_HANDLE_VALUE;
    bool createdHandle = false;
    if (hDevice == INVALID_HANDLE_VALUE) {
        hDevice = CreateFile(reinterpret_cast<LPCWSTR>(devicePath.utf16()), 
                              GENERIC_READ | GENERIC_WRITE, 
                              FILE_SHARE_READ | FILE_SHARE_WRITE, 
                              NULL, 
                              OPEN_EXISTING, 
                              0, 
                              NULL);
        createdHandle = true;
    }

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
        if (createdHandle) CloseHandle(hDevice);
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

    if (createdHandle) CloseHandle(hDevice);
    return result != 0;
#else
    return false;
#endif
}

bool DeviceManager::sendScsiCommandLinux(const QString &devicePath, const std::vector<uint8_t> &cdb, ScsiDirection direction, std::vector<uint8_t> &data, unsigned int timeout, void* overrideHandle)
{
#ifdef Q_OS_LINUX
    int fd = overrideHandle ? static_cast<int>(reinterpret_cast<intptr_t>(overrideHandle)) : -1;
    bool createdHandle = false;
    if (fd < 0) {
        fd = open(devicePath.toLatin1().constData(), O_RDWR);
        if (fd < 0) return false;
        createdHandle = true;
    }

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
    if (createdHandle) close(fd);

    return result == 0;
#else
    return false;
#endif
}

bool DeviceManager::sendScsiCommandMac(const QString &devicePath, const std::vector<uint8_t> &cdb, ScsiDirection direction, std::vector<uint8_t> &data, unsigned int timeout, void* overrideHandle)
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
    
    Q_UNUSED(overrideHandle);
    return false; // Placeholder until full IOKit integration
#else
    return false;
#endif
}

VHFLogData DeviceManager::getVHFLogPage(const QString &devicePath)
{
    VHFLogData vhf;
    vhf.isValid = false;

    // LOG SENSE (0x4D)
    // Page Code: 0x11 (DT Device Status)
    // PC: 1 (Current Cumulative) -> Byte 2 = (0x01 << 6) | 0x11 = 0x40 | 0x11 = 0x51
    
    std::vector<uint8_t> cdb(10);
    cdb[0] = 0x4D; // LOG SENSE
    cdb[1] = 0x00;
    cdb[2] = 0x51; // PC=1, Page Code=0x11
    cdb[3] = 0x00; // Subpage Code
    cdb[7] = 0x00; // Allocation Length (MSB)
    cdb[8] = 0xFF; // Allocation Length (LSB) - 255 bytes should be enough
    
    std::vector<uint8_t> data(255);
    
    // Use sendScsiCommand which handles platform specifics
    if (sendScsiCommand(devicePath, cdb, ScsiDirection::In, data)) {
        // Parse Log Page
        // Header: 4 bytes
        // Page Code (Byte 0) should be 0x11 (masked with 0x3F)
        if (data.size() < 4 || (data[0] & 0x3F) != 0x11) {
            return vhf;
        }
        
        uint16_t pageLength = (data[2] << 8) | data[3];
        // Ensure we have enough data
        if (data.size() < 4 + pageLength) {
            // Incomplete data, but we might have enough for what we need
        }
        
        // Iterate parameters
        size_t offset = 4;
        while (offset + 4 <= data.size()) {
            uint16_t paramCode = (data[offset] << 8) | data[offset+1];
            uint8_t paramLen = data[offset+3];
            
            if (offset + 4 + paramLen > data.size()) break;
            
            if (paramCode == 0x0000) { // Very High Frequency Data
                // Byte 0:
                // Bit 4: Write Protect
                // Bit 5: Clean Requested
                // Bit 6: Cleaning Required
                // Byte 1:
                // Bit 0: In Transition
                // Bit 3: Media Present
                // Bit 6: Media Threaded
                // Bit 7: Data Accessible
                // Byte 2: Device Activity
                // Byte 3: Bit 3 Encryption Parameters Present
                if (paramLen >= 4) {
                    uint8_t byte0 = data[offset + 4];
                    uint8_t byte1 = data[offset + 5];
                    uint8_t byte2 = data[offset + 6];
                    uint8_t byte3 = data[offset + 7];
                    
                    vhf.writeProtect = (byte0 >> 4) & 1;
                    vhf.cleanRequested = (byte0 >> 5) & 1;
                    vhf.cleaningRequired = (byte0 >> 6) & 1;
                    
                    vhf.inTransition = (byte1 >> 0) & 1;
                    vhf.mediaPresent = (byte1 >> 3) & 1;
                    vhf.mediaThreaded = (byte1 >> 6) & 1;
                    vhf.dataAccessible = (byte1 >> 7) & 1;
                    
                    vhf.deviceActivity = byte2; 
                    vhf.encryptionEnabled = (byte3 >> 3) & 1;
                    
                    vhf.isValid = true;
                }
            }
            
            offset += 4 + paramLen;
        }
    }
    
    return vhf;
}

DriveLedStatus DeviceManager::getDriveLedStatus(const QString &devicePath)
{
    DriveLedStatus status;
    
    // 1. Get Encryption Status from VHF Page (0x11)
    VHFLogData vhf = getVHFLogPage(devicePath);
    if (vhf.isValid) {
        status.encryption = vhf.encryptionEnabled;
    }
    
    // 2. Get Device Status Page (0x3E)
    // LOG SENSE (0x4D)
    // Page Code: 0x3E
    // PC: 1 (Current Cumulative) -> Byte 2 = (0x01 << 6) | 0x3E = 0x40 | 0x3E = 0x7E
    
    std::vector<uint8_t> cdb(10);
    cdb[0] = 0x4D; // LOG SENSE
    cdb[1] = 0x00;
    cdb[2] = 0x7E; // PC=1, Page Code=0x3E
    cdb[3] = 0x00; // Subpage Code
    cdb[7] = 0x00; // Allocation Length (MSB)
    cdb[8] = 0xFF; // Allocation Length (LSB)
    
    std::vector<uint8_t> data(255);
    
    if (sendScsiCommand(devicePath, cdb, ScsiDirection::In, data)) {
        if (data.size() >= 4 && (data[0] & 0x3F) == 0x3E) {
             size_t offset = 4;
             while (offset + 4 <= data.size()) {
                uint16_t paramCode = (data[offset] << 8) | data[offset+1];
                uint8_t paramLen = data[offset+3];
                
                if (offset + 4 + paramLen > data.size()) break;
                
                if (paramCode == 0x0001) { // Device Status Bits
                    // Byte 0: Cleaning flags (bits 5/6)
                    // Byte 1: Device Status (bits 6-7)
                    // Byte 2: Medium Status (bits 6-7)
                    if (paramLen >= 3) {
                        uint8_t byte0 = data[offset + 4];
                        uint8_t byte1 = data[offset + 5];
                        uint8_t byte2 = data[offset + 6];
                        
                        bool cleaningRequired = (byte0 & 0x60) != 0; // either required or requested
                        uint8_t deviceStatus = (byte1 & 0xC0) >> 6;
                        uint8_t mediumStatus = (byte2 & 0xC0) >> 6;
                        
                        status.clean = cleaningRequired;
                        status.driveError = (deviceStatus > 1);
                        status.tapeError = (mediumStatus > 1);
                    }
                }
                offset += 4 + paramLen;
             }
        }
    }
    
    return status;
}


uint64_t DeviceManager::readTapeAlerts(const QString &devicePath)
{
    uint64_t alerts = 0;

    // LOG SENSE (0x4D)
    // Page Code: 0x2E (TapeAlert)
    // PC: 1 (Current Cumulative) -> Byte 2 = (0x01 << 6) | 0x2E = 0x40 | 0x2E = 0x6E
    
    std::vector<uint8_t> cdb(10);
    cdb[0] = 0x4D; // LOG SENSE
    cdb[1] = 0x00;
    cdb[2] = 0x6E; // PC=1, Page Code=0x2E
    cdb[3] = 0x00; // Subpage Code
    cdb[7] = 0x02; // Allocation Length (MSB) - 512 bytes
    cdb[8] = 0x00; // Allocation Length (LSB)
    
    std::vector<uint8_t> data(512);
    
    if (sendScsiCommand(devicePath, cdb, ScsiDirection::In, data)) {
        // Parse Log Page
        if (data.size() < 4 || (data[0] & 0x3F) != 0x2E) {
            return 0;
        }
        
        // Iterate parameters
        size_t offset = 4;
        while (offset + 4 <= data.size()) {
            uint16_t paramCode = (data[offset] << 8) | data[offset+1];
            uint8_t paramLen = data[offset+3];
            
            if (offset + 4 + paramLen > data.size()) break;
            
            // TapeAlert flags are 1-64.
            if (paramCode >= 1 && paramCode <= 64) {
                // Value is usually 1 byte.
                if (paramLen >= 1) {
                    uint8_t value = data[offset + 4];
                    if (value != 0) {
                        alerts |= (1ULL << (paramCode - 1));
                    }
                }
            }
            
            offset += 4 + paramLen;
        }
    }
    
    return alerts;
}

