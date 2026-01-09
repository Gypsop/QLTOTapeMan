#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "../io/tape_device.h"
#include "ltfs_index.h"
#include "ltfs_label.h"

namespace qlto {

enum class LWStatus {
    NotReady,
    Busy,
    Succ,
    Err,
    Pause,
    Stopped
};

struct CapacityInfo {
    std::uint64_t bytes_total = 0;
    std::uint64_t bytes_used = 0;
    std::uint64_t bytes_free = 0;
};

struct WriteOptions {
    std::uint32_t block_len = 0;
    bool hash_on_write = false;
    std::uint32_t speed_limit_mib_s = 0; // 0 = unlimited
    std::uint64_t index_interval_bytes = 0; // 0 = disabled
    std::uint32_t capacity_interval_sec = 0; // 0 = disabled
    std::uint32_t clean_cycle = 0;
    std::uint8_t extra_partition_count = 0;
    bool offline_mode = false;
    std::vector<std::uint8_t> encryption_key;
    std::string auto_dump_path;
    bool force_index = false;
};

struct ServiceCallbacks {
    std::function<void(const std::string &)> log;
    std::function<void(double)> progress; // 0..1
    std::function<void(const std::string &)> status_text;
    std::function<void(const SenseData &)> sense;
    std::function<void(const CapacityInfo &)> capacity;
    std::function<void(LWStatus)> status_light;
};

class LtfsService {
public:
    virtual ~LtfsService() = default;

    virtual void set_callbacks(ServiceCallbacks cb) = 0;
    virtual bool attach_device(std::unique_ptr<TapeDevice> dev, std::string &err) = 0;

    virtual bool load_label(LtfsLabel &out, std::string &err) = 0;
    virtual bool load_index(LtfsIndex &out, bool offline, std::string &err) = 0;

    virtual bool write_files(const std::vector<LtfsFile> &files,
                             const WriteOptions &opts,
                             std::string &err) = 0;

    virtual bool write_index(bool force, std::string &err) = 0;
    virtual bool refresh_capacity(CapacityInfo &info, std::string &err) = 0;
    virtual bool eject(std::string &err) = 0;
    virtual bool flush(std::string &err) = 0;
};

} // namespace qlto
