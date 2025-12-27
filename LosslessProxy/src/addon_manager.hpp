#pragma once
#include "include/addon_api.hpp"
#include <string>
#include <vector>
#include <windows.h>

struct AddonInfo {
  std::wstring name;
  std::wstring path;
  std::wstring configPath; // Empty if no config
  bool enabled = true;
  HMODULE hModule = nullptr;
  uint32_t capabilities = 0; // Bitmask of AddonCaps

  // UI State
  bool showSettings = false;

  // Cached Function Pointers
  AddonInit_t InitFunc = nullptr;
  AddonShutdown_t ShutdownFunc = nullptr;
  AddonRenderSettings_t RenderSettingsFunc = nullptr;
  AddonInterceptResource_t InterceptResourceFunc = nullptr;
};

class AddonManager : public IHost {
public:
  AddonManager();
  ~AddonManager();
  void LoadAddons();
  void UnloadAddons();
  void ReloadAddons();

  std::vector<AddonInfo> &GetAddons();
  void ToggleAddon(int index, bool enable);
  void SaveConfig();

  // IHost Implementation
  void Log(const wchar_t *message) override;

  // Generic generic API methods
  void RenderAddonSettings(int index);
  bool InterceptResource(const wchar_t *name, const wchar_t *type,
                         const void **outData, uint32_t *outSize);

  // Lifecycle
  void InitializeAddons(void *imGuiContext);

  // Debug visualization options
  bool GetShowFpsCounter() const;
  void SetShowFpsCounter(bool show);

private:
  void LoadAddon(AddonInfo &addon);
  void UnloadAddon(AddonInfo &addon);
  void ScanAddons();
  void LoadConfig();

  std::vector<AddonInfo> addons;
  std::wstring configFilePath;
  bool showFpsCounter = false;
  bool showCustomShaderBorder = false;
};
