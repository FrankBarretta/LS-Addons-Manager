#include "shader_hook.hpp"
#include "addon_manager.hpp"
#include "dx11_hook.hpp"
#include "iat_patcher.hpp"
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>

namespace ShaderHook {

// Global state
static AddonManager *g_addonManager = nullptr;
static std::map<HRSRC, CachedShader> g_shaderCache;
static CRITICAL_SECTION g_cacheLock;
static bool g_hooksInstalled = false;
static std::wfstream g_logFile;
static int g_findResourceCallCount = 0; // Count total FindResourceW calls
static std::map<WORD, int> g_resourceIdCallCount; // Count calls per resource ID
static std::map<std::wstring, int>
    g_shaderCallCount; // Track calls per shader name for rotation

// Log helper function
static void LogToFile(const std::wstring &message) {
  try {
    // Create log file path in the same directory as Lossless Scaling
    static bool g_logInitialized = false;
    if (!g_logInitialized) {
      g_logInitialized = true;

      // Get Lossless Scaling installation directory
      wchar_t exePath[MAX_PATH];
      GetModuleFileNameW(nullptr, exePath, MAX_PATH);
      std::filesystem::path logPath =
          std::filesystem::path(exePath).parent_path() / L"ShaderHook.log";

      // Open log file for appending
      g_logFile.open(logPath, std::ios::app | std::ios::out);
    }

    if (g_logFile.is_open()) {
      g_logFile << message << std::endl;
      g_logFile.flush();
    }
  } catch (...) {
    // Silently fail if logging fails
  }
}

// Original API function pointers
static HRSRC(WINAPI *g_origFindResourceW)(HMODULE, LPCWSTR, LPCWSTR) = nullptr;
static HGLOBAL(WINAPI *g_origLoadResource)(HMODULE, HRSRC) = nullptr;
static DWORD(WINAPI *g_origSizeofResource)(HMODULE, HRSRC) = nullptr;
static LPVOID(WINAPI *g_origLockResource)(HGLOBAL) = nullptr;
static BOOL(WINAPI *g_origFreeResource)(HGLOBAL) = nullptr;

// Magic value to identify our custom handles (use a 32-bit constant)
static const uint32_t CUSTOM_SHADER_MAGIC =
    0xF00DB00Fu; // 32-bit unique identifier

// Create a fake handle that encodes our magic value into the upper half
inline HRSRC MakeCustomHandle(DWORD id) {
  uintptr_t magicShift = (sizeof(uintptr_t) == 8) ? 32 : 16;
  uintptr_t v = (((uintptr_t)CUSTOM_SHADER_MAGIC) << magicShift) |
                (uintptr_t)(id & 0xFFFF);
  return (HRSRC)(v);
}

inline bool IsCustomHandle(HRSRC handle) {
  uintptr_t v = (uintptr_t)handle;
  uintptr_t magicShift = (sizeof(uintptr_t) == 8) ? 32 : 16;
  uintptr_t magic = v >> magicShift;
  return magic == (uintptr_t)CUSTOM_SHADER_MAGIC;
}

inline DWORD GetCustomHandleId(HRSRC handle) {
  uintptr_t v = (uintptr_t)handle;
  return (DWORD)(v & 0xFFFF);
}

void Initialize(AddonManager *addonManager) {
  g_addonManager = addonManager;
  InitializeCriticalSection(&g_cacheLock);
  DX11Hook::Initialize();
  LogToFile(L"[ShaderHook] Initialized");
}

void Shutdown() {
  DX11Hook::Shutdown();
  EnterCriticalSection(&g_cacheLock);
  g_shaderCache.clear();
  LeaveCriticalSection(&g_cacheLock);
  DeleteCriticalSection(&g_cacheLock);
  g_addonManager = nullptr;
}

bool IsOurShaderHandle(HRSRC handle) { return IsCustomHandle(handle); }

const CachedShader *GetCachedShader(HRSRC handle) {
  EnterCriticalSection(&g_cacheLock);
  auto it = g_shaderCache.find(handle);
  LeaveCriticalSection(&g_cacheLock);

  if (it != g_shaderCache.end()) {
    return &it->second;
  }
  return nullptr;
}

// Helper for logging/debug only
bool IsShaderResourceId(LPCWSTR lpName, LPCWSTR lpType) {
  if ((uintptr_t)lpType != 0xa)
    return false;
  if (!IS_INTRESOURCE(lpName))
    return false;
  return true;
}

// --------------------------------------------------------------------------------------
// Hooked FindResourceW Implementation
// --------------------------------------------------------------------------------------

HRSRC WINAPI HookedFindResourceW(HMODULE hModule, LPCWSTR lpName,
                                 LPCWSTR lpType) {
  g_findResourceCallCount++;

  // Try to intercept via Addon Manager
  if (g_addonManager) {
    const void *customData = nullptr;
    uint32_t customSize = 0;

    if (g_addonManager->InterceptResource(lpName, lpType, &customData,
                                          &customSize)) {
      if (customData && customSize > 0) {
        DWORD handleId = 0;
        if (IS_INTRESOURCE(lpName)) {
          handleId = (DWORD)(uintptr_t)lpName;
        } else {
          handleId = 0xFFFF; // Fallback for string names
        }

        HRSRC customHandle = MakeCustomHandle(handleId);

        EnterCriticalSection(&g_cacheLock);
        CachedShader &cached = g_shaderCache[customHandle];
        cached.bytecode.assign((const uint8_t *)customData,
                               (const uint8_t *)customData + customSize);
        cached.size = customSize;
        LeaveCriticalSection(&g_cacheLock);

        std::wostringstream oss;
        oss << L"[ShaderHook] Intercepted resource. Handle: 0x" << std::hex
            << (uintptr_t)customHandle;
        LogToFile(oss.str());

        return customHandle;
      }
    }
  }

  // Original Behavior
  if (g_origFindResourceW) {
    return g_origFindResourceW(hModule, lpName, lpType);
  }
  return nullptr;
}

// Hooked LoadResource
HGLOBAL WINAPI HookedLoadResource(HMODULE hModule, HRSRC hResInfo) {
  if (IsCustomHandle(hResInfo)) {
    return (HGLOBAL)hResInfo;
  }
  if (g_origLoadResource) {
    return g_origLoadResource(hModule, hResInfo);
  }
  return nullptr;
}

// Hooked SizeofResource
DWORD WINAPI HookedSizeofResource(HMODULE hModule, HRSRC hResInfo) {
  if (IsCustomHandle(hResInfo)) {
    const CachedShader *cached = GetCachedShader(hResInfo);
    if (cached) {
      return cached->size;
    }
    return 0;
  }
  if (g_origSizeofResource) {
    return g_origSizeofResource(hModule, hResInfo);
  }
  return 0;
}

// Hooked LockResource
LPVOID WINAPI HookedLockResource(HGLOBAL hResData) {
  HRSRC asHandle = (HRSRC)hResData;
  if (IsCustomHandle(asHandle)) {
    const CachedShader *cached = GetCachedShader(asHandle);
    if (cached && !cached->bytecode.empty()) {
      return (void *)cached->bytecode.data();
    }
  }
  if (g_origLockResource) {
    return g_origLockResource(hResData);
  }
  return nullptr;
}

// Hooked FreeResource
BOOL WINAPI HookedFreeResource(HGLOBAL hResData) {
  HRSRC asHandle = (HRSRC)hResData;
  if (IsCustomHandle(asHandle)) {
    return TRUE;
  }
  if (g_origFreeResource) {
    return g_origFreeResource(hResData);
  }
  return TRUE;
}

// Helper to patch memory
void PatchMemory(HMODULE hModule, DWORD rva,
                 const std::vector<uint8_t> &bytes) {
  if (!hModule)
    return;
  uint8_t *address = (uint8_t *)hModule + rva;

  DWORD oldProtect;
  if (VirtualProtect(address, bytes.size(), PAGE_EXECUTE_READWRITE,
                     &oldProtect)) {
    std::memcpy(address, bytes.data(), bytes.size());
    VirtualProtect(address, bytes.size(), oldProtect, &oldProtect);
    std::wostringstream oss;
    oss << L"[ShaderHook] Patched memory at RVA 0x" << std::hex << rva;
    LogToFile(oss.str());
  }
}

// Install hooks
void InstallHooks() {
  if (g_hooksInstalled)
    return;

  HMODULE hKernel32 = GetModuleHandleW(L"kernel32.dll");
  if (!hKernel32)
    return;

  g_origFindResourceW = (HRSRC(WINAPI *)(
      HMODULE, LPCWSTR, LPCWSTR))GetProcAddress(hKernel32, "FindResourceW");
  g_origLoadResource = (HGLOBAL(WINAPI *)(HMODULE, HRSRC))GetProcAddress(
      hKernel32, "LoadResource");
  g_origSizeofResource = (DWORD(WINAPI *)(HMODULE, HRSRC))GetProcAddress(
      hKernel32, "SizeofResource");
  g_origLockResource =
      (LPVOID(WINAPI *)(HGLOBAL))GetProcAddress(hKernel32, "LockResource");
  g_origFreeResource =
      (BOOL(WINAPI *)(HGLOBAL))GetProcAddress(hKernel32, "FreeResource");

  if (!g_origFindResourceW || !g_origLoadResource || !g_origSizeofResource ||
      !g_origLockResource || !g_origFreeResource) {
    LogToFile(L"[ShaderHook] Failed to get original function pointers");
    return;
  }

  HMODULE hLosslessOriginal = GetModuleHandleW(L"Lossless_original.dll");
  if (!hLosslessOriginal) {
    LogToFile(L"[ShaderHook] Failed to get Lossless_original.dll handle");
    return;
  }

  // Patch IAT
  IatPatcher::PatchIat(hLosslessOriginal, "kernel32.dll", "FindResourceW",
                       &HookedFindResourceW);
  IatPatcher::PatchIat(hLosslessOriginal, "kernel32.dll", "LoadResource",
                       &HookedLoadResource);
  IatPatcher::PatchIat(hLosslessOriginal, "kernel32.dll", "SizeofResource",
                       &HookedSizeofResource);
  IatPatcher::PatchIat(hLosslessOriginal, "kernel32.dll", "LockResource",
                       &HookedLockResource);
  IatPatcher::PatchIat(hLosslessOriginal, "kernel32.dll", "FreeResource",
                       &HookedFreeResource);

  // Apply Patches if any addon requests it
  bool shouldApplyPatches = false;
  if (g_addonManager) {
    auto &addons = g_addonManager->GetAddons();
    for (const auto &addon : addons) {
      if (addon.enabled && (addon.capabilities & ADDON_CAP_PATCH_LS1_LOGIC)) {
        shouldApplyPatches = true;
        break;
      }
    }
  }

  if (shouldApplyPatches) {
    std::vector<uint8_t> nops = {0x90, 0x90};
    std::vector<DWORD> patchOffsets = {0x51ac, 0x59c6, 0x5ab7, 0x5bc9, 0x5ce2,
                                       0x65ec, 0x6f04, 0x6fe7, 0x78a7, 0x7f86,
                                       0x8056, 0x8128, 0x8201, 0x8bdc, 0x92f0,
                                       0x941d, 0x9d6c, 0xa480, 0xa589};
    for (DWORD rva : patchOffsets) {
      PatchMemory(hLosslessOriginal, rva, nops);
    }
    LogToFile(L"[ShaderHook] Applied memory patches");
  }

  FlushInstructionCache(GetCurrentProcess(), nullptr, 0);
  g_hooksInstalled = true;
  LogToFile(L"[ShaderHook] Hooks installed");
}

void UninstallHooks() {
  if (!g_hooksInstalled)
    return;
  g_hooksInstalled = false;
  LogToFile(L"[ShaderHook] Hooks uninstalled");
}

} // namespace ShaderHook
