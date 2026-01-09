#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace qlto {

struct LtfsXAttr {
    std::string name;
    std::string value;
};

struct LtfsFile {
    std::string name;
    std::int64_t fileuid = -1;
    std::uint64_t length = 0;
    bool readonly = false;
    bool openforwrite = false;
    bool selected = false;

    std::string creationtime; // ISO-8601 UTC string
    std::string modifytime;
    std::string accesstime;
    std::string changetime;
    std::string backuptime;

    std::vector<LtfsXAttr> extendedattributes;
};

struct LtfsDirectory;

struct LtfsContents {
    std::vector<LtfsDirectory> directories;
    std::vector<LtfsFile> files;
};

struct LtfsDirectory {
    std::string name;
    bool selected = false;
    LtfsContents contents;
};

struct LtfsLocation {
    std::uint8_t partition = 0; // a or b
    std::uint64_t startblock = 0;
};

struct LtfsIndex {
    LtfsDirectory root; // corresponds to schema._directory
    LtfsLocation location{};
    std::uint32_t generationnumber = 0;
    std::string volumeuuid;
};

// Serialization helpers
bool load_schema(const std::string &path, LtfsIndex &out, std::string &err);
bool save_schema(const LtfsIndex &idx, const std::string &path, std::string &err);

// Selection helpers
void select_all(LtfsDirectory &dir, bool selected);
void select_by_predicate(LtfsDirectory &dir, const std::function<bool(const LtfsFile &)> &pred);

// Sorting helpers
void sort_files(LtfsDirectory &dir, const std::function<bool(const LtfsFile &, const LtfsFile &)> &cmp);

// Utility
std::string sanitize_filename(const std::string &name);

} // namespace qlto
