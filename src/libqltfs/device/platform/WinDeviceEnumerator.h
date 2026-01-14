/**
 * QLTOTapeMan - Qt-based LTO Tape Manager
 * Windows Device Enumerator Header
 *
 * Copyright (c) 2026 Jeffrey ZHU (zhxsh1225@gmail.com)
 * https://github.com/Gypsop/QLTOTapeMan
 */

#ifndef QLTFS_WINDEVICEENUMERATOR_H
#define QLTFS_WINDEVICEENUMERATOR_H

#include "../../libqltfs_global.h"

#ifdef Q_OS_WIN

#include <QString>
#include <QList>
#include <optional>
#include <windows.h>
#include <setupapi.h>
#include <cfgmgr32.h>
#include <devguid.h>
#include <initguid.h>
#include <ntddscsi.h>

namespace qltfs {

/**
 * @brief Device type enumeration
 */
enum class WinDeviceType {
    Unknown,
    TapeDrive,
    TapeChanger
};

/**
 * @brief Device status enumeration
 */
enum class WinDeviceStatus {
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
struct LIBQLTFS_EXPORT WinDeviceInfo {
    QString devicePath;
    QString vendor;
    QString model;
    QString revision;
    QString serialNumber;
    WinDeviceType type = WinDeviceType::Unknown;
    WinDeviceStatus status = WinDeviceStatus::Unknown;
    int deviceIndex = -1;
    bool hasMedium = false;
    uint64_t capacity = 0;
    uint64_t freeSpace = 0;
};

/**
 * @brief Windows-specific device enumeration implementation
 *
 * Uses SetupAPI and CM API to enumerate tape devices.
 * This is a standalone implementation that can be used
 * by the main DeviceEnumerator class or independently.
 */
class LIBQLTFS_EXPORT WinDeviceEnumerator
{
public:
    WinDeviceEnumerator();
    ~WinDeviceEnumerator();

    /**
     * @brief Enumerate all tape devices
     * @return List of discovered tape devices
     */
    QList<WinDeviceInfo> enumerateDevices();

    /**
     * @brief Enumerate tape devices of specific type
     * @param type Device type to enumerate
     * @return List of discovered devices of specified type
     */
    QList<WinDeviceInfo> enumerateDevices(WinDeviceType type);

    /**
     * @brief Refresh the device list
     */
    void refresh();

    /**
     * @brief Get device by path
     * @param devicePath Windows device path
     * @return WinDeviceInfo or empty optional if not found
     */
    std::optional<WinDeviceInfo> getDeviceByPath(const QString& devicePath);

    /**
     * @brief Get device by serial number
     * @param serialNumber Device serial number
     * @return WinDeviceInfo or empty optional if not found
     */
    std::optional<WinDeviceInfo> getDeviceBySerial(const QString& serialNumber);

    /**
     * @brief Check if device is ready
     * @param devicePath Windows device path
     * @return true if device is ready for use
     */
    bool isDeviceReady(const QString& devicePath);

private:
    /**
     * @brief Enumerate tape drives using \\.\Tape0, Tape1, etc.
     */
    void enumerateTapeDrives();

    /**
     * @brief Enumerate tape changers (medium changers)
     */
    void enumerateTapeChangers();

    /**
     * @brief Get device information via SCSI inquiry
     * @param devicePath Windows device path
     * @return WinDeviceInfo with filled details
     */
    WinDeviceInfo getDeviceInfoByInquiry(const QString& devicePath);

    /**
     * @brief Get device description from SetupAPI
     * @param devicePath Device interface path
     * @return Human-readable device description
     */
    QString getDeviceDescription(const QString& devicePath);

    /**
     * @brief Get device serial number
     * @param devicePath Windows device path
     * @return Serial number string
     */
    QString getSerialNumber(const QString& devicePath);

    /**
     * @brief Parse SCSI Inquiry data
     * @param inquiryData Raw inquiry response
     * @param info WinDeviceInfo to fill
     */
    void parseInquiryData(const uint8_t* inquiryData, size_t length, WinDeviceInfo& info);

    /**
     * @brief Get tape drive capacity and status
     * @param devicePath Windows device path
     * @param info WinDeviceInfo to update
     */
    void getTapeDriveStatus(const QString& devicePath, WinDeviceInfo& info);

    QList<WinDeviceInfo> m_cachedDevices;  ///< Cached device list
    bool m_cacheValid;                      ///< Cache validity flag
};

} // namespace qltfs

#endif // Q_OS_WIN

#endif // QLTFS_WINDEVICEENUMERATOR_H
