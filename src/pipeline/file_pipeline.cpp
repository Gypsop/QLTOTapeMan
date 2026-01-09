#include "file_data_provider.h"

#include <algorithm>
#include <condition_variable>
#include <deque>
#include <fstream>
#include <mutex>
#include <thread>

namespace qlto {

class FileDataProviderImpl : public FileDataProvider {
public:
    FileDataProviderImpl(std::vector<FileRecord> files, DataProviderOptions opts)
        : files_(std::move(files)), opts_(opts) {
        if (opts_.chunk_size == 0) opts_.chunk_size = 512 * 1024;
        if (opts_.ring_buffer_bytes == 0) opts_.ring_buffer_bytes = opts_.chunk_size * 2;
        if (opts_.small_file_threshold == 0) opts_.small_file_threshold = opts_.chunk_size / 4;
    }

    ~FileDataProviderImpl() override { cancel(); }

    void start() override {
        if (started_) return;
        started_ = true;
        worker_ = std::thread([this]() { this->run(); });
    }

    void request_next_file() override {
        if (!opts_.require_signal) return;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            proceed_ = true;
        }
        cv_control_.notify_one();
    }

    void cancel() override {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            cancelled_ = true;
        }
        cv_control_.notify_all();
        cv_data_.notify_all();
        if (worker_.joinable()) worker_.join();
    }

    bool completed() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return worker_done_ && queue_.empty();
    }

    bool read_chunk(std::vector<std::uint8_t> &out) override {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_data_.wait(lock, [this]() { return cancelled_ || !queue_.empty() || worker_done_; });
        if (cancelled_) return false;
        if (queue_.empty()) return false;
        out = std::move(queue_.front());
        queue_.pop_front();
        buffered_bytes_ = buffered_bytes_ >= out.size() ? buffered_bytes_ - out.size() : 0;
        cv_control_.notify_one();
        return true;
    }

private:
    void run() {
        for (const auto &rec : files_) {
            if (cancelled_) break;
            if (opts_.require_signal) {
                std::unique_lock<std::mutex> lock(mutex_);
                cv_control_.wait(lock, [this]() { return cancelled_ || proceed_; });
                proceed_ = false;
                if (cancelled_) break;
            }
            read_one_file(rec);
            if (cancelled_) break;
        }
        {
            std::lock_guard<std::mutex> lock(mutex_);
            worker_done_ = true;
        }
        cv_data_.notify_all();
    }

    void read_one_file(const FileRecord &rec) {
        const std::string &path = rec.source_path;
        if (rec.file.length == 0) return;

        if (!rec.buffer.empty()) {
            std::vector<std::uint8_t> buf = rec.buffer;
            if (buf.size() > rec.file.length) buf.resize(static_cast<std::size_t>(rec.file.length));
            push_chunk(std::move(buf));
            return;
        }

        if (path.empty()) return;
        std::ifstream ifs(path, std::ios::binary);
        if (!ifs) return;

        std::size_t threshold = opts_.small_file_threshold;
        if (rec.file.length <= threshold) {
            std::vector<std::uint8_t> buf(static_cast<std::size_t>(rec.file.length));
            ifs.read(reinterpret_cast<char *>(buf.data()), static_cast<std::streamsize>(buf.size()));
            std::size_t got = static_cast<std::size_t>(ifs.gcount());
            buf.resize(got);
            push_chunk(std::move(buf));
            return;
        }

        std::vector<std::uint8_t> buf(opts_.chunk_size);
        std::uint64_t remaining = rec.file.length;
        while (remaining > 0 && !cancelled_) {
            std::size_t to_read = static_cast<std::size_t>(std::min<std::uint64_t>(remaining, buf.size()));
            ifs.read(reinterpret_cast<char *>(buf.data()), static_cast<std::streamsize>(to_read));
            std::size_t got = static_cast<std::size_t>(ifs.gcount());
            if (got == 0) break;
            remaining -= got;
            push_chunk(std::vector<std::uint8_t>(buf.begin(), buf.begin() + got));
        }
    }

    void push_chunk(std::vector<std::uint8_t> &&chunk) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (opts_.ring_buffer_bytes > 0) {
            cv_control_.wait(lock, [this, &chunk]() {
                return cancelled_ || (buffered_bytes_ + chunk.size() <= opts_.ring_buffer_bytes);
            });
        }
        if (cancelled_) return;
        buffered_bytes_ += chunk.size();
        queue_.push_back(std::move(chunk));
        cv_data_.notify_one();
    }

    std::vector<FileRecord> files_;
    DataProviderOptions opts_{};

    mutable std::mutex mutex_;
    std::condition_variable cv_data_;
    std::condition_variable cv_control_;
    std::deque<std::vector<std::uint8_t>> queue_;
    std::size_t buffered_bytes_ = 0;
    bool cancelled_ = false;
    bool worker_done_ = false;
    bool proceed_ = false;
    bool started_ = false;
    std::thread worker_;
};

std::unique_ptr<FileDataProvider> make_file_data_provider(const std::vector<FileRecord> &files,
                                                          const DataProviderOptions &opts) {
    std::vector<FileRecord> copied = files;
    return std::make_unique<FileDataProviderImpl>(std::move(copied), opts);
}

} // namespace qlto
