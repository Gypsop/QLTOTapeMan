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

    bool isDeviceReady(const QString &devicePath);
    bool rewindDevice(const QString &devicePath);
    bool unloadDevice(const QString &devicePath);
    
    // Block I/O Operations (Requires openDevice)
    bool writeScsiBlock(const QByteArray &data);
    QByteArray readScsiBlock(uint32_t length);
    bool writeFileMark(uint8_t count = 1);
    bool space(int32_t count, uint8_t code); // code: 0=Blocks, 1=FileMarks
    bool locate(uint32_t blockAddress);

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
