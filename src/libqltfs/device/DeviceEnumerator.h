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

#pragma once

#include "libqltfs_global.h"

#include <QString>
#include <QStringList>
#include <QList>

namespace qltfs {

/**
 * @brief Device type enumeration
 */
enum class DeviceType {
    Unknown,
    TapeDrive,          ///< Sequential access tape drive
    MediumChanger,      ///< Tape library / autoloader
    Printer,            ///< Printer device (rare)
    Processor,          ///< Processor device
    Worm,               ///< Write-once read-many
    CdDvd,              ///< CD/DVD/Blu-ray
    Scanner,            ///< Scanner
    OpticalMemory,      ///< Optical memory device
    DiskDrive,          ///< Direct access disk
    Communication,      ///< Communication device
    ArrayController,    ///< Storage array controller
    EnclosureServices,  ///< SES device
    SimplifiedDisk,     ///< Simplified direct access
    OpticalCardReader,  ///< Optical card reader/writer
    BridgeController,   ///< Bridge controller
    ObjectBased,        ///< Object-based storage
    AutomationDrive,    ///< Automation/Drive interface
    SecurityManager,    ///< Security manager
    ZonedBlock,         ///< Zoned block commands
    WellKnownLU         ///< Well known logical unit
};

/**
 * @brief Information about a detected tape device
 */
struct LIBQLTFS_EXPORT TapeDeviceInfo {
    QString devicePath;         ///< System device path (e.g., \\.\Tape0, /dev/st0)
    QString genericPath;        ///< Generic SCSI path (Linux: /dev/sg*, Windows: same as devicePath)
    QString vendor;             ///< Device vendor string
    QString product;            ///< Device product/model string
    QString revision;           ///< Firmware revision
    QString serialNumber;       ///< Device serial number
    DeviceType type = DeviceType::Unknown;
    bool isReady = false;       ///< Device has media and is ready

    /**
     * @brief Get display name for this device
     */
    QString displayName() const;

    /**
     * @brief Check if device info is valid
     */
    bool isValid() const;
};

/**
 * @brief Information about a medium changer (tape library)
 */
struct LIBQLTFS_EXPORT ChangerDeviceInfo {
    QString devicePath;         ///< System device path
    QString vendor;             ///< Device vendor string
    QString product;            ///< Device product/model string
    QString revision;           ///< Firmware revision
    QString serialNumber;       ///< Device serial number
    int dataTransferElements = 0;   ///< Number of drives
    int storageElements = 0;        ///< Number of storage slots
    int importExportElements = 0;   ///< Number of I/E slots
    int mediumTransportElements = 0; ///< Number of robot arms

    /**
     * @brief Get display name for this device
     */
    QString displayName() const;

    /**
     * @brief Check if device info is valid
     */
    bool isValid() const;
};

/**
 * @brief Enumerates tape drives and medium changers on the system
 *
 * Platform-independent interface for discovering tape devices.
 * Uses platform-specific APIs:
 * - Windows: SetupAPI device enumeration
 * - Linux: /sys/class/scsi_tape, /sys/class/scsi_changer enumeration
 */
class LIBQLTFS_EXPORT DeviceEnumerator
{
public:
    DeviceEnumerator();
    ~DeviceEnumerator();

    // Disable copy
    DeviceEnumerator(const DeviceEnumerator &) = delete;
    DeviceEnumerator &operator=(const DeviceEnumerator &) = delete;

    /**
     * @brief Refresh device list
     *
     * Scans the system for tape drives and medium changers.
     * This operation may take a few seconds.
     *
     * @return true if enumeration succeeded
     */
    bool refresh();

    /**
     * @brief Get list of detected tape drives
     */
    QList<TapeDeviceInfo> tapeDevices() const;

    /**
     * @brief Get list of detected medium changers
     */
    QList<ChangerDeviceInfo> changerDevices() const;

    /**
     * @brief Find tape device by path
     * @param path Device path to search for
     * @return Device info, or invalid info if not found
     */
    TapeDeviceInfo findTapeDevice(const QString &path) const;

    /**
     * @brief Find changer device by path
     * @param path Device path to search for
     * @return Device info, or invalid info if not found
     */
    ChangerDeviceInfo findChangerDevice(const QString &path) const;

    /**
     * @brief Get all device paths as strings
     *
     * Convenience method for populating combo boxes.
     */
    QStringList tapeDevicePaths() const;

    /**
     * @brief Get all changer device paths as strings
     */
    QStringList changerDevicePaths() const;

    /**
     * @brief Get last error message
     */
    QString lastError() const;

    // === Static helper methods ===

    /**
     * @brief Convert device type code to DeviceType enum
     * @param peripheralDeviceType SCSI peripheral device type byte
     */
    static DeviceType scsiDeviceType(quint8 peripheralDeviceType);

    /**
     * @brief Get device type name string
     */
    static QString deviceTypeName(DeviceType type);

    /**
     * @brief Check if path is a valid tape device path format
     */
    static bool isValidTapeDevicePath(const QString &path);

    /**
     * @brief Get default tape device path for the platform
     *
     * Returns the first typical tape device path:
     * - Windows: "\\\\.\\Tape0"
     * - Linux: "/dev/st0"
     */
    static QString defaultTapeDevicePath();

private:
    class Private;
    Private *d;
};

} // namespace qltfs
