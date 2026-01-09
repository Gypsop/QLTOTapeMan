#include "ltfs_service.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <optional>
#include <sstream>
#include <thread>
#include <utility>

namespace qlto {

namespace {

constexpr std::uint32_t kDefaultBlockSize = 524288; // 512 KiB
constexpr std::uint32_t kMillisPerSec = 1000;

std::string trim_string(const std::string &s) {
    auto start = s.find_first_not_of(" \t\r\n");
    auto end = s.find_last_not_of(" \t\r\n");
    if (start == std::string::npos || end == std::string::npos) return std::string();
    return s.substr(start, end - start + 1);
}

std::string mam_attr_to_string(const MamAttribute &attr) {
    return std::string(attr.data.begin(), attr.data.end());
}

std::string find_xattr(const LtfsFile &file, const std::string &key) {
    for (const auto &xa : file.extendedattributes) {
        if (xa.name == key) return xa.value;
    }
    return {};
}

std::uint32_t crc32_update(std::uint32_t crc, const std::uint8_t *data, std::size_t len) {
    static std::uint32_t table[256];
    static bool init = false;
    if (!init) {
        for (std::uint32_t i = 0; i < 256; ++i) {
            std::uint32_t c = i;
            for (int j = 0; j < 8; ++j) {
                c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            }
            table[i] = c;
        }
        init = true;
    }
    crc = crc ^ 0xFFFFFFFFu;
    for (std::size_t i = 0; i < len; ++i) {
        crc = table[(crc ^ data[i]) & 0xFFu] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFFu;
}

void apply_speed_limit(std::uint32_t limit_mib_s, std::uint64_t bytes_written_in_window,
                       std::chrono::steady_clock::time_point window_start) {
    if (limit_mib_s == 0) return;
    double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - window_start).count();
    if (seconds <= 0.0) return;
    double rate = (bytes_written_in_window / (1024.0 * 1024.0)) / seconds;
    if (rate > limit_mib_s) {
        double target_seconds = (bytes_written_in_window / (1024.0 * 1024.0)) / static_cast<double>(limit_mib_s);
        double sleep_s = target_seconds - seconds;
        if (sleep_s > 0) {
            std::this_thread::sleep_for(std::chrono::duration<double>(sleep_s));
        }
    }
}

} // namespace

class LtfsServiceImpl : public LtfsService {
public:
    void set_callbacks(ServiceCallbacks cb) override {
        std::lock_guard<std::mutex> lock(mutex_);
        callbacks_ = std::move(cb);
    }

    bool attach_device(std::unique_ptr<TapeDevice> dev, std::string &err) override {
        if (!dev) {
            err = "device is null";
            return false;
        }
        std::lock_guard<std::mutex> lock(mutex_);
        device_ = std::move(dev);
        current_label_.reset();
        current_index_.reset();
        emit_status("device attached");
        return true;
    }

    bool load_label(LtfsLabel &out, std::string &err) override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!device_) {
            err = "device not attached";
            return false;
        }
        SenseData sense{};
        MamAttribute barcode_attr{};
        if (!device_->read_mam(0x08, 0x06, barcode_attr, sense, err)) {
            emit_sense(sense);
            return false;
        }
        out.barcode = trim_string(mam_attr_to_string(barcode_attr));
        out.volume_label = out.barcode;
        out.blocksize = kDefaultBlockSize;
        out.partitions = {0, 1, 0};
        out.generation = 0;
        out.worm = false;
        out.encryption = {};
        current_label_ = out;
        emit_status("label loaded");
        return true;
    }

    bool load_index(LtfsIndex &out, bool offline, std::string &err) override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (offline) {
            auto path = std::getenv("QLTOTAPEMAN_SCHEMA_PATH");
            std::filesystem::path schema = path && *path ? path : std::filesystem::path("ltfsindex.xml");
            if (!std::filesystem::exists(schema)) {
                err = "schema file not found: " + schema.string();
                return false;
            }
            if (!load_schema(schema.string(), out, err)) {
                return false;
            }
            current_index_ = out;
            emit_status("offline index loaded");
            return true;
        }

        if (!device_) {
            err = "device not attached";
            return false;
        }
        SenseData sense{};
        std::uint32_t blk = current_label_ ? current_label_->blocksize : kDefaultBlockSize;
        std::vector<std::uint8_t> buffer;
        if (!device_->read_block(buffer, blk, sense, err)) {
            emit_sense(sense);
            return false;
        }
        std::filesystem::path tmp = std::filesystem::temp_directory_path() / "qltotapeman_index_tmp.xml";
        {
            std::ofstream ofs(tmp, std::ios::binary | std::ios::trunc);
            ofs.write(reinterpret_cast<const char *>(buffer.data()), static_cast<std::streamsize>(buffer.size()));
        }
        bool ok = load_schema(tmp.string(), out, err);
        std::error_code ec;
        std::filesystem::remove(tmp, ec);
        if (!ok) return false;
        current_index_ = out;
        emit_status("index loaded from device");
        return true;
    }

    bool write_files(const std::vector<LtfsFile> &files, const WriteOptions &opts, std::string &err) override {
        if (files.empty()) {
            err = "no files to write";
            return false;
        }
        if (opts.offline_mode) {
            err = "offline mode enabled; cannot write";
            return false;
        }
        std::lock_guard<std::mutex> lock(mutex_);
        if (!device_) {
            err = "device not attached";
            return false;
        }
        last_opts_ = opts;
        std::uint64_t total_bytes = 0;
        for (const auto &f : files) total_bytes += f.length;
        std::uint64_t written = 0;
        std::uint32_t block = opts.block_len ? opts.block_len : (current_label_ ? current_label_->blocksize : kDefaultBlockSize);
        SenseData sense{};
        std::uint64_t bytes_since_index = 0;
        auto window_start = std::chrono::steady_clock::now();
        std::uint64_t window_bytes = 0;

        for (const auto &f : files) {
            std::string src = find_xattr(f, "source_path");
            if (src.empty()) {
                err = "missing source_path xattr for file: " + f.name;
                return false;
            }
            std::ifstream ifs(src, std::ios::binary);
            if (!ifs) {
                err = "cannot open source file: " + src;
                return false;
            }
            std::vector<std::uint8_t> buf(block);
            std::uint64_t remaining = f.length;
            std::uint32_t crc = 0;

            PositionData pos_before{};
            if (!device_->read_position(pos_before, sense, err)) {
                emit_sense(sense);
                return false;
            }

            while (remaining > 0) {
                std::size_t to_read = static_cast<std::size_t>(std::min<std::uint64_t>(remaining, buf.size()));
                ifs.read(reinterpret_cast<char *>(buf.data()), static_cast<std::streamsize>(to_read));
                std::size_t got = static_cast<std::size_t>(ifs.gcount());
                if (got == 0) {
                    err = "unexpected EOF while reading " + src;
                    return false;
                }
                if (opts.hash_on_write) {
                    crc = crc32_update(crc, buf.data(), got);
                }
                apply_speed_limit(opts.speed_limit_mib_s, window_bytes, window_start);
                if (!device_->write_block(buf.data(), got, sense, err)) {
                    emit_sense(sense);
                    return false;
                }
                remaining -= got;
                written += got;
                window_bytes += got;
                bytes_since_index += got;
                emit_progress(total_bytes ? static_cast<double>(written) / static_cast<double>(total_bytes) : 1.0);

                auto now = std::chrono::steady_clock::now();
                if (std::chrono::duration_cast<std::chrono::milliseconds>(now - window_start).count() >= kMillisPerSec) {
                    window_start = now;
                    window_bytes = 0;
                }

                if (opts.index_interval_bytes && bytes_since_index >= opts.index_interval_bytes && current_index_) {
                    std::string tmp_err;
                    write_index(true, tmp_err);
                    bytes_since_index = 0;
                }
            }
            if (!device_->write_filemark(sense, err)) {
                emit_sense(sense);
                return false;
            }

            if (opts.capacity_interval_sec > 0) {
                CapacityInfo info{};
                std::string cap_err;
                refresh_capacity(info, cap_err);
            }

            if (opts.hash_on_write) {
                std::ostringstream oss;
                oss << "CRC32 for " << f.name << " = 0x" << std::hex << crc;
                emit_status(oss.str());
            }

            if (current_index_) {
                current_index_->root.contents.files.push_back(f);
                current_index_->root.contents.files.back().openforwrite = false;
                current_index_->root.contents.files.back().selected = false;
                current_index_->location.startblock = pos_before.block;
            }
        }
        emit_status("files written");
        return true;
    }

    bool write_index(bool force, std::string &err) override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!device_) {
            err = "device not attached";
            return false;
        }
        if (!current_index_) {
            err = "no index loaded";
            return false;
        }
        std::filesystem::path tmp = std::filesystem::temp_directory_path() / "qltotapeman_write_index.xml";
        if (!save_schema(*current_index_, tmp.string(), err)) {
            return false;
        }
        SenseData sense{};
        std::uint32_t block = current_label_ ? current_label_->blocksize : kDefaultBlockSize;
        std::ifstream ifs(tmp, std::ios::binary);
        std::vector<std::uint8_t> buf(block);
        while (ifs) {
            ifs.read(reinterpret_cast<char *>(buf.data()), static_cast<std::streamsize>(buf.size()));
            std::streamsize got = ifs.gcount();
            if (got <= 0) break;
            if (!device_->write_block(buf.data(), static_cast<std::size_t>(got), sense, err)) {
                emit_sense(sense);
                std::error_code ec;
                std::filesystem::remove(tmp, ec);
                return false;
            }
        }
        if (!device_->write_filemark(sense, err)) {
            emit_sense(sense);
            std::error_code ec;
            std::filesystem::remove(tmp, ec);
            return false;
        }
        std::error_code ec;
        if (!last_opts_.auto_dump_path.empty()) {
            std::filesystem::copy_file(tmp, last_opts_.auto_dump_path, std::filesystem::copy_options::overwrite_existing, ec);
        }
        std::filesystem::remove(tmp, ec);
        emit_status(force ? "index force-written" : "index written");
        return true;
    }

    bool refresh_capacity(CapacityInfo &info, std::string &err) override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!device_) {
            err = "device not attached";
            return false;
        }
        SenseData sense{};
        PositionData pos{};
        if (!device_->read_position(pos, sense, err)) {
            emit_sense(sense);
            return false;
        }
        std::uint32_t blk = current_label_ ? current_label_->blocksize : kDefaultBlockSize;
        info.bytes_used = static_cast<std::uint64_t>(pos.block) * blk;
        info.bytes_total = info.bytes_used; // unknown total; best-effort
        info.bytes_free = 0;
        emit_capacity(info);
        return true;
    }

    bool eject(std::string &err) override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!device_) {
            err = "device not attached";
            return false;
        }
        SenseData sense{};
        if (!device_->unload(sense, err)) {
            emit_sense(sense);
            return false;
        }
        emit_status("tape unloaded");
        return true;
    }

    bool flush(std::string &err) override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!device_) {
            err = "device not attached";
            return false;
        }
        // No explicit flush command available; treat as successful.
        emit_status("flush complete");
        return true;
    }

private:
    void emit_status(const std::string &msg) {
        if (callbacks_.status_text) callbacks_.status_text(msg);
        if (callbacks_.log) callbacks_.log(msg);
    }

    void emit_progress(double p) {
        if (callbacks_.progress) callbacks_.progress(p);
    }

    void emit_sense(const SenseData &s) {
        if (callbacks_.sense) callbacks_.sense(s);
    }

    void emit_capacity(const CapacityInfo &c) {
        if (callbacks_.capacity) callbacks_.capacity(c);
    }

    std::unique_ptr<TapeDevice> device_;
    ServiceCallbacks callbacks_{};
    std::optional<LtfsLabel> current_label_;
    std::optional<LtfsIndex> current_index_;
    std::mutex mutex_;
    WriteOptions last_opts_{};
};

// Factory helper for clients
std::unique_ptr<LtfsService> make_ltfs_service() {
    return std::make_unique<LtfsServiceImpl>();
}

} // namespace qlto
