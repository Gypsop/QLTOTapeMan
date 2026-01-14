/**
 * QLTOTapeMan - Qt-based LTO Tape Manager
 * Windows SCSI Implementation Header
 *
 * Copyright (c) 2026 Jeffrey ZHU (zhxsh1225@gmail.com)
 * https://github.com/Gypsop/QLTOTapeMan
 */

#ifndef QLTFS_WINSCSI_H
#define QLTFS_WINSCSI_H

#include "../../libqltfs_global.h"

#ifdef Q_OS_WIN

#include <QString>
#include <windows.h>
#include <winioctl.h>
#include <ntddscsi.h>

namespace qltfs {

/**
 * @brief Tape position structure
 */
struct WinTapePosition {
    uint8_t partition = 0;
    uint32_t blockPosition = 0;
    uint32_t filePosition = 0;
    bool bop = false;  ///< Beginning of partition
    bool eop = false;  ///< End of partition
};

/**
 * @brief Space code for SPACE command
 */
enum class WinSpaceCode : uint8_t {
    Blocks = 0x00,
    Filemarks = 0x01,
    SequentialFilemarks = 0x02,
    EndOfData = 0x03,
    Setmarks = 0x04
};

/**
 * @brief Data direction for SCSI commands
 */
enum class WinDataDirection {
    NoData,
    DataIn,
    DataOut
};

/**
 * @brief Result of SCSI command execution
 */
struct LIBQLTFS_EXPORT WinScsiResult {
    bool success = false;
    uint8_t scsiStatus = 0;
    uint8_t senseKey = 0;
    uint8_t asc = 0;
    uint8_t ascq = 0;
    size_t dataTransferred = 0;
    QString errorMessage;
};

/**
 * @brief Windows-specific SCSI command implementation
 *
 * Uses IOCTL_SCSI_PASS_THROUGH_DIRECT for low-level SCSI access.
 * This is a standalone low-level implementation that can be used
 * by the main ScsiCommand class or independently.
 */
class LIBQLTFS_EXPORT WinScsi
{
public:
    /**
     * @brief Construct Windows SCSI handler
     * @param devicePath Windows device path (e.g., \\.\Tape0)
     */
    explicit WinScsi(const QString& devicePath);
    ~WinScsi();

    // Prevent copying
    WinScsi(const WinScsi&) = delete;
    WinScsi& operator=(const WinScsi&) = delete;

    // Allow moving
    WinScsi(WinScsi&& other) noexcept;
    WinScsi& operator=(WinScsi&& other) noexcept;

    /**
     * @brief Open the device for SCSI operations
     * @return true if opened successfully
     */
    bool open();

    /**
     * @brief Close the device
     */
    void close();

    /**
     * @brief Check if device is open
     * @return true if device handle is valid
     */
    bool isOpen() const;

    /**
     * @brief Execute a SCSI command
     * @param cdb Command Descriptor Block
     * @param cdbLength Length of CDB
     * @param data Data buffer (for transfer)
     * @param dataLength Length of data buffer
     * @param direction Data transfer direction
     * @param timeoutSecs Timeout in seconds
     * @param senseData Buffer for sense data (optional)
     * @param senseLength Length of sense buffer
     * @return WinScsiResult with status and any error information
     */
    WinScsiResult executeCommand(
        const uint8_t* cdb,
        size_t cdbLength,
        uint8_t* data,
        size_t dataLength,
        WinDataDirection direction,
        int timeoutSecs = 60,
        uint8_t* senseData = nullptr,
        size_t senseLength = 0
    );

    // === SCSI Tape Commands ===

    /**
     * @brief Test Unit Ready command
     */
    WinScsiResult testUnitReady();

    /**
     * @brief Inquiry command
     * @param inquiryData Buffer to receive inquiry data
     * @param length Expected length of inquiry data
     */
    WinScsiResult inquiry(uint8_t* inquiryData, size_t length);

    /**
     * @brief Read Block Limits command
     */
    WinScsiResult readBlockLimits(uint32_t& minBlockSize, uint32_t& maxBlockSize);

    /**
     * @brief Request Sense command
     */
    WinScsiResult requestSense(uint8_t* senseBuffer, size_t length);

    /**
     * @brief Read command (6-byte CDB)
     */
    WinScsiResult read6(uint8_t* buffer, size_t length, bool fixed, size_t transferLength);

    /**
     * @brief Write command (6-byte CDB)
     */
    WinScsiResult write6(const uint8_t* buffer, size_t length, bool fixed, size_t transferLength);

    /**
     * @brief Read Position command
     */
    WinScsiResult readPosition(WinTapePosition& position);

    /**
     * @brief Locate command (10-byte)
     */
    WinScsiResult locate10(uint32_t blockAddress, uint8_t partition);

    /**
     * @brief Locate command (16-byte)
     */
    WinScsiResult locate16(uint64_t blockAddress, uint8_t partition);

    /**
     * @brief Rewind command
     */
    WinScsiResult rewind();

    /**
     * @brief Space command
     */
    WinScsiResult space(WinSpaceCode code, int32_t count);

    /**
     * @brief Write Filemarks command
     */
    WinScsiResult writeFilemarks(uint32_t count, bool setmark = false);

    /**
     * @brief Load/Unload command
     */
    WinScsiResult loadUnload(bool load);

    /**
     * @brief Erase command
     */
    WinScsiResult erase(bool longErase = false);

    /**
     * @brief Mode Sense (6-byte) command
     */
    WinScsiResult modeSense6(uint8_t pageCode, uint8_t* buffer, size_t length);

    /**
     * @brief Mode Sense (10-byte) command
     */
    WinScsiResult modeSense10(uint8_t pageCode, uint8_t* buffer, size_t length);

    /**
     * @brief Mode Select (6-byte) command
     */
    WinScsiResult modeSelect6(const uint8_t* buffer, size_t length, bool savePage = false);

    /**
     * @brief Mode Select (10-byte) command
     */
    WinScsiResult modeSelect10(const uint8_t* buffer, size_t length, bool savePage = false);

    /**
     * @brief Log Sense command
     */
    WinScsiResult logSense(uint8_t pageCode, uint8_t* buffer, size_t length);

    /**
     * @brief Read Attribute command
     */
    WinScsiResult readAttribute(uint16_t attributeId, uint8_t* buffer, size_t length, uint8_t partition = 0);

    /**
     * @brief Write Attribute command
     */
    WinScsiResult writeAttribute(uint16_t attributeId, const uint8_t* buffer, size_t length, uint8_t partition = 0);

    /**
     * @brief Report Density Support command
     */
    WinScsiResult reportDensitySupport(uint8_t* buffer, size_t length, bool mediaInfo = false);

    /**
     * @brief Set Capacity command (for WORM verification)
     */
    WinScsiResult setCapacity(uint64_t capacity);

    /**
     * @brief Allow/Prevent Medium Removal command
     */
    WinScsiResult preventAllowMediumRemoval(bool prevent);

    /**
     * @brief Reserve Unit command
     */
    WinScsiResult reserveUnit();

    /**
     * @brief Release Unit command
     */
    WinScsiResult releaseUnit();

    // === Utility Methods ===

    /**
     * @brief Get the last Windows error code
     */
    DWORD getLastWinError() const { return m_lastWinError; }

    /**
     * @brief Format Windows error code to string
     */
    static QString formatWinError(DWORD errorCode);

private:
    /**
     * @brief Build SCSI_PASS_THROUGH_DIRECT structure
     */
    bool buildSptd(
        SCSI_PASS_THROUGH_DIRECT& sptd,
        const uint8_t* cdb,
        size_t cdbLength,
        uint8_t* data,
        size_t dataLength,
        DataDirection direction,
        int timeoutSecs
    );

    /**
     * @brief Parse SCSI sense data
     */
    void parseSenseData(const uint8_t* senseData, size_t length, ScsiResult& result);

    QString m_devicePath;       ///< Device path
    HANDLE m_deviceHandle;      ///< Windows device handle
    DWORD m_lastWinError;       ///< Last Windows error code
    uint8_t m_senseBuffer[64];  ///< Internal sense buffer
};

} // namespace qltfs

#endif // Q_OS_WIN

#endif // QLTFS_WINSCSI_H
