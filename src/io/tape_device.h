#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace qlto {

struct SenseData {
    std::array<std::uint8_t, 64> buffer{};
};

struct PositionData {
    std::uint64_t block = 0;
    std::uint32_t partition = 0;
};

struct LogPage {
    std::uint8_t page = 0;
    std::uint8_t subpage = 0;
    std::vector<std::uint8_t> data;
};

struct MamAttribute {
    std::uint8_t page = 0;
    std::uint8_t id = 0;
    std::vector<std::uint8_t> data;
};

class TapeDevice {
public:
    virtual ~TapeDevice() = default;

    virtual bool open(const std::string &path, std::string &err) = 0;
    virtual void close() = 0;

    virtual bool read_block(std::vector<std::uint8_t> &out, std::uint32_t block_len,
                            SenseData &sense, std::string &err) = 0;

    virtual bool write_block(const std::uint8_t *data, std::size_t len,
                             SenseData &sense, std::string &err) = 0;

    virtual bool write_filemark(SenseData &sense, std::string &err) = 0;

    virtual bool load(bool threaded, SenseData &sense, std::string &err) = 0;
    virtual bool unload(SenseData &sense, std::string &err) = 0;

    virtual bool read_position(PositionData &pos, SenseData &sense, std::string &err) = 0;

    virtual bool log_sense(std::uint8_t page, std::uint8_t subpage, LogPage &out,
                           SenseData &sense, std::string &err) = 0;

    virtual bool read_mam(std::uint8_t page, std::uint8_t id, MamAttribute &out,
                          SenseData &sense, std::string &err) = 0;

    virtual bool write_mam(std::uint8_t page, const MamAttribute &attr,
                           SenseData &sense, std::string &err) = 0;

    virtual bool format_mkltfs(const std::string &barcode, const std::string &volume_label,
                               std::uint8_t extra_partitions, std::uint32_t block_len,
                               bool worm_mode, const std::vector<std::uint8_t> &encryption_key,
                               std::string &err) = 0;

    virtual bool scsi_pass_through(const std::vector<std::uint8_t> &cdb,
                                   std::vector<std::uint8_t> &data,
                                   bool data_in,
                                   std::uint32_t timeout_ms,
                                   SenseData &sense,
                                   std::string &err) = 0;
};

} // namespace qlto
