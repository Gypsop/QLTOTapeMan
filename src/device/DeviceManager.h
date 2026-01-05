#ifndef DEVICEMANAGER_H
#define DEVICEMANAGER_H

#include <QObject>
#include <QList>
#include <QString>
#include <QVariantMap>
#include <vector>
#include "ScsiUtils.h"

struct TapeDeviceInfo {
    QString devicePath;
    QString vendorId;
    QString productId;
    QString serialNumber;
    QString productRevision;
    QString driveType; // LTO-5, LTO-6, etc.
    
    // Helper to display friendly name
    QString friendlyName() const {
        return QString("%1 %2 (%3)").arg(vendorId, productId, serialNumber);
    }
};

struct TapeStatus {
    bool isReady = false;
    bool isLoaded = false;
    bool isWriteProtected = false;
    bool needsCleaning = false;
    
    // Position
    uint32_t currentPartition = 0;
    uint64_t currentBlock = 0;
    
    uint64_t capacityBytes = 0;
    uint64_t remainingBytes = 0;
    uint32_t blockSize = 0;
    uint32_t partitionCount = 0;
    
    // Drive Capabilities
    bool compressionEnabled = false;
    uint32_t maxBlockSize = 0;
    
    QString statusMessage; // Human readable status
};

struct VHFLogData {
    bool cleanRequested = false;
    bool cleaningRequired = false;
    bool mediaPresent = false;
    bool mediaThreaded = false;
    bool dataAccessible = false;
    bool writeProtect = false;
    bool inTransition = false;
    uint8_t deviceActivity = 0; // 0=No activity, 1=Cleaning, 2=Loading, 3=Unloading, 4=Reading, 5=Writing, etc.
    bool isValid = false;
};

class DeviceManager : public QObject
{
    Q_OBJECT
public:
    explicit DeviceManager(QObject *parent = nullptr);
    
    // Scan for connected tape devices
    QList<TapeDeviceInfo> scanDevices();

    // Send a raw SCSI command
    bool sendScsiCommand(const QString &devicePath,
                         const std::vector<uint8_t> &cdb,
                         ScsiDirection direction,
                         std::vector<uint8_t> &data,
                         unsigned int timeout = 5000);

    // Tape Operations
    bool openDevice(const QString &devicePath);
    void closeDevice();
    bool isDeviceOpen() const;

    TapeStatus getDeviceStatus(const QString &devicePath); // High-level status check
    VHFLogData getVHFLogPage(const QString &devicePath); // Get Very High Frequency Log Page (0x11)
    uint64_t readTapeAlerts(const QString &devicePath); // Get TapeAlert Log Page (0x2E)

    bool isDeviceReady(const QString &devicePath);
    bool rewindDevice(const QString &devicePath);
    bool unloadDevice(const QString &devicePath);
    bool loadDevice(const QString &devicePath);
    bool setMediaRemovalPrevention(const QString &devicePath, bool prevent);
    
    // Block I/O Operations (Requires openDevice)
    bool setBlockSize(uint32_t blockSize); // 0 = Variable Block Mode
    
    struct ScsiWriteResult {
        bool isEOM = false; // Early Warning EOM
        bool isError = false;
        QString errorMessage;
    };
    ScsiWriteResult writeScsiBlock(const QByteArray &data);
    bool synchronizeCache();
    
    struct ScsiReadResult {
        QByteArray data;
        bool isFileMark = false;
        bool isEOM = false; // End of Medium (Physical End)
        bool isEOD = false; // End of Data (Blank Check)
        bool isError = false;
        QString errorMessage;
    };
    ScsiReadResult readScsiBlock(uint32_t length);
    
    struct BlockLimits {
        uint32_t maxBlockLength = 0;
        uint16_t minBlockLength = 0;
        bool valid = false;
    };
    BlockLimits readBlockLimits();

    struct TapePosition {
        uint32_t partition = 0;
        uint64_t blockNumber = 0;
        bool bop = false; // Beginning of Partition
        bool eop = false; // End of Partition
        bool valid = false;
    };
    TapePosition readPosition();

    bool writeFileMark(uint8_t count = 1);
    bool writeSetMark(uint8_t count = 1);
    bool eraseTape(bool longErase = false);
    bool createPartition(uint8_t method, uint16_t sizeMB); // method: 0=IDP, 1=Wrap, 2=Size
    bool space(int32_t count, uint8_t code); // code: 0=Blocks, 1=FileMarks, 3=End of Data, 4=Setmarks
    bool locate(uint64_t blockAddress, uint32_t partition = 0);
    
    // MAM & Logs
    QByteArray getMAMAttribute(const QString &devicePath, uint16_t attributeId);
    bool setMAMAttribute(const QString &devicePath, uint16_t attributeId, const QByteArray &value);

signals:
    void deviceListChanged(const QList<TapeDeviceInfo> &devices);

private:
    void* m_deviceHandle = nullptr; // Platform specific handle
    QString m_currentDevicePath;

    // OS-specific implementations
    QList<TapeDeviceInfo> scanDevicesWindows();
    QList<TapeDeviceInfo> scanDevicesLinux();
    QList<TapeDeviceInfo> scanDevicesMac();

    // OS-specific SCSI implementations
    bool sendScsiCommandWindows(const QString &devicePath, const std::vector<uint8_t> &cdb, ScsiDirection direction, std::vector<uint8_t> &data, unsigned int timeout);
    bool sendScsiCommandLinux(const QString &devicePath, const std::vector<uint8_t> &cdb, ScsiDirection direction, std::vector<uint8_t> &data, unsigned int timeout);
    bool sendScsiCommandMac(const QString &devicePath, const std::vector<uint8_t> &cdb, ScsiDirection direction, std::vector<uint8_t> &data, unsigned int timeout);
};

#endif // DEVICEMANAGER_H
