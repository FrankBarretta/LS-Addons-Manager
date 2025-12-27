#pragma once

#include <windows.h>
#include <unordered_map>
#include <vector>
#include <memory>

class AddonManager;

// Shader hook - handles FindResourceW/LoadResource interception
namespace ShaderHook {

    // Custom shader info stored in our cache
    struct CachedShader {
        std::vector<uint8_t> bytecode;
        DWORD size;
    };

    // Initialize hook with addon manager reference
    void Initialize(AddonManager* addonManager);
    
    // Cleanup
    void Shutdown();

    // Install Windows API hooks
    void InstallHooks();

    // Uninstall hooks
    void UninstallHooks();

    // Check if a resource handle is one of ours (custom shader)
    bool IsOurShaderHandle(HRSRC handle);

    // Get cached shader data for a handle
    const CachedShader* GetCachedShader(HRSRC handle);

    // Hooked API functions (these match the original signatures)
    HRSRC WINAPI HookedFindResourceW(HMODULE hModule, LPCWSTR lpName, LPCWSTR lpType);
    HGLOBAL WINAPI HookedLoadResource(HMODULE hModule, HRSRC hResInfo);
    DWORD WINAPI HookedSizeofResource(HMODULE hModule, HRSRC hResInfo);
    LPVOID WINAPI HookedLockResource(HGLOBAL hResData);
    BOOL WINAPI HookedFreeResource(HGLOBAL hResData);
}



