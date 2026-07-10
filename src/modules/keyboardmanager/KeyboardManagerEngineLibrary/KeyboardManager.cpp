#include "pch.h"
#include "KeyboardManager.h"
#include <interface/powertoy_module_interface.h>
#include <common/SettingsAPI/settings_objects.h>
#include <common/SettingsAPI/settings_helpers.h>
#include <common/interop/shared_constants.h>
#include <common/debug_control.h>
#include <common/utils/winapi_error.h>
#include <common/utils/json.h>
#include <common/logger/logger_settings.h>

#include <keyboardmanager/common/KeyboardManagerConstants.h>
#include <keyboardmanager/common/Helpers.h>
#include <keyboardmanager/common/KeyboardEventHandlers.h>
#include <ctime>

#include "KeyboardEventHandlers.h"
#include "trace.h"

HHOOK KeyboardManager::hookHandleCopy;
HHOOK KeyboardManager::hookHandle;
KeyboardManager* KeyboardManager::keyboardManagerObjectPtr;

namespace
{
    DWORD mainThreadId = {};
}

KeyboardManager::KeyboardManager()
{
    mainThreadId = GetCurrentThreadId();

    // Load the initial settings.
    LoadSettings();

    // Set the static pointer to the newest object of the class
    keyboardManagerObjectPtr = this;

    std::filesystem::path modulePath(PTSettingsHelper::get_module_save_folder_location(moduleName));
    auto changeSettingsCallback = [this](DWORD err) {
        Logger::trace(L"{} event was signaled", KeyboardManagerConstants::SettingsEventName);
        if (err != ERROR_SUCCESS)
        {
            Logger::error(L"Failed to watch settings changes. {}", get_last_error_or_default(err));
        }

        loadingSettings = true;
        bool loadedSuccessfully = false;
        try
        {
            LoadSettings();
            loadedSuccessfully = true;
        }
        catch (...)
        {
            Logger::error("Failed to load settings");
        }

        loadingSettings = false;

        if (!loadedSuccessfully)
            return;

        const bool newHasRemappings = HasRegisteredRemappingsUnchecked();
        // We didn't have any bindings before and we have now
        if (newHasRemappings && !hookHandle)
            PostThreadMessageW(mainThreadId, StartHookMessageID, 0, 0);

        // All bindings were removed
        if (!newHasRemappings && hookHandle)
            StopLowlevelKeyboardHook();
    };

    editorIsRunningEvent = CreateEvent(nullptr, true, false, KeyboardManagerConstants::EditorWindowEventName.c_str());
    settingsEventWaiter.start(KeyboardManagerConstants::SettingsEventName, changeSettingsCallback);

    // Start detecting which physical keyboard is being typed on (for per-keyboard profile switching).
    rawInputTracker = std::make_unique<RawInputKeyboardTracker>(
        [this](const RawInputKeyboardTracker::KeyEvent& keyEvent) { OnRawKeyEvent(keyEvent); });
    rawInputTracker->Start();
}

void KeyboardManager::OnRawKeyEvent(const RawInputKeyboardTracker::KeyEvent& keyEvent)
{
    // Injected input (hDevice == NULL) includes KBM's own remap output; never drive switching on it.
    if (keyEvent.injected || keyEvent.devicePath.empty())
    {
        return;
    }

    // Track held keys so we don't switch profiles in the middle of a chord.
    if (keyEvent.keyDown)
    {
        pressedKeys.insert(keyEvent.vkey);
    }
    else
    {
        pressedKeys.erase(keyEvent.vkey);
        return; // decisions are made on key-down only
    }

    // Log the active keyboard when it changes (helps discover device paths for the profile map).
    if (keyEvent.devicePath != lastSeenDevice)
    {
        lastSeenDevice = keyEvent.devicePath;
        Logger::trace(L"Detected keyboard: {}", keyEvent.devicePath);
    }

    if (!autoSwitchEnabled.load())
    {
        return;
    }

    // Which profile is this keyboard bound to? Unmapped keyboards keep the current profile.
    std::wstring target;
    {
        std::lock_guard<std::mutex> lock(deviceMapMutex);
        auto it = deviceProfileMap.find(keyEvent.devicePath);
        if (it == deviceProfileMap.end())
        {
            return;
        }

        target = it->second;
    }

    std::wstring current;
    {
        std::lock_guard<std::mutex> lock(activeProfileMutex);
        current = activeProfileName;
    }

    if (target == current)
    {
        // Already on the right profile; reset any pending switch.
        pendingTarget.clear();
        pendingCount = 0;
        requestedProfile.clear();
        return;
    }

    // A switch to this profile was already requested; wait for the reload to take effect.
    if (target == requestedProfile)
    {
        return;
    }

    // Defer switching while another key is held down (avoid breaking a combo mid-press).
    if (pressedKeys.size() > 1)
    {
        return;
    }

    // Hysteresis: require a few consecutive clean keystrokes on the new keyboard before switching.
    if (target == pendingTarget)
    {
        ++pendingCount;
    }
    else
    {
        pendingTarget = target;
        pendingCount = 1;
    }

    if (pendingCount < AutoSwitchThreshold)
    {
        return;
    }

    pendingCount = 0;
    requestedProfile = target;
    Logger::trace(L"Auto-switch: keyboard {} -> profile '{}'", keyEvent.devicePath, target);
    SwitchActiveProfile(target);
}

void KeyboardManager::LoadDeviceProfiles()
{
    bool enabled = false;
    std::unordered_map<std::wstring, std::wstring> map;

    try
    {
        const auto path = PTSettingsHelper::get_module_save_folder_location(moduleName) + L"\\deviceProfiles.json";
        auto parsed = json::from_file(path);
        if (parsed.has_value())
        {
            const json::JsonObject& obj = parsed.value();
            if (obj.HasKey(L"autoSwitchEnabled"))
            {
                enabled = obj.GetNamedBoolean(L"autoSwitchEnabled", false);
            }

            if (obj.HasKey(L"map"))
            {
                const auto arr = obj.GetNamedArray(L"map");
                for (uint32_t i = 0; i < arr.Size(); ++i)
                {
                    const auto entry = arr.GetObjectAt(i);
                    std::wstring device{ entry.GetNamedString(L"device", L"") };
                    std::wstring profile{ entry.GetNamedString(L"profile", L"") };
                    if (!device.empty() && !profile.empty())
                    {
                        map[device] = profile;
                    }
                }
            }
        }
    }
    catch (...)
    {
        Logger::error(L"Failed to load deviceProfiles.json");
    }

    {
        std::lock_guard<std::mutex> lock(deviceMapMutex);
        deviceProfileMap = std::move(map);
    }

    autoSwitchEnabled.store(enabled);
}

void KeyboardManager::SwitchActiveProfile(const std::wstring& profile)
{
    try
    {
        const auto path = PTSettingsHelper::get_module_save_folder_location(moduleName) + L"\\settings.json";
        auto parsed = json::from_file(path);
        json::JsonObject root = parsed.has_value() ? parsed.value() : json::JsonObject{};

        json::JsonObject properties = root.HasKey(L"properties") ? root.GetNamedObject(L"properties") : json::JsonObject{};

        json::JsonObject activeConfiguration;
        activeConfiguration.SetNamedValue(L"value", json::JsonValue::CreateStringValue(profile));
        properties.SetNamedValue(KeyboardManagerConstants::ActiveConfigurationSettingName, activeConfiguration);
        root.SetNamedValue(L"properties", properties);

        json::to_file(path, root);
    }
    catch (...)
    {
        Logger::error(L"Failed to write activeConfiguration for auto-switch");
        return;
    }

    // Reuse the existing reload path: the engine's own settings watcher will apply the new profile.
    HANDLE hEvent = CreateEvent(nullptr, false, false, KeyboardManagerConstants::SettingsEventName.c_str());
    if (hEvent)
    {
        SetEvent(hEvent);
        CloseHandle(hEvent);
    }
    else
    {
        Logger::error(L"Auto-switch: failed to signal settings event");
    }
}

void KeyboardManager::LoadSettings()
{
    bool loadedSuccessful = state.LoadSettings();
    if (!loadedSuccessful)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        // retry once
        state.LoadSettings();
    }

    // Track the active profile (for auto-switch decisions) and refresh the device->profile map.
    {
        std::lock_guard<std::mutex> lock(activeProfileMutex);
        activeProfileName = state.currentConfig;
    }
    LoadDeviceProfiles();

    try
    {
        // Send telemetry about configured key/shortcut to key/shortcut mappings, OS an app specific level.
        Trace::SendKeyAndShortcutRemapLoadedConfiguration(state);
    }
    catch (...)
    {
        try
        {
            Logger::error("Failed to send telemetry for the configured remappings.");
            // Try not to crash the app sending telemetry. Everything inside a try.
            Trace::ErrorSendingKeyAndShortcutRemapLoadedConfiguration();
        }
        catch (...)
        {

        }
    }
}

LRESULT CALLBACK KeyboardManager::HookProc(int nCode, const WPARAM wParam, const LPARAM lParam)
{
    LowlevelKeyboardEvent event{};
    if (nCode == HC_ACTION)
    {
        event.lParam = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
        event.wParam = wParam;
        event.lParam->vkCode = Helpers::EncodeKeyNumpadOrigin(event.lParam->vkCode, event.lParam->flags & LLKHF_EXTENDED);

        if (keyboardManagerObjectPtr->HandleKeyboardHookEvent(&event) == 1)
        {
            // Reset Num Lock whenever a NumLock key down event is suppressed since Num Lock key state change occurs before it is intercepted by low level hooks
            if (event.lParam->vkCode == VK_NUMLOCK && (event.wParam == WM_KEYDOWN || event.wParam == WM_SYSKEYDOWN) && event.lParam->dwExtraInfo != KeyboardManagerConstants::KEYBOARDMANAGER_SUPPRESS_FLAG)
            {
                KeyboardEventHandlers::SetNumLockToPreviousState(keyboardManagerObjectPtr->inputHandler);
            }
            return 1;
        }
    }

    return CallNextHookEx(hookHandleCopy, nCode, wParam, lParam);
}

void KeyboardManager::StartLowlevelKeyboardHook()
{
#if defined(DISABLE_LOWLEVEL_HOOKS_WHEN_DEBUGGED)
    if (IsDebuggerPresent())
    {
        return;
    }
#endif

    if (!hookHandle)
    {
        hookHandle = SetWindowsHookEx(WH_KEYBOARD_LL, HookProc, GetModuleHandle(NULL), NULL);
        hookHandleCopy = hookHandle;
        if (!hookHandle)
        {
            DWORD errorCode = GetLastError();
            show_last_error_message(L"SetWindowsHookEx", errorCode, L"PowerToys - Keyboard Manager");
            auto errorMessage = get_last_error_message(errorCode);
            Trace::Error(errorCode, errorMessage.has_value() ? errorMessage.value() : L"", L"StartLowlevelKeyboardHook::SetWindowsHookEx");
        }
    }
}

void KeyboardManager::StopLowlevelKeyboardHook()
{
    if (hookHandle)
    {
        UnhookWindowsHookEx(hookHandle);
        hookHandle = nullptr;
    }
}

bool KeyboardManager::HasRegisteredRemappings() const
{
    constexpr int MaxAttempts = 5;

    if (loadingSettings)
    {
        for (int currentAttempt = 0; currentAttempt < MaxAttempts; ++currentAttempt)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            if (!loadingSettings)
                break;
        }
    }

    // Assume that we have registered remappings to be on the safe side if we couldn't check
    if (loadingSettings)
        return true;

    return HasRegisteredRemappingsUnchecked();
}

bool KeyboardManager::HasRegisteredRemappingsUnchecked() const
{
    return !(state.appSpecificShortcutReMap.empty() && state.appSpecificShortcutReMapSortedKeys.empty() && state.osLevelShortcutReMap.empty() && state.osLevelShortcutReMapSortedKeys.empty() && state.singleKeyReMap.empty() && state.singleKeyToTextReMap.empty());
}

intptr_t KeyboardManager::HandleKeyboardHookEvent(LowlevelKeyboardEvent* data) noexcept
{
    if (loadingSettings)
    {
        return 0;
    }

    // Suspend remapping if remap key/shortcut window is opened
    if (editorIsRunningEvent != nullptr && WaitForSingleObject(editorIsRunningEvent, 0) == WAIT_OBJECT_0)
    {
        return 0;
    }

    // If key has suppress flag, then suppress it
    if (data->lParam->dwExtraInfo == KeyboardManagerConstants::KEYBOARDMANAGER_SUPPRESS_FLAG)
    {
        return 1;
    }

    // Remap a key
    intptr_t SingleKeyRemapResult = KeyboardEventHandlers::HandleSingleKeyRemapEvent(inputHandler, data, state);

    // Single key remaps have priority. If a key is remapped, only the remapped version should be visible to the shortcuts and hence the event should be suppressed here.
    if (SingleKeyRemapResult == 1)
    {
        return 1;
    }

    /* This feature has not been enabled (code from proof of concept stage)
        // Remap a key to behave like a modifier instead of a toggle
        intptr_t SingleKeyToggleToModResult = KeyboardEventHandlers::HandleSingleKeyToggleToModEvent(inputHandler, data, keyboardManagerState);
    */

    // Handle an app-specific shortcut remapping
    intptr_t AppSpecificShortcutRemapResult = KeyboardEventHandlers::HandleAppSpecificShortcutRemapEvent(inputHandler, data, state);

    // If an app-specific shortcut is remapped then the os-level shortcut remapping should be suppressed.
    if (AppSpecificShortcutRemapResult == 1)
    {
        return 1;
    }

    intptr_t SingleKeyToTextRemapResult = KeyboardEventHandlers::HandleSingleKeyToTextRemapEvent(inputHandler, data, state);

    if (SingleKeyToTextRemapResult == 1)
    {
        return 1;
    }

    // Handle an os-level shortcut remapping
    return KeyboardEventHandlers::HandleOSLevelShortcutRemapEvent(inputHandler, data, state);
}
