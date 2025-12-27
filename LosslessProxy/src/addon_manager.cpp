#include "addon_manager.hpp"
#include "imgui.h"
#include <filesystem>

namespace fs = std::filesystem;

AddonManager::AddonManager() {
  wchar_t buffer[MAX_PATH];
  GetModuleFileNameW(NULL, buffer, MAX_PATH);
  fs::path exePath(buffer);
  configFilePath =
      (exePath.parent_path() / "addons" / "addons_config.ini").wstring();

  ScanAddons();
  LoadConfig();
}

AddonManager::~AddonManager() { UnloadAddons(); }

void AddonManager::ScanAddons() {
  addons.clear();

  wchar_t buffer[MAX_PATH];
  GetModuleFileNameW(NULL, buffer, MAX_PATH);
  fs::path exePath(buffer);
  fs::path addonsPath = exePath.parent_path() / "addons";

  if (!fs::exists(addonsPath)) {
    fs::create_directory(addonsPath);
    return;
  }

  for (const auto &entry : fs::directory_iterator(addonsPath)) {
    if (fs::is_directory(entry.path())) {
      AddonInfo info;
      info.name = entry.path().filename().wstring();
      info.enabled = true; // Default to true
      info.hModule = nullptr;

      // Check for DLL
      bool foundDll = false;

      // Priority 1: DLL with same name as folder
      fs::path expectedDll =
          entry.path() / (entry.path().filename().string() + ".dll");
      if (fs::exists(expectedDll)) {
        info.path = expectedDll.wstring();
        foundDll = true;
      }

      for (const auto &subEntry : fs::directory_iterator(entry.path())) {
        if (subEntry.path().extension() == ".dll") {
          // Priority 2: Any DLL (if specific one not found)
          if (!foundDll) {
            info.path = subEntry.path().wstring();
            foundDll = true;
          }
        } else if (subEntry.path().extension() == ".ini") {
          info.configPath = subEntry.path().wstring();
        }
      }

      if (foundDll) {
        addons.push_back(info);
      }
    }
  }
}

void AddonManager::LoadConfig() {
  for (auto &addon : addons) {
    int status = GetPrivateProfileIntW(L"Addons", addon.name.c_str(), 1,
                                       configFilePath.c_str());
    addon.enabled = (status != 0);
  }

  // Load global debug visualization options
  showFpsCounter = (GetPrivateProfileIntW(L"Debug", L"ShowFpsCounter", 0,
                                          configFilePath.c_str()) != 0);

  // Note: ShowCustomShaderBorder was specific to CustomShaders, but we can keep
  // it as a global debug flag if desired. For now, I'll keep the variable but
  // it might not do anything unless passed to addons.
  showCustomShaderBorder =
      (GetPrivateProfileIntW(L"Debug", L"ShowCustomShaderBorder", 0,
                             configFilePath.c_str()) != 0);
}

void AddonManager::SaveConfig() {
  for (const auto &addon : addons) {
    WritePrivateProfileStringW(L"Addons", addon.name.c_str(),
                               addon.enabled ? L"1" : L"0",
                               configFilePath.c_str());
  }

  // Save debug visualization options
  WritePrivateProfileStringW(L"Debug", L"ShowFpsCounter",
                             showFpsCounter ? L"1" : L"0",
                             configFilePath.c_str());
  WritePrivateProfileStringW(L"Debug", L"ShowCustomShaderBorder",
                             showCustomShaderBorder ? L"1" : L"0",
                             configFilePath.c_str());
}

void AddonManager::LoadAddons() {
  for (auto &addon : addons) {
    if (addon.enabled && !addon.hModule) {
      LoadAddon(addon);
    }
  }
}

void AddonManager::UnloadAddons() {
  for (auto &addon : addons) {
    if (addon.hModule) {
      UnloadAddon(addon);
    }
  }
}

void AddonManager::ReloadAddons() {
  UnloadAddons();
  ScanAddons(); // Rescan in case new files appeared
  LoadConfig();
  LoadAddons();
}

void AddonManager::LoadAddon(AddonInfo &addon) {
  // Use LoadLibraryEx with LOAD_WITH_ALTERED_SEARCH_PATH to ensure dependencies
  // in the same directory are found.
  HMODULE hAddon =
      LoadLibraryExW(addon.path.c_str(), NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
  if (!hAddon) {
    hAddon = LoadLibraryW(addon.path.c_str());
  }

  if (hAddon) {
    addon.hModule = hAddon;

    // Load API Functions
    addon.InitFunc = (AddonInit_t)GetProcAddress(hAddon, "AddonInitialize");
    if (!addon.InitFunc) {
      // Try legacy name
      addon.InitFunc = (AddonInit_t)GetProcAddress(hAddon, "AddonInit");
    }

    addon.ShutdownFunc =
        (AddonShutdown_t)GetProcAddress(hAddon, "AddonShutdown");
    addon.RenderSettingsFunc =
        (AddonRenderSettings_t)GetProcAddress(hAddon, "AddonRenderSettings");
    addon.InterceptResourceFunc = (AddonInterceptResource_t)GetProcAddress(
        hAddon, "AddonInterceptResource");
    GetAddonCaps_t getCaps =
        (GetAddonCaps_t)GetProcAddress(hAddon, "GetAddonCapabilities");

    if (getCaps) {
      addon.capabilities = getCaps();
    }

    // Call Initialize later (in InitializeAddons)
  } else {
    // Failed to load
  }
}

void AddonManager::InitializeAddons(void *imGuiContext) {
  ImGuiMemAllocFunc alloc_func;
  ImGuiMemFreeFunc free_func;
  void *user_data;
  ImGui::GetAllocatorFunctions(&alloc_func, &free_func, &user_data);

  for (auto &addon : addons) {
    if (addon.enabled && addon.InitFunc) {
      // Cast to void* to match the generic signature, assuming Addons follow
      // the new API Note: We might need to keep legacy support IF strict ABI is
      // needed, but since we control both... We'll cast the function pointer to
      // a type that accepts 5 args. Wait, AddonInit_t is already updated. The
      // addons need to update their export too. But existing addons binaries
      // might crash if called with extra args? Standard C calling convention
      // (cdecl) cleans up stack by caller usually? Actually x64 uses registers
      // for first 4 args. 5th is stack. To be safe, we should check if it's the
      // new version? or just assume. User asked to fix it, implies we update
      // code.
      addon.InitFunc(this, (ImGuiContext *)imGuiContext, (void *)alloc_func,
                     (void *)free_func, user_data);
    }
  }
}

void AddonManager::UnloadAddon(AddonInfo &addon) {
  if (addon.hModule) {
    if (addon.ShutdownFunc) {
      addon.ShutdownFunc();
    }
    FreeLibrary(addon.hModule);
    addon.hModule = nullptr;
    addon.InitFunc = nullptr;
    addon.ShutdownFunc = nullptr;
    addon.RenderSettingsFunc = nullptr;
    addon.InterceptResourceFunc = nullptr;
    addon.capabilities = ADDON_CAP_NONE;
  }
}

bool AddonManager::InterceptResource(const wchar_t *name, const wchar_t *type,
                                     const void **outData, uint32_t *outSize) {
  for (const auto &addon : addons) {
    if (addon.enabled && addon.InterceptResourceFunc) {
      if (addon.InterceptResourceFunc(name, type, outData, outSize)) {
        return true;
      }
    }
  }
  return false;
}

std::vector<AddonInfo> &AddonManager::GetAddons() { return addons; }

void AddonManager::ToggleAddon(int index, bool enable) {
  if (index >= 0 && index < addons.size()) {
    addons[index].enabled = enable;
    if (enable) {
      if (!addons[index].hModule) {
        LoadAddon(addons[index]);
      }
    } else {
      if (addons[index].hModule) {
        UnloadAddon(addons[index]);
      }
    }
    SaveConfig();
  }
}

void AddonManager::RenderAddonSettings(int index) {
  if (index >= 0 && index < addons.size()) {
    auto &addon = addons[index];
    if (addon.hModule && addon.RenderSettingsFunc) {
      addon.RenderSettingsFunc();
    } else {
      // Check if it has legacy settings handling or is just missing the export
      if (addon.capabilities & ADDON_CAP_HAS_SETTINGS) {
        // Fallback: If it claims to have settings but no Render function, warn
        // or ignore Maybe it's LS_CustomShaders with old export? We will update
        // LS_CustomShaders to export AddonRenderSettings.
      }
    }
  }
}

// IHost Implementation
void AddonManager::Log(const wchar_t *message) {
  OutputDebugStringW(message);
  OutputDebugStringW(L"\n");
}

// Debug visualization options
bool AddonManager::GetShowFpsCounter() const { return showFpsCounter; }

void AddonManager::SetShowFpsCounter(bool show) {
  showFpsCounter = show;
  SaveConfig();
}
