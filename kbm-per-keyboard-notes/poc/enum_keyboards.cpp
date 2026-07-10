// PoC: enumerate connected keyboards via Raw Input, print stable identifiers.
// No window/message loop required — GetRawInputDeviceList works standalone.
// Goal: prove we can (a) list each physical keyboard, (b) get VID/PID +
// device path + product/manufacturer strings, so a per-keyboard profile
// map can key off a stable identifier. (feature/kbm-per-keyboard-remap)
//
// Build (Developer PowerShell): cl /utf-8 /EHsc /nologo enum_keyboards.cpp user32.lib hid.lib
// Run:                          .\enum_keyboards.exe

#include <windows.h>
#include <hidsdi.h>
#include <cwchar>
#include <cwctype>
#include <cstdio>
#include <string>
#include <vector>

static std::wstring ToUpper(std::wstring s)
{
    for (auto& c : s) c = towupper(c);
    return s;
}

// Pull the VID/PID out of a raw-input device path. Two formats occur:
//   USB:       \\?\HID#VID_046D&PID_C232#...            -> "VID_" + 4 hex
//   Bluetooth: \\?\HID#{...}_VID&000205ac_PID&0239_...  -> "VID&" + 8 hex (last 4 = real VID)
// `key` is L"VID" or L"PID". Returns empty if not present.
static std::wstring ExtractToken(const std::wstring& path, const wchar_t* key)
{
    const std::wstring up = ToUpper(path);
    for (const wchar_t sep : { L'_', L'&' })
    {
        std::wstring needle = key; needle += sep;
        size_t pos = up.find(needle);
        if (pos == std::wstring::npos) continue;
        pos += needle.size();
        std::wstring hex;
        while (pos < up.size() && iswxdigit(up[pos])) hex += up[pos++];
        if (hex.empty()) continue;
        // BT VID is "0002" (source) + real 4-hex VID; keep the last 4.
        return hex.size() == 8 ? hex.substr(4) : hex;
    }
    return L"";
}

// Try to read a HID string (product/manufacturer) from the device path.
static std::wstring HidString(const std::wstring& path, bool product)
{
    HANDLE h = CreateFileW(path.c_str(), 0,
                           FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                           OPEN_EXISTING, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE) return L"(cannot open)";
    wchar_t buf[256] = {};
    BOOLEAN ok = product ? HidD_GetProductString(h, buf, sizeof(buf))
                         : HidD_GetManufacturerString(h, buf, sizeof(buf));
    CloseHandle(h);
    if (!ok || buf[0] == 0) return L"(none)";
    return buf;
}

int wmain()
{
    UINT count = 0;
    if (GetRawInputDeviceList(nullptr, &count, sizeof(RAWINPUTDEVICELIST)) != 0 || count == 0)
    {
        wprintf(L"No raw input devices (or query failed).\n");
        return 1;
    }
    std::vector<RAWINPUTDEVICELIST> list(count);
    count = GetRawInputDeviceList(list.data(), &count, sizeof(RAWINPUTDEVICELIST));
    if (count == (UINT)-1)
    {
        wprintf(L"GetRawInputDeviceList failed: %lu\n", GetLastError());
        return 1;
    }

    int kbIndex = 0;
    for (const auto& dev : list)
    {
        if (dev.dwType != RIM_TYPEKEYBOARD) continue;

        // Device interface path (stable-ish per device model/instance).
        UINT size = 0;
        GetRawInputDeviceInfoW(dev.hDevice, RIDI_DEVICENAME, nullptr, &size);
        std::wstring path(size ? size - 1 : 0, L'\0');
        if (size) GetRawInputDeviceInfoW(dev.hDevice, RIDI_DEVICENAME, &path[0], &size);

        // Hardware descriptor.
        RID_DEVICE_INFO info = {};
        info.cbSize = sizeof(info);
        UINT infoSize = sizeof(info);
        GetRawInputDeviceInfoW(dev.hDevice, RIDI_DEVICEINFO, &info, &infoSize);

        const std::wstring vid = ExtractToken(path, L"VID");
        const std::wstring pid = ExtractToken(path, L"PID");
        const bool isRdp = path.find(L"Root#RDP_KBD") != std::wstring::npos ||
                           path.find(L"ROOT#RDP_KBD") != std::wstring::npos;

        wprintf(L"[Keyboard #%d]%s\n", kbIndex++, isRdp ? L" (virtual/RDP)" : L"");
        wprintf(L"  VID/PID   : %s / %s\n",
                vid.empty() ? L"(n/a)" : vid.c_str(),
                pid.empty() ? L"(n/a)" : pid.c_str());
        wprintf(L"  Product   : %s\n", HidString(path, true).c_str());
        wprintf(L"  Vendor    : %s\n", HidString(path, false).c_str());
        if (info.dwType == RIM_TYPEKEYBOARD)
        {
            wprintf(L"  HW type   : type=%lu subtype=%lu keys=%lu funckeys=%lu\n",
                    info.keyboard.dwType, info.keyboard.dwSubType,
                    info.keyboard.dwNumberOfKeysTotal,
                    info.keyboard.dwNumberOfFunctionKeys);
        }
        wprintf(L"  Path      : %s\n\n", path.c_str());
    }

    wprintf(L"Total keyboards enumerated: %d\n", kbIndex);
    wprintf(L"(Apple VID=05AC / 004C indicates a Mac keyboard.)\n");
    return 0;
}
