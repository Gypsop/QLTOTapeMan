#include "tape_device_linux.h"

#include <linux/mtio.h>
#include <linux/sg.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <cstring>
#include <sstream>
#include <system_error>

namespace qlto {

namespace {

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

TapeDeviceLinux::~TapeDeviceLinux() { close(); }

bool TapeDeviceLinux::open(const std::string &path, std::string &err) {
    close();
    fd_ = ::open(path.c_str(), O_RDWR | O_NONBLOCK);
    if (fd_ < 0) {
        err = "failed to open device: " + path;
        return false;
    }
    if (fd_ >= 0) {
        device_path_ = path;
        return true;
    }
    return false;
}

void TapeDeviceLinux::close() {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
    device_path_.clear();
}

bool TapeDeviceLinux::send_cdb(const std::vector<std::uint8_t> &cdb,
                               std::vector<std::uint8_t> &data,
                               bool data_in,
                               std::uint32_t timeout_ms,
                               SenseData &sense,
                               std::string &err) {
    if (fd_ < 0) {
        err = "device not open";
        return false;
    }

    sg_io_hdr hdr{};
    std::memset(&hdr, 0, sizeof(hdr));
    hdr.interface_id = 'S';
    hdr.cmdp = const_cast<unsigned char *>(reinterpret_cast<const unsigned char *>(cdb.data()));
    hdr.cmd_len = static_cast<unsigned char>(cdb.size());
    hdr.mx_sb_len = sense.buffer.size();
    hdr.sbp = reinterpret_cast<unsigned char *>(sense.buffer.data());
    hdr.timeout = timeout_ms;

    if (!data.empty()) {
        hdr.dxferp = data.data();
        hdr.dxfer_len = data.size();
        hdr.dxfer_direction = data_in ? SG_DXFER_FROM_DEV : SG_DXFER_TO_DEV;
    } else {
        hdr.dxfer_direction = SG_DXFER_NONE;
    }

    int ret = ::ioctl(fd_, SG_IO, &hdr);
    if (ret < 0) {
        err = "SG_IO ioctl failed";
        return false;
    }
    if ((hdr.info & SG_INFO_OK_MASK) != SG_INFO_OK) {
        err = "SCSI command failed";
        return false;
    }
    return true;
}

bool TapeDeviceLinux::read_block(std::vector<std::uint8_t> &out, std::uint32_t block_len,
                                 SenseData &sense, std::string &err) {
    out.resize(block_len);
    std::vector<std::uint8_t> cdb = {0x08, 0, 0, 0, 0, 0}; // READ(6)
    cdb[2] = static_cast<std::uint8_t>((block_len >> 16) & 0xFF);
    cdb[3] = static_cast<std::uint8_t>((block_len >> 8) & 0xFF);
    cdb[4] = static_cast<std::uint8_t>(block_len & 0xFF);
    return send_cdb(cdb, out, true, 30000, sense, err);
}

bool TapeDeviceLinux::write_block(const std::uint8_t *data, std::size_t len,
                                  SenseData &sense, std::string &err) {
    std::vector<std::uint8_t> buf(data, data + len);
    std::vector<std::uint8_t> cdb = {0x0A, 0, 0, 0, 0, 0}; // WRITE(6)
    cdb[2] = static_cast<std::uint8_t>((len >> 16) & 0xFF);
    cdb[3] = static_cast<std::uint8_t>((len >> 8) & 0xFF);
    cdb[4] = static_cast<std::uint8_t>(len & 0xFF);
    return send_cdb(cdb, buf, false, 30000, sense, err);
}

bool TapeDeviceLinux::write_filemark(SenseData &sense, std::string &err) {
    std::vector<std::uint8_t> cdb = {0x10, 0, 0, 0, 1, 0}; // WRITE FILEMARKS(6)
    std::vector<std::uint8_t> data;
    return send_cdb(cdb, data, false, 10000, sense, err);
}

bool TapeDeviceLinux::load(bool threaded, SenseData &sense, std::string &err) {
    std::vector<std::uint8_t> cdb = {0x1B, 0, 0, 0, 0, 0}; // LOAD/UNLOAD
    cdb[4] = threaded ? 0x01 : 0x03; // 1 = load, 3 = load + set fm? using default load
    std::vector<std::uint8_t> data;
    return send_cdb(cdb, data, false, 60000, sense, err);
}

bool TapeDeviceLinux::unload(SenseData &sense, std::string &err) {
    std::vector<std::uint8_t> cdb = {0x1B, 0, 0, 0, 0, 0};
    cdb[4] = 0x00; // unload
    std::vector<std::uint8_t> data;
    return send_cdb(cdb, data, false, 60000, sense, err);
}

bool TapeDeviceLinux::read_position(PositionData &pos, SenseData &sense, std::string &err) {
    std::vector<std::uint8_t> cdb = {0x34, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    std::vector<std::uint8_t> data(20);
    if (!send_cdb(cdb, data, true, 5000, sense, err)) return false;
    pos.partition = data[1];
    pos.block = (static_cast<std::uint64_t>(data[4]) << 24) |
                (static_cast<std::uint64_t>(data[5]) << 16) |
                (static_cast<std::uint64_t>(data[6]) << 8) |
                (static_cast<std::uint64_t>(data[7]));
    return true;
}

bool TapeDeviceLinux::log_sense(std::uint8_t page, std::uint8_t subpage, LogPage &out,
                                SenseData &sense, std::string &err) {
    std::vector<std::uint8_t> cdb = {0x4D, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    cdb[1] = 0x00; // PPC=0
    cdb[2] = page;
    cdb[3] = subpage;
    std::vector<std::uint8_t> data(0xFF);
    cdb[7] = static_cast<std::uint8_t>((data.size() >> 8) & 0xFF);
    cdb[8] = static_cast<std::uint8_t>(data.size() & 0xFF);
    if (!send_cdb(cdb, data, true, 5000, sense, err)) return false;
    out.page = page;
    out.subpage = subpage;
    out.data = std::move(data);
    return true;
}

bool TapeDeviceLinux::read_mam(std::uint8_t page, std::uint8_t id, MamAttribute &out,
                               SenseData &sense, std::string &err) {
    std::vector<std::uint8_t> cdb = {0x8C, 0, 0, page, id, 0, 0, 0, 0, 0, 0, 0};
    std::vector<std::uint8_t> data(512);
    cdb[10] = static_cast<std::uint8_t>((data.size() >> 8) & 0xFF);
    cdb[11] = static_cast<std::uint8_t>(data.size() & 0xFF);
    if (!send_cdb(cdb, data, true, 5000, sense, err)) return false;
    out.page = page;
    out.id = id;
    out.data = std::move(data);
    return true;
}

bool TapeDeviceLinux::write_mam(std::uint8_t page, const MamAttribute &attr,
                                SenseData &sense, std::string &err) {
    std::vector<std::uint8_t> cdb = {0x8D, 0, 0, page, attr.id, 0, 0, 0, 0, 0, 0, 0};
    std::vector<std::uint8_t> data = attr.data;
    cdb[10] = static_cast<std::uint8_t>((data.size() >> 8) & 0xFF);
    cdb[11] = static_cast<std::uint8_t>(data.size() & 0xFF);
    return send_cdb(cdb, data, false, 5000, sense, err);
}

bool TapeDeviceLinux::format_mkltfs(const std::string &barcode, const std::string &volume_label,
                                    std::uint8_t extra_partitions, std::uint32_t block_len,
                                    bool worm_mode, const std::vector<std::uint8_t> &encryption_key,
                                    std::string &err) {
    if (device_path_.empty()) {
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
        std::string rendered = render_template(tmpl_env,
                                               device_path_,
                                               barcode,
                                               volume_label,
                                               partitions,
                                               block_len,
                                               worm_mode,
                                               keyfile_path.empty() ? std::string() : keyfile_path.string());
        cmd = rendered;
    } else {
        std::string exec_path = (exec_env && *exec_env) ? exec_env : "mkltfs";
        std::ostringstream oss;
        oss << quote_arg(exec_path) << " -d " << quote_arg(device_path_);
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

bool TapeDeviceLinux::scsi_pass_through(const std::vector<std::uint8_t> &cdb,
                                        std::vector<std::uint8_t> &data,
                                        bool data_in,
                                        std::uint32_t timeout_ms,
                                        SenseData &sense,
                                        std::string &err) {
    return send_cdb(cdb, data, data_in, timeout_ms, sense, err);
}

} // namespace qlto
