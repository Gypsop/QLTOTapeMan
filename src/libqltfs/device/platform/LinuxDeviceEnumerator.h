/**
 * QLTOTapeMan - Qt-based LTO Tape Manager
 * Linux Device Enumerator Header
 *
 * Copyright (c) 2026 Jeffrey ZHU (zhxsh1225@gmail.com)
 * https://github.com/Gypsop/QLTOTapeMan
 */

#ifndef QLTFS_LINUXDEVICEENUMERATOR_H
#define QLTFS_LINUXDEVICEENUMERATOR_H

#include "../../libqltfs_global.h"

#if defined(Q_OS_LINUX)

#include <QString>
#include <QList>
#include <optional>

namespace qltfs {

/**
 * @brief Device type enumeration
 */
enum class LinuxDeviceType {
    Unknown,
    TapeDrive,
    TapeChanger
};

/**
 * @brief Device status enumeration
 */
enum class LinuxDeviceStatus {
    Unknown,
    Ready,
    NotReady,
    Empty,
    Busy,
    Error
};

/**
 * @brief Device information structure
 */
struct LIBQLTFS_EXPORT LinuxDeviceInfo {
    QString devicePath;
    QString vendor;
    QString model;
    QString revision;
    QString serialNumber;
    LinuxDeviceType type = LinuxDeviceType::Unknown;
    LinuxDeviceStatus status = LinuxDeviceStatus::Unknown;
    int deviceIndex = -1;
    bool hasMedium = false;
    uint64_t capacity = 0;
    uint64_t freeSpace = 0;
};

/**
 * @brief Linux-specific device enumeration implementation
 *
 * Uses /sys filesystem and SCSI generic (sg) layer to enumerate tape devices.
 * This is a standalone implementation that can be used
 * by the main DeviceEnumerator class or independently.
 */
class LIBQLTFS_EXPORT LinuxDeviceEnumerator
{
public:
    LinuxDeviceEnumerator();
    ~LinuxDeviceEnumerator();

    /**
     * @brief Enumerate all tape devices
     * @return List of discovered tape devices
     */
    QList<LinuxDeviceInfo> enumerateDevices();

    /**
     * @brief Enumerate tape devices of specific type
     * @param type Device type to enumerate
     * @return List of discovered devices of specified type
     */
    QList<LinuxDeviceInfo> enumerateDevices(LinuxDeviceType type);

    /**
     * @brief Refresh the device list
     */
    void refresh();

    /**
     * @brief Get device by path
     * @param devicePath Linux device path
     * @return LinuxDeviceInfo or empty optional if not found
     */
    std::optional<LinuxDeviceInfo> getDeviceByPath(const QString& devicePath);

    /**
     * @brief Get device by serial number
     * @param serialNumber Device serial number
     * @return LinuxDeviceInfo or empty optional if not found
     */
    std::optional<LinuxDeviceInfo> getDeviceBySerial(const QString& serialNumber);

    /**
     * @brief Check if device is ready
     * @param devicePath Linux device path
     * @return true if device is ready for use
     */
    bool isDeviceReady(const QString& devicePath);

private:
    /**
     * @brief Enumerate tape drives via /dev/nst* and /dev/st*
     */
    void enumerateTapeDrives();

    /**
     * @brief Enumerate tape changers via /dev/sch*
     */
    void enumerateTapeChangers();

    /**
     * @brief Enumerate devices using /sys/class/scsi_tape
     */
    void enumerateViaSysfs();

    /**
     * @brief Enumerate devices using /sys/class/scsi_changer
     */
    void enumerateChangersViaSysfs();

    /**
     * @brief Get device information via SCSI inquiry
     * @param devicePath Linux device path
     * @return LinuxDeviceInfo with filled details
     */
    LinuxDeviceInfo getDeviceInfoByInquiry(const QString& devicePath);

    /**
     * @brief Get the SG device path for a tape device
     * @param tapeDevicePath Path like /dev/nst0
     * @return Corresponding /dev/sg* path or empty string
     */
    QString getSgDevicePath(const QString& tapeDevicePath);

    /**
     * @brief Parse SCSI Inquiry data
     * @param inquiryData Raw inquiry response
     * @param info LinuxDeviceInfo to fill
     */
    void parseInquiryData(const uint8_t* inquiryData, size_t length, LinuxDeviceInfo& info);

    /**
     * @brief Get serial number via VPD page 80h
     * @param devicePath Device path
     * @return Serial number string
     */
    QString getSerialNumber(const QString& devicePath);

    /**
     * @brief Read sysfs attribute
     * @param path Path to sysfs attribute file
     * @return Attribute value or empty string
     */
    QString readSysfsAttribute(const QString& path);

    /**
     * @brief Get tape drive status
     * @param devicePath Device path
     * @param info LinuxDeviceInfo to update
     */
    void getTapeDriveStatus(const QString& devicePath, LinuxDeviceInfo& info);

    /**
     * @brief Find the sysfs device path for a tape device
     * @param devicePath Device path like /dev/nst0
     * @return Sysfs path like /sys/class/scsi_tape/nst0
     */
    QString findSysfsPath(const QString& devicePath);

    QList<LinuxDeviceInfo> m_cachedDevices;  ///< Cached device list
    bool m_cacheValid;                        ///< Cache validity flag
};

} // namespace qltfs

#endif // Q_OS_LINUX

#endif // QLTFS_LINUXDEVICEENUMERATOR_H
