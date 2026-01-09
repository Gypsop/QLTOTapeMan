#include "tape_device_mac.h"

#include <IOKit/IOKitLib.h>
#include <IOKit/serial/IOSerialKeys.h>
#include <IOKit/storage/IODVDMedia.h>
#include <IOKit/storage/IOLTO.h>
#include <IOKit/storage/IOMedia.h>
#include <IOKit/storage/IOTape.h>
#include <IOKit/storage/IOBlockStorageDriver.h>
#include <IOKit/storage/IOStorageDeviceCharacteristics.h>
#include <IOKit/storage/IOStorageProtocolCharacteristics.h>
#include <IOKit/scsi/SCSITaskLib.h>
#include <IOKit/scsi/SCSICommandOperationCodes.h>
#include <IOKit/scsi/SCSICmds_REQUEST_SENSE_Defs.h>
#include <IOKit/scsi/IOSCSITaskDeviceInterface.h>
#include <CoreFoundation/CoreFoundation.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits.h>
#include <memory>
#include <sstream>
#include <system_error>
#include <cstdlib>

namespace qlto {
namespace {

class ScsiTaskWrapper {
public:
    explicit ScsiTaskWrapper(SCSITaskDeviceInterface **iface) : iface_(iface) {}
    ~ScsiTaskWrapper() {
        if (task_) {
            (*iface_)->ReleaseTask(iface_, task_);
        }
    }

    bool allocate(std::string &err) {
        task_ = (*iface_)->CreateTask(iface_);
        if (!task_) {
            err = "Failed to create SCSI task";
            return false;
        }
        return true;
    }

    SCSITaskInterface **get() { return task_; }

private:
    SCSITaskDeviceInterface **iface_{};
    SCSITaskInterface **task_{};
};

static void fill_sense(const SCSI_Sense_Data &sense, SenseData &out) {
    out.key = sense.SENSE_KEY & kSENSE_KEY_Mask;
    out.asc = sense.ADDITIONAL_SENSE_CODE;
    out.ascq = sense.ADDITIONAL_SENSE_CODE_QUALIFIER;
}

static bool wait_task(SCSITaskInterface **task, std::uint32_t timeout_ms, SenseData &sense, std::string &err) {
    SCSITaskStatus status = kSCSITaskStatus_No_Status;
    IOReturn rc = (*task)->ExecuteTaskSync(task, timeout_ms, &status, nullptr);
    if (rc != kIOReturnSuccess) {
        err = "SCSI task execution failed: " + std::to_string(rc);
        return false;
    }
    if (status != kSCSITaskStatus_GOOD) {
        SCSI_Sense_Data sense_data{};
        std::memset(&sense_data, 0, sizeof(sense_data));
        (*task)->GetSenseData(task, &sense_data, sizeof(sense_data));
        fill_sense(sense_data, sense);
        err = "SCSI task returned status " + std::to_string(status);
        return false;
    }
    return true;
}

static bool build_cdb(SCSITaskInterface **task, const std::vector<std::uint8_t> &cdb, bool data_in,
                      std::vector<std::uint8_t> &data, SenseData &sense, std::string &err) {
    if (cdb.size() > kSCSICDBSize_16Byte) {
        err = "CDB too large";
        return false;
    }

    if (!data.empty()) {
        IOVirtualRange range{};
        range.address = reinterpret_cast<IOVirtualAddress>(data.data());
        range.length = data.size();
        auto dir = data_in ? kSCSIDataTransfer_FromTargetToInitiator : kSCSIDataTransfer_FromInitiatorToTarget;
        if ((*task)->SetScatterGatherEntries(task, &range, 1, 0, data.size(), dir) != kIOReturnSuccess) {
            err = "Failed to set SG entries";
            return false;
        }
    }

    std::array<std::uint8_t, kSCSICDBSize_16Byte> cdb_buf{};
    std::copy(cdb.begin(), cdb.end(), cdb_buf.begin());
    if ((*task)->SetCommandDescriptorBlock(task, cdb_buf.data(), cdb.size()) != kIOReturnSuccess) {
        err = "Failed to set CDB";
        return false;
    }
    return true;
}

std::string quote_arg(const std::string &arg) {
    std::ostringstream oss;
    oss << "'";
    for (char c : arg) {
        if (c == '\'') {
            oss << "'\\''";
        } else {
            oss << c;
        }
    }
    oss << "'";
    return oss.str();
}

std::string replace_all(std::string text, const std::string &from, const std::string &to) {
    std::size_t pos = 0;
    while ((pos = text.find(from, pos)) != std::string::npos) {
        text.replace(pos, from.size(), to);
        pos += to.size();
    }
    return text;
}

std::string render_template(const std::string &tmpl,
                            const std::string &device,
                            const std::string &barcode,
                            const std::string &volume,
                            std::uint32_t partitions,
                            std::uint32_t block_len,
                            bool worm,
                            const std::string &keyfile) {
    std::string res = tmpl;
    res = replace_all(res, "{device}", device);
    res = replace_all(res, "{barcode}", barcode);
    res = replace_all(res, "{volume}", volume);
    res = replace_all(res, "{partitions}", std::to_string(partitions));
    res = replace_all(res, "{block}", std::to_string(block_len));
    res = replace_all(res, "{worm}", worm ? std::string("--worm") : std::string());
    res = replace_all(res, "{keyfile}", keyfile);
    return res;
}

} // namespace

class TapeDeviceMac::Impl {
public:
    ~Impl() { close(); }

    bool open(const std::string &path, std::string &err) {
        CFMutableDictionaryRef matching = IOServiceMatching(kIOSCSITapeClassName);
        if (!matching) {
            err = "Failed to create matching dictionary";
            return false;
        }

        io_iterator_t iter{};
        if (IOServiceGetMatchingServices(kIOMainPortDefault, matching, &iter) != KERN_SUCCESS) {
            err = "Failed to enumerate tape devices";
            return false;
        }

        bool found = false;
        io_object_t obj;
        while ((obj = IOIteratorNext(iter))) {
            CFStringRef bsd_path = (CFStringRef)IORegistryEntryCreateCFProperty(
                obj, CFSTR(kIOBSDNameKey), kCFAllocatorDefault, 0);
            if (bsd_path) {
                char buf[PATH_MAX]{};
                if (CFStringGetCString(bsd_path, buf, sizeof(buf), kCFStringEncodingUTF8)) {
                    std::string dev = "/dev/" + std::string(buf);
                    if (dev == path) {
                        found = true;
                        IOObjectRelease(bsd_path);
                        break;
                    }
                }
                IOObjectRelease(bsd_path);
            }
            IOObjectRelease(obj);
        }
        IOObjectRelease(iter);
        if (!found) {
            err = "Device not found: " + path;
            return false;
        }

        SInt32 score = 0;
        IOCFPlugInInterface **plugin = nullptr;
        if (IOCreatePlugInInterfaceForService(obj, kIOSCSITaskDeviceUserClientTypeID,
                                              kIOCFPlugInInterfaceID, &plugin, &score) != kIOReturnSuccess) {
            err = "Failed to create plugin interface";
            IOObjectRelease(obj);
            return false;
        }
        HRESULT res = (*plugin)->QueryInterface(plugin, CFUUIDGetUUIDBytes(kIOSCSITaskDeviceInterfaceID),
                               reinterpret_cast<void **>(&task_iface_));
        (*plugin)->Release(plugin);
        IOObjectRelease(obj);
        if (res || !task_iface_) {
            err = "Failed to query SCSI task interface";
            return false;
        }
        opened_path_ = path;
        return true;
    }

    void close() {
        if (task_iface_) {
            (*task_iface_)->Release(task_iface_);
            task_iface_ = nullptr;
        }
        opened_path_.clear();
    }

    bool scsi_io(const std::vector<std::uint8_t> &cdb, std::vector<std::uint8_t> &data, bool data_in,
                 std::uint32_t timeout_ms, SenseData &sense, std::string &err) {
        if (!task_iface_) {
            err = "Device not open";
            return false;
        }
        ScsiTaskWrapper wrapper(task_iface_);
        if (!wrapper.allocate(err)) return false;
        if (!build_cdb(wrapper.get(), cdb, data_in, data, sense, err)) return false;
        return wait_task(wrapper.get(), timeout_ms, sense, err);
    }

    std::string opened_path_;
    SCSITaskDeviceInterface **task_iface_{};
};

TapeDeviceMac::TapeDeviceMac() : impl_(std::make_unique<Impl>()) {}
TapeDeviceMac::~TapeDeviceMac() = default;

bool TapeDeviceMac::open(const std::string &path, std::string &err) { return impl_->open(path, err); }
void TapeDeviceMac::close() { impl_->close(); }

bool TapeDeviceMac::read_block(std::vector<std::uint8_t> &out, std::uint32_t block_len, SenseData &sense, std::string &err) {
    out.resize(block_len);
    std::vector<std::uint8_t> cdb(10, 0);
    cdb[0] = kSCSICmd_READ10;
    cdb[7] = static_cast<std::uint8_t>((block_len >> 8) & 0xFF);
    cdb[8] = static_cast<std::uint8_t>(block_len & 0xFF);
    return scsi_pass_through(cdb, out, true, 60000, sense, err);
}

bool TapeDeviceMac::write_block(const std::uint8_t *data, std::size_t len, SenseData &sense, std::string &err) {
    std::vector<std::uint8_t> cdb(10, 0);
    cdb[0] = kSCSICmd_WRITE10;
    cdb[7] = static_cast<std::uint8_t>((len >> 8) & 0xFF);
    cdb[8] = static_cast<std::uint8_t>(len & 0xFF);
    std::vector<std::uint8_t> payload(data, data + len);
    return scsi_pass_through(cdb, payload, false, 60000, sense, err);
}

bool TapeDeviceMac::write_filemark(SenseData &sense, std::string &err) {
    std::vector<std::uint8_t> cdb(6, 0);
    cdb[0] = kSCSICmd_WRITE_FILEMARKS6;
    return scsi_pass_through(cdb, {}, false, 60000, sense, err);
}

bool TapeDeviceMac::load(bool, SenseData &sense, std::string &err) {
    std::vector<std::uint8_t> cdb(6, 0);
    cdb[0] = kSCSICmd_LOAD_UNLOAD;
    cdb[4] = 1; // load
    return scsi_pass_through(cdb, {}, false, 120000, sense, err);
}

bool TapeDeviceMac::unload(SenseData &sense, std::string &err) {
    std::vector<std::uint8_t> cdb(6, 0);
    cdb[0] = kSCSICmd_LOAD_UNLOAD;
    cdb[4] = 0; // unload
    return scsi_pass_through(cdb, {}, false, 120000, sense, err);
}

bool TapeDeviceMac::read_position(PositionData &pos, SenseData &sense, std::string &err) {
    std::vector<std::uint8_t> cdb(10, 0);
    cdb[0] = kSCSICmd_READ_POSITION;
    std::vector<std::uint8_t> buf(20, 0);
    if (!scsi_pass_through(cdb, buf, true, 60000, sense, err)) return false;
    if (buf.size() < 20) {
        err = "READ POSITION buffer too small";
        return false;
    }
    pos.partition = buf[1];
    pos.block_number = (static_cast<std::uint32_t>(buf[4]) << 24) |
                       (static_cast<std::uint32_t>(buf[5]) << 16) |
                       (static_cast<std::uint32_t>(buf[6]) << 8) |
                       static_cast<std::uint32_t>(buf[7]);
    pos.file_number = (static_cast<std::uint32_t>(buf[8]) << 24) |
                      (static_cast<std::uint32_t>(buf[9]) << 16) |
                      (static_cast<std::uint32_t>(buf[10]) << 8) |
                      static_cast<std::uint32_t>(buf[11]);
    pos.setmarks = false;
    pos.bop = (buf[0] & 0x80) != 0;
    pos.eop = (buf[0] & 0x40) != 0;
    return true;
}

bool TapeDeviceMac::log_sense(std::uint8_t page, std::uint8_t subpage, LogPage &out, SenseData &sense, std::string &err) {
    std::vector<std::uint8_t> cdb(10, 0);
    cdb[0] = kSCSICmd_LOG_SENSE;
    cdb[1] = 0x40; // PPC = 0, SPF = 1
    cdb[2] = page;
    cdb[3] = subpage;
    cdb[7] = 0;
    cdb[8] = 0xFF; // max length
    std::vector<std::uint8_t> buf(0xFF, 0);
    if (!scsi_pass_through(cdb, buf, true, 30000, sense, err)) return false;
    out.page_code = page;
    out.subpage = subpage;
    out.data.assign(buf.begin(), buf.end());
    return true;
}

bool TapeDeviceMac::read_mam(std::uint8_t page, std::uint8_t id, MamAttribute &out, SenseData &sense, std::string &err) {
    std::vector<std::uint8_t> cdb(12, 0);
    cdb[0] = 0x8C; // READ ATTRIBUTE (SSC)
    cdb[1] = 0x00; // service action
    cdb[2] = 0x00; // element type
    cdb[3] = page;
    cdb[4] = id;
    cdb[8] = 0;
    cdb[9] = 0xFC; // allocation length (252)
    std::vector<std::uint8_t> buf(0xFC, 0);
    if (!scsi_pass_through(cdb, buf, true, 30000, sense, err)) return false;
    if (buf.size() < 8) {
        err = "READ ATTRIBUTE response too short";
        return false;
    }
    out.page = page;
    out.id = id;
    out.data.assign(buf.begin() + 8, buf.end());
    return true;
}

bool TapeDeviceMac::write_mam(std::uint8_t page, const MamAttribute &attr, SenseData &sense, std::string &err) {
    std::vector<std::uint8_t> cdb(12, 0);
    cdb[0] = 0x8D; // WRITE ATTRIBUTE (SSC)
    cdb[3] = page;
    cdb[4] = attr.id;
    cdb[8] = static_cast<std::uint8_t>((attr.data.size() >> 8) & 0xFF);
    cdb[9] = static_cast<std::uint8_t>(attr.data.size() & 0xFF);

    std::vector<std::uint8_t> payload;
    payload.reserve(8 + attr.data.size());
    payload.push_back(0x00); // element type
    payload.push_back(page);
    payload.push_back(attr.id);
    payload.push_back(0x00);
    payload.push_back(0x00);
    payload.push_back(static_cast<std::uint8_t>((attr.data.size() >> 8) & 0xFF));
    payload.push_back(static_cast<std::uint8_t>(attr.data.size() & 0xFF));
    payload.push_back(0x00);
    payload.insert(payload.end(), attr.data.begin(), attr.data.end());

    return scsi_pass_through(cdb, payload, false, 30000, sense, err);
}

bool TapeDeviceMac::format_mkltfs(const std::string &barcode, const std::string &volume_label, std::uint8_t extra_partitions, std::uint32_t block_len,
                                  bool worm_mode, const std::vector<std::uint8_t> &encryption_key, std::string &err) {
    if (impl_->opened_path_.empty()) {
        err = "device not open";
        return false;
    }

    std::filesystem::path keyfile_path;
    if (!encryption_key.empty()) {
        keyfile_path = std::filesystem::temp_directory_path() / "qltotapeman_mkltfs.key";
        std::ofstream ofs(keyfile_path, std::ios::binary | std::ios::trunc);
        if (!ofs) {
            err = "failed to create temporary key file";
            return false;
        }
        ofs.write(reinterpret_cast<const char *>(encryption_key.data()), static_cast<std::streamsize>(encryption_key.size()));
    }

    const char *tmpl_env = std::getenv("QLTOTAPEMAN_MKLTFSCMD");
    const char *exec_env = std::getenv("QLTOTAPEMAN_MKLTFSPATH");
    std::string cmd;
    std::uint32_t partitions = static_cast<std::uint32_t>(1 + extra_partitions);

    if (tmpl_env && *tmpl_env) {
        cmd = render_template(tmpl_env,
                              impl_->opened_path_,
                              barcode,
                              volume_label,
                              partitions,
                              block_len,
                              worm_mode,
                              keyfile_path.empty() ? std::string() : keyfile_path.string());
    } else {
        std::string exec_path = (exec_env && *exec_env) ? exec_env : "mkltfs";
        std::ostringstream oss;
        oss << quote_arg(exec_path) << " -d " << quote_arg(impl_->opened_path_);
        if (!volume_label.empty()) oss << " --labeltext " << quote_arg(volume_label);
        if (!barcode.empty()) oss << " --barcode " << quote_arg(barcode);
        if (block_len > 0) oss << " --blocksize " << block_len;
        if (partitions > 1) oss << " --partitions " << partitions;
        if (worm_mode) oss << " --worm";
        if (!keyfile_path.empty()) oss << " --keyfile " << quote_arg(keyfile_path.string());
        oss << " --force";
        cmd = oss.str();
    }

    int rc = std::system(cmd.c_str());
    if (!keyfile_path.empty()) {
        std::error_code ec;
        std::filesystem::remove(keyfile_path, ec);
    }
    if (rc != 0) {
        err = "mkltfs command failed with exit code " + std::to_string(rc);
        return false;
    }
    return true;
}

bool TapeDeviceMac::scsi_pass_through(const std::vector<std::uint8_t> &cdb, std::vector<std::uint8_t> &data,
                                      bool data_in, std::uint32_t timeout_ms, SenseData &sense, std::string &err) {
    return impl_->scsi_io(cdb, data, data_in, timeout_ms, sense, err);
}

} // namespace qlto
