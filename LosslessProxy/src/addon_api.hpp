#pragma once
#include <cstdint>
#include <windows.h>

// Forward declaration of ImGui context (if we support UI embedding)
struct ImGuiContext;

// Addon Capabilities Bitmask
enum AddonCaps : uint32_t {
  ADDON_CAP_NONE = 0,
  ADDON_CAP_HAS_SETTINGS = 1 << 0, // Provides a settings UI via RenderSettings
  ADDON_CAP_UPSCALER_PROVIDER = 1 << 1, // Provides upscalers (future use)
  ADDON_CAP_FRAMEGEN_PROVIDER = 1 << 2, // Provides framegen (future use)
  ADDON_CAP_PATCH_LS1_LOGIC = 1
                              << 3 // Request host to patch LS1 JMP instructions
};

// Interface for the Host (LosslessProxy) to expose services to Addons
struct IHost {
  virtual void Log(const wchar_t *message) = 0;
  // Add more host services here (e.g. Config access)
};

// Standard Export Function Names
// Addons should export:
// extern "C" __declspec(dllexport) void AddonInitialize(IHost* host,
// ImGuiContext* ctx); extern "C" __declspec(dllexport) void AddonShutdown();
// extern "C" __declspec(dllexport) uint32_t GetAddonCapabilities();
// extern "C" __declspec(dllexport) void AddonRenderSettings(); // If
// CAP_HAS_SETTINGS set extern "C" __declspec(dllexport) const char*
// GetAddonName(); extern "C" __declspec(dllexport) const char*
// GetAddonVersion();

typedef void (*AddonInit_t)(IHost *host, ImGuiContext *ctx, void *alloc_func,
                            void *free_func, void *user_data);
typedef void (*AddonShutdown_t)();
typedef uint32_t (*GetAddonCaps_t)();
typedef void (*AddonRenderSettings_t)();
typedef bool (*AddonInterceptResource_t)(const wchar_t *name,
                                         const wchar_t *type,
                                         const void **outData,
                                         uint32_t *outSize);
typedef const char *(*GetAddonName_t)();
typedef const char *(*GetAddonVersion_t)();
