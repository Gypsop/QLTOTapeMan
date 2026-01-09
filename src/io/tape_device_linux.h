#pragma once

#include <fcntl.h>
#include <string>
#include <vector>

#include "tape_device.h"

namespace qlto {

class TapeDeviceLinux : public TapeDevice {
public:
    TapeDeviceLinux() = default;
    ~TapeDeviceLinux() override;

    bool open(const std::string &path, std::string &err) override;
    void close() override;

    bool read_block(std::vector<std::uint8_t> &out, std::uint32_t block_len,
                    SenseData &sense, std::string &err) override;

    bool write_block(const std::uint8_t *data, std::size_t len,
                     SenseData &sense, std::string &err) override;

    bool write_filemark(SenseData &sense, std::string &err) override;

    bool load(bool threaded, SenseData &sense, std::string &err) override;
    bool unload(SenseData &sense, std::string &err) override;

    bool read_position(PositionData &pos, SenseData &sense, std::string &err) override;

    bool log_sense(std::uint8_t page, std::uint8_t subpage, LogPage &out,
                   SenseData &sense, std::string &err) override;

    bool read_mam(std::uint8_t page, std::uint8_t id, MamAttribute &out,
                  SenseData &sense, std::string &err) override;

    bool write_mam(std::uint8_t page, const MamAttribute &attr,
                   SenseData &sense, std::string &err) override;

    bool format_mkltfs(const std::string &barcode, const std::string &volume_label,
                       std::uint8_t extra_partitions, std::uint32_t block_len,
                       bool worm_mode, const std::vector<std::uint8_t> &encryption_key,
                       std::string &err) override;

    bool scsi_pass_through(const std::vector<std::uint8_t> &cdb,
                           std::vector<std::uint8_t> &data,
                           bool data_in,
                           std::uint32_t timeout_ms,
                           SenseData &sense,
                           std::string &err) override;

private:
    int fd_ = -1;
    std::string device_path_;
    bool send_cdb(const std::vector<std::uint8_t> &cdb,
                  std::vector<std::uint8_t> &data,
                  bool data_in,
                  std::uint32_t timeout_ms,
                  SenseData &sense,
                  std::string &err);
};

} // namespace qlto
