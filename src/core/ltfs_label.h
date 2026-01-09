#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace qlto {

struct LtfsPartitions {
    std::uint8_t index = 0;  // typically partition 0
    std::uint8_t data = 1;   // typically partition 1
    std::uint8_t extra = 0;  // optional extra partitions count or id
};

struct EncryptionInfo {
    std::vector<std::uint8_t> key; // 32 bytes preferred
    bool enabled() const { return !key.empty(); }
};

struct LtfsLabel {
    std::string barcode;
    std::string volume_label;
    std::uint32_t blocksize = 0;
    LtfsPartitions partitions{};
    std::uint32_t generation = 0;
    bool worm = false;
    EncryptionInfo encryption{};
};
// Basic validation for label parameters.
bool validate_label(const LtfsLabel &label, std::string &err);

} // namespace qlto
