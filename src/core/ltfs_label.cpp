#include "ltfs_label.h"

namespace qlto {

bool validate_label(const LtfsLabel &label, std::string &err) {
    if (label.blocksize == 0) {
        err = "blocksize must be > 0";
        return false;
    }
    if (label.partitions.index == label.partitions.data) {
        err = "index and data partition cannot be the same";
        return false;
    }
    if (!label.barcode.empty() && label.barcode.size() > 32) {
        err = "barcode too long";
        return false;
    }
    // basic success
    return true;
}

} // namespace qlto
