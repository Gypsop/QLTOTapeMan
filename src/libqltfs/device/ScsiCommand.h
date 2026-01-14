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
#include "core/LtfsTypes.h"

#include <QByteArray>
#include <QString>

namespace qltfs {

/**
 * @brief SCSI command operation codes
 */
enum class ScsiOpCode : quint8 {
    TestUnitReady       = 0x00,
    Rewind              = 0x01,
    RequestSense        = 0x03,
    Read6               = 0x08,
    Write6              = 0x0A,
    WriteFilemark       = 0x10,
    Space               = 0x11,
    Inquiry             = 0x12,
    Verify6             = 0x13,
    ModeSelect6         = 0x15,
    Reserve             = 0x16,
    Release             = 0x17,
    Erase               = 0x19,
    ModeSense6          = 0x1A,
    LoadUnload          = 0x1B,
    ReadBlockLimits     = 0x05,
    PreventAllowMediumRemoval = 0x1E,
    ReadCapacity        = 0x25,
    Read10              = 0x28,
    Write10             = 0x2A,
    Verify10            = 0x2F,
    Locate10            = 0x2B,
    ReadPosition        = 0x34,
    ReportDensitySupport = 0x44,
    LogSelect           = 0x4C,
    LogSense            = 0x4D,
    ModeSelect10        = 0x55,
    ModeSense10         = 0x5A,
    ReportLuns          = 0xA0,
    Read16              = 0x88,
    Write16             = 0x8A,
    Verify16            = 0x8F,
    Locate16            = 0x92,
    ReadAttribute       = 0x8C,
    WriteAttribute      = 0x8D,
    AllowOverwrite      = 0x82,
    FormatMedium        = 0x04,
    ReadBuffer          = 0x3C,
    WriteBuffer         = 0x3B,
    SetCapacity         = 0x0B,
};

/**
 * @brief SCSI command direction
 */
enum class ScsiDataDirection {
    None,           ///< No data transfer
    ToDevice,       ///< Data transfer to device (write)
    FromDevice      ///< Data transfer from device (read)
};

/**
 * @brief SCSI sense key values
 */
enum class ScsiSenseKey : quint8 {
    NoSense         = 0x00,
    RecoveredError  = 0x01,
    NotReady        = 0x02,
    MediumError     = 0x03,
    HardwareError   = 0x04,
    IllegalRequest  = 0x05,
    UnitAttention   = 0x06,
    DataProtect     = 0x07,
    BlankCheck      = 0x08,
    VendorSpecific  = 0x09,
    CopyAborted     = 0x0A,
    AbortedCommand  = 0x0B,
    VolumeOverflow  = 0x0D,
    Miscompare      = 0x0E,
    Completed       = 0x0F
};

/**
 * @brief SCSI sense data structure
 */
struct LIBQLTFS_EXPORT ScsiSenseData {
    bool valid = false;
    ScsiSenseKey senseKey = ScsiSenseKey::NoSense;
    quint8 additionalSenseCode = 0;         ///< ASC
    quint8 additionalSenseCodeQualifier = 0; ///< ASCQ
    quint32 information = 0;
    QByteArray rawData;

    /**
     * @brief Check if this is an error condition
     */
    bool isError() const;

    /**
     * @brief Get human-readable sense description
     */
    QString toString() const;

    /**
     * @brief Parse sense data from raw bytes
     */
    static ScsiSenseData fromRawData(const QByteArray &data);
};

/**
 * @brief Result of SCSI command execution
 */
struct LIBQLTFS_EXPORT ScsiCommandResult {
    bool success = false;
    int hostStatus = 0;             ///< Host adapter status
    int driverStatus = 0;           ///< Driver status
    int scsiStatus = 0;             ///< SCSI status byte
    ScsiSenseData senseData;
    QByteArray data;                ///< Data transferred
    quint32 bytesTransferred = 0;   ///< Actual bytes transferred
    quint32 residual = 0;           ///< Residual count

    QString errorMessage() const;
};

/**
 * @brief SCSI command builder and executor
 *
 * Provides platform-independent interface for constructing and
 * executing SCSI commands. Platform-specific implementation is
 * selected at runtime.
 */
class LIBQLTFS_EXPORT ScsiCommand
{
public:
    /**
     * @brief Constructor
     * @param devicePath Path to the tape device
     */
    explicit ScsiCommand(const QString &devicePath);
    ~ScsiCommand();

    // Disable copy
    ScsiCommand(const ScsiCommand &) = delete;
    ScsiCommand &operator=(const ScsiCommand &) = delete;

    /**
     * @brief Open device for SCSI command execution
     * @return true if device opened successfully
     */
    bool open();

    /**
     * @brief Close device
     */
    void close();

    /**
     * @brief Check if device is open
     */
    bool isOpen() const;

    /**
     * @brief Get device path
     */
    QString devicePath() const;

    /**
     * @brief Set command timeout in seconds
     */
    void setTimeout(int seconds);

    /**
     * @brief Get current timeout
     */
    int timeout() const;

    // === SCSI Commands ===

    /**
     * @brief Test Unit Ready - Check if device is ready
     */
    ScsiCommandResult testUnitReady();

    /**
     * @brief Inquiry - Get device information
     * @param evpd Enable Vital Product Data page
     * @param pageCode VPD page code if evpd is true
     * @param allocationLength Buffer size
     */
    ScsiCommandResult inquiry(bool evpd = false,
                              quint8 pageCode = 0,
                              quint16 allocationLength = 96);

    /**
     * @brief Request Sense - Get sense data
     */
    ScsiCommandResult requestSense(quint8 allocationLength = 252);

    /**
     * @brief Read Block Limits - Get min/max block sizes
     */
    ScsiCommandResult readBlockLimits();

    /**
     * @brief Mode Sense (10) - Get mode pages
     * @param pageCode Mode page code
     * @param subPageCode Sub-page code
     * @param allocationLength Buffer size
     */
    ScsiCommandResult modeSense10(quint8 pageCode,
                                  quint8 subPageCode = 0,
                                  quint16 allocationLength = 4096);

    /**
     * @brief Mode Select (10) - Set mode pages
     * @param data Mode page data
     * @param savePages Save pages permanently
     */
    ScsiCommandResult modeSelect10(const QByteArray &data, bool savePages = false);

    /**
     * @brief Log Sense - Get log pages
     * @param pageCode Log page code
     * @param subPageCode Sub-page code
     * @param allocationLength Buffer size
     */
    ScsiCommandResult logSense(quint8 pageCode,
                               quint8 subPageCode = 0,
                               quint16 allocationLength = 16384);

    /**
     * @brief Read Position - Get current tape position
     * @param serviceAction Service action (0=short, 1=long, 6=extended)
     */
    ScsiCommandResult readPosition(quint8 serviceAction = 0);

    /**
     * @brief Locate (16) - Seek to position
     * @param partition Partition number (0 or 1)
     * @param blockNumber Block number within partition
     */
    ScsiCommandResult locate16(quint8 partition, quint64 blockNumber);

    /**
     * @brief Rewind - Rewind tape to beginning
     * @param immediate Return immediately without waiting
     */
    ScsiCommandResult rewind(bool immediate = false);

    /**
     * @brief Load/Unload - Load or eject tape
     * @param load true=load, false=unload/eject
     * @param immediate Return immediately
     */
    ScsiCommandResult loadUnload(bool load, bool immediate = false);

    /**
     * @brief Space - Move tape by filemarks or blocks
     * @param code Space code (0=blocks, 1=filemarks, 3=end of data)
     * @param count Number of items to space over (negative for reverse)
     */
    ScsiCommandResult space(quint8 code, qint32 count);

    /**
     * @brief Read (6) - Read data blocks
     * @param buffer Buffer for read data
     * @param blockCount Number of blocks to read
     * @param fixedBlock Fixed block mode
     */
    ScsiCommandResult read6(QByteArray &buffer,
                            quint32 blockCount,
                            bool fixedBlock = true);

    /**
     * @brief Write (6) - Write data blocks
     * @param data Data to write
     * @param fixedBlock Fixed block mode
     */
    ScsiCommandResult write6(const QByteArray &data, bool fixedBlock = true);

    /**
     * @brief Write Filemarks
     * @param count Number of filemarks to write
     * @param setMark Write setmarks instead of filemarks
     * @param immediate Return immediately
     */
    ScsiCommandResult writeFilemark(quint32 count = 1,
                                    bool setMark = false,
                                    bool immediate = false);

    /**
     * @brief Erase - Erase tape
     * @param longErase Long erase (full tape)
     * @param immediate Return immediately
     */
    ScsiCommandResult erase(bool longErase = false, bool immediate = true);

    /**
     * @brief Read Attribute - Read MAM attributes
     * @param serviceAction Service action
     * @param attributeId Attribute ID to read
     * @param allocationLength Buffer size
     */
    ScsiCommandResult readAttribute(quint8 serviceAction,
                                    quint16 attributeId,
                                    quint32 allocationLength = 16384);

    /**
     * @brief Write Attribute - Write MAM attributes
     * @param data Attribute data
     */
    ScsiCommandResult writeAttribute(const QByteArray &data);

    /**
     * @brief Allow Overwrite - Allow data overwrite at current position
     * @param mode Overwrite mode
     */
    ScsiCommandResult allowOverwrite(quint8 mode);

    /**
     * @brief Prevent/Allow Medium Removal
     * @param prevent true to prevent, false to allow
     */
    ScsiCommandResult preventAllowMediumRemoval(bool prevent);

    /**
     * @brief Format Medium - Format or partition tape
     * @param format Format type
     * @param partition Whether to partition
     */
    ScsiCommandResult formatMedium(quint8 format, bool partition = false);

    /**
     * @brief Report Density Support - Get supported density codes
     * @param mediaType Media type to query
     * @param allocationLength Buffer size
     */
    ScsiCommandResult reportDensitySupport(bool mediaType = false,
                                           quint16 allocationLength = 8192);

    /**
     * @brief Execute raw SCSI command
     * @param cdb Command Descriptor Block
     * @param direction Data transfer direction
     * @param data Data buffer (in/out depending on direction)
     * @param dataLength Expected data length
     * @return Command result
     */
    ScsiCommandResult executeRaw(const QByteArray &cdb,
                                 ScsiDataDirection direction,
                                 QByteArray &data,
                                 quint32 dataLength);

private:
    class Private;
    Private *d;
};

} // namespace qltfs
