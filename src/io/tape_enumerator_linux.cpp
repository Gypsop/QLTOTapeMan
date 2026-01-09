#include "tape_enumerator_linux.h"

#include <dirent.h>
#include <sys/stat.h>

#include <algorithm>
#include <fstream>

namespace qlto {

namespace {
std::string read_first_line(const std::string &path) {
    std::ifstream ifs(path);
    std::string line;
    std::getline(ifs, line);
    return line;
}
}

std::vector<BlockDevice> TapeEnumeratorLinux::list(std::string &err) {
    std::vector<BlockDevice> devices;
    DIR *d = ::opendir("/dev");
    if (!d) {
        err = "cannot open /dev";
        return devices;
    }
    struct dirent *ent;
    while ((ent = ::readdir(d)) != nullptr) {
        std::string name = ent->d_name;
        if (name.rfind("sg", 0) == 0) {
            BlockDevice dev{};
            dev.device_path = "/dev/" + name;
            dev.display_name = dev.device_path;
            dev.index = std::stoi(name.substr(2));
            // Try to read sysfs info
            std::string sys_base = "/sys/class/scsi_generic/" + name;
            dev.vendor = read_first_line(sys_base + "/device/vendor");
            dev.product = read_first_line(sys_base + "/device/model");
            dev.serial = read_first_line(sys_base + "/device/scsi_tape/" + name + "/../../serial");
            devices.push_back(std::move(dev));
        }
    }
    ::closedir(d);
    std::sort(devices.begin(), devices.end(), [](const BlockDevice &a, const BlockDevice &b) { return a.index < b.index; });
    return devices;
}

} // namespace qlto
