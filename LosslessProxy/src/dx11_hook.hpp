#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include <cstdint>

namespace DX11Hook {
    
    // Initialize DirectX 11 hooks
    void Initialize();
    
    // Shutdown and cleanup
    void Shutdown();
    
    // Hook DirectX device context to intercept shader execution
    bool HookDeviceContext(ID3D11DeviceContext* context);
    
    // Create and bind resources for custom shaders
    void SetupShaderResources(ID3D11DeviceContext* context, uint32_t inputWidth, uint32_t inputHeight, uint32_t outputWidth, uint32_t outputHeight);
    
} // namespace DX11Hook
