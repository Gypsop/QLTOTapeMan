#ifndef SCSIUTILS_H
#define SCSIUTILS_H

#include <cstdint>
#include <cstring>

// SCSI Operation Codes
#define SCSIOP_INQUIRY          0x12
#define SCSIOP_TEST_UNIT_READY  0x00
#define SCSIOP_REWIND           0x01
#define SCSIOP_SPACE            0x11
#define SCSIOP_LOCATE           0x2B
#define SCSIOP_READ_6           0x08
#define SCSIOP_WRITE_6          0x0A
#define SCSIOP_MODE_SENSE_6     0x1A
#define SCSIOP_MODE_SELECT_6    0x15
#define SCSIOP_START_STOP_UNIT  0x1B
#define SCSIOP_READ_BLOCK_LIMITS 0x05
#define SCSIOP_READ_POSITION    0x34

// SCSI Status Codes
#define SCSISTAT_GOOD           0x00
#define SCSISTAT_CHECK_CONDITION 0x02
#define SCSISTAT_CONDITION_MET  0x04
#define SCSISTAT_BUSY           0x08

// Data Direction
enum class ScsiDirection {
    None,
    In,     // Device to Host
    Out     // Host to Device
};

// Standard Inquiry Data Structure
#pragma pack(push, 1)
struct ScsiInquiryData {
    uint8_t DeviceType : 5;
    uint8_t PeripheralQualifier : 3;
    
    uint8_t RemovableMedia : 1;
    uint8_t Reserved1 : 7;
    
    uint8_t Version;
    
    uint8_t ResponseDataFormat : 4;
    uint8_t HiSupport : 1;
    uint8_t NormACA : 1;
    uint8_t Reserved2 : 2;
    
    uint8_t AdditionalLength;
    uint8_t Reserved3[2];
    
    uint8_t SoftReset : 1;
    uint8_t CmdQue : 1;
    uint8_t Reserved4 : 1;
    uint8_t Linked : 1;
    uint8_t Sync : 1;
    uint8_t WBus16 : 1;
    uint8_t WBus32 : 1;
    uint8_t RelAddr : 1;
    
    char VendorId[8];
    char ProductId[16];
    char ProductRevisionLevel[4];
    char VendorSpecific[20];
    uint8_t Reserved5[40];
};

struct ScsiSenseData {
    uint8_t ResponseCode; // 0x70 or 0x71. Bit 7 is Valid.
    uint8_t SegmentNumber;
    uint8_t Flags; // Bit 7=FileMark, 6=EOM, 5=ILI, 3-0=SenseKey
    uint8_t Information[4];
    uint8_t AdditionalSenseLength;
    uint8_t CommandSpecificInformation[4];
    uint8_t ASC;
    uint8_t ASCQ;
    uint8_t FieldReplaceableUnitCode;
    uint8_t SenseKeySpecific[3];
    
    bool isValid() const { return (ResponseCode & 0x80) != 0; }
    bool isFileMark() const { return (Flags & 0x80) != 0; }
    bool isEOM() const { return (Flags & 0x40) != 0; }
    bool isILI() const { return (Flags & 0x20) != 0; }
    uint8_t senseKey() const { return Flags & 0x0F; }
};
#pragma pack(pop)

// Helper to clean strings (remove padding spaces)
inline void cleanScsiString(char* str, size_t len) {
    for (int i = len - 1; i >= 0; --i) {
        if (str[i] == ' ' || str[i] == 0) {
            str[i] = 0;
        } else {
            break;
        }
    }
}

#endif // SCSIUTILS_H
