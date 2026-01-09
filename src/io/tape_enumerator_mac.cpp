#include "tape_enumerator_mac.h"

#include <IOKit/IOKitLib.h>
#include <IOKit/storage/IOTape.h>
#include <IOKit/scsi/SCSITaskLib.h>
#include <CoreFoundation/CoreFoundation.h>

namespace qlto {

std::vector<BlockDevice> TapeEnumeratorMac::list(std::string &err) {
    std::vector<BlockDevice> out;
    CFMutableDictionaryRef matching = IOServiceMatching(kIOSCSITapeClassName);
    if (!matching) {
        err = "Failed to create matching dictionary";
        return out;
    }

    io_iterator_t iter{};
    if (IOServiceGetMatchingServices(kIOMainPortDefault, matching, &iter) != KERN_SUCCESS) {
        err = "Failed to enumerate tape devices";
        return out;
    }

    io_object_t obj;
    while ((obj = IOIteratorNext(iter))) {
        BlockDevice info{};
        CFStringRef vendor = (CFStringRef)IORegistryEntryCreateCFProperty(obj, CFSTR("Vendor Identification"), kCFAllocatorDefault, 0);
        CFStringRef product = (CFStringRef)IORegistryEntryCreateCFProperty(obj, CFSTR("Product Identification"), kCFAllocatorDefault, 0);
        CFStringRef revision = (CFStringRef)IORegistryEntryCreateCFProperty(obj, CFSTR("Product Revision Level"), kCFAllocatorDefault, 0);
        CFStringRef bsd = (CFStringRef)IORegistryEntryCreateCFProperty(obj, CFSTR(kIOBSDNameKey), kCFAllocatorDefault, 0);

        auto cf_to_str = [](CFStringRef s) {
            std::string res;
            if (!s) return res;
            char buf[256]{};
            if (CFStringGetCString(s, buf, sizeof(buf), kCFStringEncodingUTF8)) {
                res = buf;
            }
            return res;
        };

        info.vendor = cf_to_str(vendor);
        info.product = cf_to_str(product);
        info.os_driver = cf_to_str(revision);
        if (bsd) {
            info.device_path = "/dev/" + cf_to_str(bsd);
            info.display_name = info.device_path;
        }

        if (vendor) CFRelease(vendor);
        if (product) CFRelease(product);
        if (revision) CFRelease(revision);
        if (bsd) CFRelease(bsd);
        IOObjectRelease(obj);

        out.push_back(std::move(info));
    }
    IOObjectRelease(iter);
    return out;
}

} // namespace qlto
