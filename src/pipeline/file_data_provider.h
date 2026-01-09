#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

#include "file_record.h"

namespace qlto {

struct DataProviderOptions {
    std::size_t chunk_size = 512 * 1024;          // bytes per read
    std::size_t ring_buffer_bytes = 256 * 1024 * 1024; // total buffered bytes
    std::size_t small_file_threshold = 16 * 1024; // cache whole file if size <= threshold
    bool require_signal = false;                  // if true, wait request_next_file per file
};

class FileDataProvider {
public:
    virtual ~FileDataProvider() = default;

    virtual void start() = 0;
    virtual void request_next_file() = 0;
    virtual void cancel() = 0;

    virtual bool completed() const = 0;
    virtual bool read_chunk(std::vector<std::uint8_t> &out) = 0;
};

std::unique_ptr<FileDataProvider> make_file_data_provider(const std::vector<FileRecord> &files,
                                                          const DataProviderOptions &opts = {});

} // namespace qlto
