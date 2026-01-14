/**
 * QLTOTapeMan - Qt-based LTO Tape Manager
 * Linux SCSI Implementation Header
 *
 * Copyright (c) 2026 Jeffrey ZHU (zhxsh1225@gmail.com)
 * https://github.com/Gypsop/QLTOTapeMan
 */

#ifndef QLTFS_LINUXSCSI_H
#define QLTFS_LINUXSCSI_H

#include "../../libqltfs_global.h"

#include <QString>

#if defined(Q_OS_LINUX)

namespace qltfs {

/**
 * @brief Tape position structure for Linux
 */
struct LinuxTapePosition {
    uint8_t partition = 0;
    uint32_t blockPosition = 0;
    uint32_t filePosition = 0;
    bool bop = false;  ///< Beginning of partition
    bool eop = false;  ///< End of partition
};

/**
 * @brief Space code for SPACE command
 */
enum class LinuxSpaceCode : uint8_t {
    Blocks = 0x00,
    Filemarks = 0x01,
    SequentialFilemarks = 0x02,
    EndOfData = 0x03,
    Setmarks = 0x04
};

/**
 * @brief Data direction for SCSI commands
 */
enum class LinuxDataDirection {
    NoData,
    DataIn,
    DataOut
};

/**
 * @brief Result of SCSI command execution
 */
struct LIBQLTFS_EXPORT LinuxScsiResult {
    bool success = false;
    uint8_t scsiStatus = 0;
    uint8_t senseKey = 0;
    uint8_t asc = 0;
    uint8_t ascq = 0;
    size_t dataTransferred = 0;
    QString errorMessage;
};

/**
 * @brief Linux-specific SCSI command implementation
 *
 * Uses SG_IO ioctl for low-level SCSI access.
 * This is a standalone low-level implementation that can be used
 * by the main ScsiCommand class or independently.
 */
class LIBQLTFS_EXPORT LinuxScsi
{
public:
    /**
     * @brief Construct Linux SCSI handler
     * @param devicePath Linux device path (e.g., /dev/nst0, /dev/sg0)
     */
    explicit LinuxScsi(const QString& devicePath);
    ~LinuxScsi();

    // Prevent copying
    LinuxScsi(const LinuxScsi&) = delete;
    LinuxScsi& operator=(const LinuxScsi&) = delete;

    // Allow moving
    LinuxScsi(LinuxScsi&& other) noexcept;
    LinuxScsi& operator=(LinuxScsi&& other) noexcept;

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
     * @return true if device file descriptor is valid
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
     * @return LinuxScsiResult with status and any error information
     */
    LinuxScsiResult executeCommand(
        const uint8_t* cdb,
        size_t cdbLength,
        uint8_t* data,
        size_t dataLength,
        LinuxDataDirection direction,
        int timeoutSecs = 60,
        uint8_t* senseData = nullptr,
        size_t senseLength = 0
    );

    // === SCSI Tape Commands ===

    /**
     * @brief Test Unit Ready command
     */
    LinuxScsiResult testUnitReady();

    /**
     * @brief Inquiry command
     */
    LinuxScsiResult inquiry(uint8_t* inquiryData, size_t length);

    /**
     * @brief Read Block Limits command
     */
    LinuxScsiResult readBlockLimits(uint32_t& minBlockSize, uint32_t& maxBlockSize);

    /**
     * @brief Request Sense command
     */
    LinuxScsiResult requestSense(uint8_t* senseBuffer, size_t length);

    /**
     * @brief Read command (6-byte CDB)
     */
    LinuxScsiResult read6(uint8_t* buffer, size_t length, bool fixed, size_t transferLength);

    /**
     * @brief Write command (6-byte CDB)
     */
    LinuxScsiResult write6(const uint8_t* buffer, size_t length, bool fixed, size_t transferLength);

    /**
     * @brief Read Position command
     */
    LinuxScsiResult readPosition(LinuxTapePosition& position);

    /**
     * @brief Locate command (10-byte)
     */
    LinuxScsiResult locate10(uint32_t blockAddress, uint8_t partition);

    /**
     * @brief Locate command (16-byte)
     */
    LinuxScsiResult locate16(uint64_t blockAddress, uint8_t partition);

    /**
     * @brief Rewind command
     */
    LinuxScsiResult rewind();

    /**
     * @brief Space command
     */
    LinuxScsiResult space(LinuxSpaceCode code, int32_t count);

    /**
     * @brief Write Filemarks command
     */
    LinuxScsiResult writeFilemarks(uint32_t count, bool setmark = false);

    /**
     * @brief Load/Unload command
     */
    LinuxScsiResult loadUnload(bool load);

    /**
     * @brief Erase command
     */
    LinuxScsiResult erase(bool longErase = false);

    /**
     * @brief Mode Sense (6-byte) command
     */
    LinuxScsiResult modeSense6(uint8_t pageCode, uint8_t* buffer, size_t length);

    /**
     * @brief Mode Sense (10-byte) command
     */
    LinuxScsiResult modeSense10(uint8_t pageCode, uint8_t* buffer, size_t length);

    /**
     * @brief Mode Select (6-byte) command
     */
    LinuxScsiResult modeSelect6(const uint8_t* buffer, size_t length, bool savePage = false);

    /**
     * @brief Mode Select (10-byte) command
     */
    LinuxScsiResult modeSelect10(const uint8_t* buffer, size_t length, bool savePage = false);

    /**
     * @brief Log Sense command
     */
    LinuxScsiResult logSense(uint8_t pageCode, uint8_t* buffer, size_t length);

    /**
     * @brief Read Attribute command
     */
    LinuxScsiResult readAttribute(uint16_t attributeId, uint8_t* buffer, size_t length, uint8_t partition = 0);

    /**
     * @brief Write Attribute command
     */
    LinuxScsiResult writeAttribute(uint16_t attributeId, const uint8_t* buffer, size_t length, uint8_t partition = 0);

    /**
     * @brief Report Density Support command
     */
    LinuxScsiResult reportDensitySupport(uint8_t* buffer, size_t length, bool mediaInfo = false);

    /**
     * @brief Set Capacity command (for WORM verification)
     */
    LinuxScsiResult setCapacity(uint64_t capacity);

    /**
     * @brief Allow/Prevent Medium Removal command
     */
    LinuxScsiResult preventAllowMediumRemoval(bool prevent);

    /**
     * @brief Reserve Unit command
     */
    LinuxScsiResult reserveUnit();

    /**
     * @brief Release Unit command
     */
    LinuxScsiResult releaseUnit();

    // === Utility Methods ===

    /**
     * @brief Get the last errno value
     */
    int getLastErrno() const { return m_lastErrno; }

    /**
     * @brief Format errno to string
     */
    static QString formatErrno(int errnoValue);

    /**
     * @brief Check if device supports SG_IO
     */
    bool supportsSgIo() const { return m_sgIoSupported; }

private:
    /**
     * @brief Parse SCSI sense data
     */
    void parseSenseData(const uint8_t* senseData, size_t length, ScsiResult& result);

    /**
     * @brief Check device type and capabilities
     */
    bool checkDeviceCapabilities();

    QString m_devicePath;       ///< Device path
    int m_fd;                   ///< File descriptor
    int m_lastErrno;            ///< Last errno value
    bool m_sgIoSupported;       ///< SG_IO support flag
    uint8_t m_senseBuffer[64];  ///< Internal sense buffer
};

} // namespace qltfs

#endif // Q_OS_LINUX

#endif // QLTFS_LINUXSCSI_H
