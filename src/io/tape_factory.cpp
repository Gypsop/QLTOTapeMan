#include "tape_factory.h"

#include <memory>
#include <string>

#include "tape_device.h"
#include "tape_enumerator.h"

#if defined(QLTO_PLATFORM_LINUX)
#include "tape_device_linux.h"
#include "tape_enumerator_linux.h"
#elif defined(QLTO_PLATFORM_WIN)
#include "tape_device_win.h"
#include "tape_enumerator_win.h"
#elif defined(QLTO_PLATFORM_MAC)
#include "tape_device_mac.h"
#include "tape_enumerator_mac.h"
#endif

namespace qlto {

std::unique_ptr<TapeDevice> make_default_device(std::string &err) {
#if defined(QLTO_PLATFORM_LINUX)
    return std::make_unique<TapeDeviceLinux>();
#elif defined(QLTO_PLATFORM_WIN)
    return std::make_unique<TapeDeviceWin>();
#elif defined(QLTO_PLATFORM_MAC)
    return std::make_unique<TapeDeviceMac>();
#else
    err = "No platform tape device available";
    return nullptr;
#endif
}

std::unique_ptr<TapeEnumerator> make_default_enumerator(std::string &err) {
#if defined(QLTO_PLATFORM_LINUX)
    return std::make_unique<TapeEnumeratorLinux>();
#elif defined(QLTO_PLATFORM_WIN)
    return std::make_unique<TapeEnumeratorWin>();
#elif defined(QLTO_PLATFORM_MAC)
    return std::make_unique<TapeEnumeratorMac>();
#else
    err = "No platform enumerator available";
    return nullptr;
#endif
}

} // namespace qlto
