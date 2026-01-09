#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "../core/ltfs_index.h"

namespace qlto {

struct FileRecord {
    const LtfsDirectory *parent_directory = nullptr;
    LtfsFile file; // copy of target file metadata
    std::string source_path;
    std::vector<std::uint8_t> buffer; // optional in-memory cache

    bool is_opened = false;
};

} // namespace qlto
