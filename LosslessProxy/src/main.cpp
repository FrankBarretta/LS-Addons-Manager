#include "addon_manager.hpp"
#include "gui_manager.hpp"
#include "shader_hook.hpp"
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>
#include <windows.h>

// Forward exports to the original DLL
#pragma comment(linker, "/export:Activate=Lossless_original.Activate")
#pragma comment(linker, "/export:ApplySettings=Lossless_original.ApplySettings")
#pragma comment(linker,                                                        \
                "/export:GetAdapterNames=Lossless_original.GetAdapterNames")
#pragma comment(linker,                                                        \
                "/export:GetDisplayNames=Lossless_original.GetDisplayNames")
#pragma comment(                                                               \
    linker, "/export:GetDwmRefreshRate=Lossless_original.GetDwmRefreshRate")
#pragma comment(                                                               \
    linker,                                                                    \
    "/export:GetForegroundWindowEx=Lossless_original.GetForegroundWindowEx")
#pragma comment(linker, "/export:Init=Lossless_original.Init")
#pragma comment(                                                               \
    linker,                                                                    \
    "/export:IsWindowsBuildAtLeast=Lossless_original.IsWindowsBuildAtLeast")
#pragma comment(                                                               \
    linker, "/export:SetDriverSettings=Lossless_original.SetDriverSettings")
#pragma comment(                                                               \
    linker, "/export:SetWindowsSettings=Lossless_original.SetWindowsSettings")
#pragma comment(linker, "/export:UnInit=Lossless_original.UnInit")

AddonManager *g_addonManager = nullptr;

extern "C" {
__declspec(dllexport) int GetShowFpsCounterProxy() {
  if (!g_addonManager)
    return 0;
  return g_addonManager->GetShowFpsCounter() ? 1 : 0;
}
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call,
                      LPVOID lpReserved) {
  switch (ul_reason_for_call) {
  case DLL_PROCESS_ATTACH: {
    DisableThreadLibraryCalls(hModule);

    // CRITICAL: Load Lossless_original.dll FIRST so we can IAT patch it
    // The pragma linker comments will auto-load it, but we do it explicitly
    // here
    HMODULE hLosslessOriginal = LoadLibraryW(L"Lossless_original.dll");
    if (!hLosslessOriginal) {
      std::wcerr << L"[ERROR] Failed to load Lossless_original.dll"
                 << std::endl;
      return FALSE;
    }
    std::wcout << L"[Main] Lossless_original.dll loaded successfully"
               << std::endl;

    // Initialize Addon Manager
    g_addonManager = new AddonManager();

    // Initialize shader hook system AFTER Lossless_original is loaded
    ShaderHook::Initialize(g_addonManager);
    ShaderHook::InstallHooks(); // This patches the IAT of Lossless_original

    std::wcout << L"[Main] Shader hooks installed, loading addons..."
               << std::endl;

    // We load addons in the GUI thread to avoid Loader Lock issues with
    // LoadLibrary
    GuiManager::StartGuiThread(g_addonManager);
    break;
  }
  case DLL_PROCESS_DETACH:
    ShaderHook::UninstallHooks();
    ShaderHook::Shutdown();
    if (g_addonManager) {
      delete g_addonManager;
      g_addonManager = nullptr;
    }
    break;
  }
  return TRUE;
}
