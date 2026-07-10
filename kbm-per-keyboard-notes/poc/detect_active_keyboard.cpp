// PoC: detect WHICH physical keyboard is being typed on, in real time.
// This is the auto-switch trigger for per-keyboard profiles: on each keydown
// we learn the source device (hDevice) via Raw Input, and print a banner
// whenever the active keyboard changes -> that's where a profile switch fires.
//
// Needs a window to receive WM_INPUT (a thread-only message loop, like the KBM
// engine's, does NOT get WM_INPUT). We use a hidden top-level window +
// RIDEV_INPUTSINK so keystrokes are seen even without focus.
//
// Build (Developer PowerShell): cl /utf-8 /EHsc /std:c++17 detect_active_keyboard.cpp user32.lib hid.lib
// Run:                          .\detect_active_keyboard.exe   (then type on each keyboard; Esc to quit)

#include <windows.h>
#include <hidsdi.h>
#include <cwctype>
#include <cstdio>
#include <string>
#include <unordered_map>

static std::wstring ToUpper(std::wstring s) { for (auto& c : s) c = towupper(c); return s; }

// Extract VID/PID from a raw-input device path (USB "VID_" or Bluetooth "VID&").
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
        return hex.size() == 8 ? hex.substr(4) : hex;   // BT: last 4 hex = real VID
    }
    return L"";
}

static std::wstring HidProduct(const std::wstring& path)
{
    HANDLE h = CreateFileW(path.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE,
                           nullptr, OPEN_EXISTING, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE) return L"";
    wchar_t buf[256] = {};
    BOOLEAN ok = HidD_GetProductString(h, buf, sizeof(buf));
    CloseHandle(h);
    return (ok && buf[0]) ? buf : L"";
}

struct DevInfo { std::wstring label; std::wstring path; };

// Read the raw-input device path (this is our proposed STABLE identity key).
static std::wstring DevicePath(HANDLE hDevice)
{
    UINT size = 0;
    GetRawInputDeviceInfoW(hDevice, RIDI_DEVICENAME, nullptr, &size);
    std::wstring path(size ? size - 1 : 0, L'\0');
    if (size) GetRawInputDeviceInfoW(hDevice, RIDI_DEVICENAME, &path[0], &size);
    return path;
}

// Build a friendly, stable label + keep the raw path.
static DevInfo InfoFor(HANDLE hDevice)
{
    if (hDevice == nullptr) return { L"(synthetic/injected input, hDevice=NULL)", L"" };

    DevInfo di;
    di.path = DevicePath(hDevice);
    const std::wstring vid = ExtractToken(di.path, L"VID");
    const std::wstring pid = ExtractToken(di.path, L"PID");
    const std::wstring product = HidProduct(di.path);

    if (!product.empty()) di.label = product + (vid.empty() ? L"" : L"  [VID " + vid + L"/PID " + pid + L"]");
    else if (!vid.empty()) di.label = L"VID " + vid + L"/PID " + pid;
    else di.label = L"(no VID/product — identify by path)";
    return di;
}

static std::unordered_map<HANDLE, DevInfo> g_labelCache;
static HANDLE g_lastDevice = reinterpret_cast<HANDLE>(-1);

static void OnKeyboardInput(HRAWINPUT hInput)
{
    UINT size = 0;
    GetRawInputData(hInput, RID_INPUT, nullptr, &size, sizeof(RAWINPUTHEADER));
    if (size == 0) return;
    std::string buf(size, 0);
    if (GetRawInputData(hInput, RID_INPUT, buf.data(), &size, sizeof(RAWINPUTHEADER)) != size) return;

    RAWINPUT* ri = reinterpret_cast<RAWINPUT*>(buf.data());
    if (ri->header.dwType != RIM_TYPEKEYBOARD) return;

    const RAWKEYBOARD& kb = ri->data.keyboard;
    // Only react to key-DOWN (ignore key-up and the 0xFF overrun marker).
    const bool keyDown = (kb.Message == WM_KEYDOWN || kb.Message == WM_SYSKEYDOWN);
    if (!keyDown || kb.VKey == 0xFF) return;

    HANDLE dev = ri->header.hDevice;
    auto it = g_labelCache.find(dev);
    if (it == g_labelCache.end())
        it = g_labelCache.emplace(dev, InfoFor(dev)).first;
    const DevInfo& di = it->second;

    if (dev != g_lastDevice)
    {
        wprintf(L"\n==================================================================\n");
        wprintf(L">>> ACTIVE KEYBOARD -> %s\n", di.label.c_str());
        wprintf(L"    stable id (RIDI_DEVICENAME path):\n      %s\n",
                di.path.empty() ? L"(empty!)" : di.path.c_str());
        wprintf(L"    (this is where a per-keyboard profile switch would fire)\n");
        wprintf(L"==================================================================\n");
        g_lastDevice = dev;
    }
    wprintf(L"   key vk=0x%02X sc=0x%02X on: %s\n", kb.VKey, kb.MakeCode, di.label.c_str());
    if (kb.VKey == VK_ESCAPE) PostQuitMessage(0);
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_INPUT) { OnKeyboardInput(reinterpret_cast<HRAWINPUT>(lp)); return 0; }
    if (msg == WM_DESTROY) { PostQuitMessage(0); return 0; }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

int wmain()
{
    setvbuf(stdout, nullptr, _IONBF, 0);   // unbuffered so output shows immediately when piped

    const wchar_t* cls = L"KbmPerKeyboardPoc";
    WNDCLASSW wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = cls;
    RegisterClassW(&wc);

    // Hidden top-level window (not message-only: RIDEV_INPUTSINK is most reliable
    // with a real window). Never shown.
    HWND hwnd = CreateWindowExW(0, cls, L"", 0, 0, 0, 0, 0,
                                nullptr, nullptr, wc.hInstance, nullptr);
    if (!hwnd) { wprintf(L"CreateWindow failed: %lu\n", GetLastError()); return 1; }

    RAWINPUTDEVICE rid = {};
    rid.usUsagePage = 0x01;          // Generic Desktop
    rid.usUsage = 0x06;              // Keyboard
    rid.dwFlags = RIDEV_INPUTSINK;   // receive input even when not focused
    rid.hwndTarget = hwnd;
    if (!RegisterRawInputDevices(&rid, 1, sizeof(rid)))
    {
        wprintf(L"RegisterRawInputDevices failed: %lu\n", GetLastError());
        return 1;
    }

    wprintf(L"Listening for keystrokes. Type on EACH keyboard in turn.\n");
    wprintf(L"A banner prints when the active keyboard changes. Press Esc to quit.\n");

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return 0;
}
