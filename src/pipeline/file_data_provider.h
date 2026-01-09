#pragma once

#include <cstdint>
#include <functional>
#include <vector>

#include "file_record.h"

namespace qlto {

class FileDataProvider {
public:
    virtual ~FileDataProvider() = default;

    virtual void start() = 0;
    virtual void request_next_file() = 0;
    virtual void cancel() = 0;

    virtual bool completed() const = 0;
    virtual bool read_chunk(std::vector<std::uint8_t> &out) = 0;
};

} // namespace qlto
