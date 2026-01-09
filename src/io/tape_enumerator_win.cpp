#include "tape_enumerator_win.h"

#include <windows.h>
#include <initguid.h>
#include <setupapi.h>

#include <string>
#include <vector>

#ifndef GUID_DEVINTERFACE_TAPE
// {53f5630b-b6bf-11d0-94f2-00a0c91efb8b}
DEFINE_GUID(GUID_DEVINTERFACE_TAPE, 0x53f5630b, 0xb6bf, 0x11d0, 0x94, 0xf2, 0x00, 0xa0, 0xc9, 0x1e, 0xfb, 0x8b);
#endif

namespace qlto {

std::vector<BlockDevice> TapeEnumeratorWin::list(std::string &err) {
    std::vector<BlockDevice> out;
    HDEVINFO hDevInfo = SetupDiGetClassDevs(&GUID_DEVINTERFACE_TAPE, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (hDevInfo == INVALID_HANDLE_VALUE) {
        err = "SetupDiGetClassDevs failed";
        return out;
    }

    SP_DEVICE_INTERFACE_DATA ifData{};
    ifData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

    for (DWORD i = 0;; ++i) {
        if (!SetupDiEnumDeviceInterfaces(hDevInfo, nullptr, &GUID_DEVINTERFACE_TAPE, i, &ifData)) {
            if (GetLastError() == ERROR_NO_MORE_ITEMS) break;
            continue;
        }
        DWORD required = 0;
        SetupDiGetDeviceInterfaceDetailW(hDevInfo, &ifData, nullptr, 0, &required, nullptr);
        std::vector<std::uint8_t> buffer(required);
        auto *detail = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W *>(buffer.data());
        detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);
        SP_DEVINFO_DATA devInfo{};
        devInfo.cbSize = sizeof(SP_DEVINFO_DATA);
        if (!SetupDiGetDeviceInterfaceDetailW(hDevInfo, &ifData, detail, required, nullptr, &devInfo)) {
            continue;
        }
        std::wstring wpath(detail->DevicePath);
        std::string path(wpath.begin(), wpath.end());

        BlockDevice dev{};
        dev.device_path = path;
        dev.display_name = path;
        dev.index = static_cast<int>(i);

        wchar_t bufferStr[256];
        if (SetupDiGetDeviceRegistryPropertyW(hDevInfo, &devInfo, SPDRP_MFG, nullptr, reinterpret_cast<PBYTE>(bufferStr), sizeof(bufferStr), nullptr)) {
            std::wstring w(bufferStr);
            dev.vendor = std::string(w.begin(), w.end());
        }
        if (SetupDiGetDeviceRegistryPropertyW(hDevInfo, &devInfo, SPDRP_FRIENDLYNAME, nullptr, reinterpret_cast<PBYTE>(bufferStr), sizeof(bufferStr), nullptr)) {
            std::wstring w(bufferStr);
            dev.product = std::string(w.begin(), w.end());
            dev.display_name = dev.product;
        }
        out.push_back(std::move(dev));
    }

    SetupDiDestroyDeviceInfoList(hDevInfo);
    return out;
}

} // namespace qlto
