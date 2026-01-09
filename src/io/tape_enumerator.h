#pragma once

#include <string>
#include <vector>

namespace qlto {

struct BlockDevice {
    std::string vendor;
    std::string product;
    std::string serial;
    std::string device_path;
    std::string display_name;
    std::string os_driver;
    int index = -1;
};

class TapeEnumerator {
public:
    virtual ~TapeEnumerator() = default;
    virtual std::vector<BlockDevice> list(std::string &err) = 0;
};

} // namespace qlto
