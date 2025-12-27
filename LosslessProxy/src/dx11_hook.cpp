#include "dx11_hook.hpp"
#include <fstream>
#include <filesystem>
#include <Windows.h>
#include <string>
#include <sstream>

using Microsoft::WRL::ComPtr;

namespace DX11Hook {

    // Cached resources
    static ComPtr<ID3D11Buffer> g_constantBuffer;
    static ComPtr<ID3D11SamplerState> g_linearSampler;
    static ComPtr<ID3D11Device> g_device;

    // Constant buffer structure (must match shader)
    struct UpscaleParams {
        uint32_t InputWidth;
        uint32_t InputHeight;
        uint32_t OutputWidth;
        uint32_t OutputHeight;
        float ScaleX;
        float ScaleY;
        float InvScaleX;
        float InvScaleY;
    };

    // Log helper
    static std::wofstream g_logFile;
    static void LogToFile(const std::wstring& message) {
        static bool initialized = false;
        if (!initialized) {
            initialized = true;
            wchar_t exePath[MAX_PATH];
            GetModuleFileNameW(nullptr, exePath, MAX_PATH);
            std::filesystem::path logPath = std::filesystem::path(exePath).parent_path() / L"DX11Hook.log";
            g_logFile.open(logPath, std::ios::app);
        }
        if (g_logFile.is_open()) {
            g_logFile << message << std::endl;
            g_logFile.flush();
        }
    }

    void Initialize() {
        LogToFile(L"[DX11Hook] Initialize called");
    }

    void Shutdown() {
        g_constantBuffer.Reset();
        g_linearSampler.Reset();
        g_device.Reset();
        LogToFile(L"[DX11Hook] Shutdown complete");
    }

    bool HookDeviceContext(ID3D11DeviceContext* context) {
        if (!context) return false;
        
        // We don't actually hook the vtable - just create the resources needed
        LogToFile(L"[DX11Hook] DeviceContext provided - creating resources");
        
        // Get device from context
        if (!g_device) {
            context->GetDevice(g_device.GetAddressOf());
        }
        
        return g_device != nullptr;
    }

    void SetupShaderResources(ID3D11DeviceContext* context, uint32_t inputWidth, uint32_t inputHeight, 
                              uint32_t outputWidth, uint32_t outputHeight) {
        
        if (!context) return;

        // Get device from context
        if (!g_device) {
            context->GetDevice(g_device.GetAddressOf());
        }

        if (!g_device) return;

        // Create constant buffer
        if (!g_constantBuffer) {
            D3D11_BUFFER_DESC cbDesc = {};
            cbDesc.ByteWidth = sizeof(UpscaleParams);
            cbDesc.Usage = D3D11_USAGE_DYNAMIC;
            cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
            cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

            HRESULT hr = g_device->CreateBuffer(&cbDesc, nullptr, g_constantBuffer.GetAddressOf());
            if (SUCCEEDED(hr)) {
                LogToFile(L"[DX11Hook] Created constant buffer");
            } else {
                LogToFile(L"[DX11Hook] Failed to create constant buffer");
                return;
            }
        }

        // Update constant buffer with dimensions
        if (g_constantBuffer) {
            D3D11_MAPPED_SUBRESOURCE mapped;
            HRESULT hr = context->Map(g_constantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
            if (SUCCEEDED(hr)) {
                UpscaleParams* params = (UpscaleParams*)mapped.pData;
                params->InputWidth = inputWidth;
                params->InputHeight = inputHeight;
                params->OutputWidth = outputWidth;
                params->OutputHeight = outputHeight;
                params->ScaleX = (float)outputWidth / (float)inputWidth;
                params->ScaleY = (float)outputHeight / (float)inputHeight;
                params->InvScaleX = (float)inputWidth / (float)outputWidth;
                params->InvScaleY = (float)inputHeight / (float)outputHeight;
                context->Unmap(g_constantBuffer.Get(), 0);
                
                // Bind constant buffer
                context->CSSetConstantBuffers(0, 1, g_constantBuffer.GetAddressOf());
                
                std::wostringstream oss;
                oss << L"[DX11Hook] Updated and bound constant buffer: " 
                    << inputWidth << L"x" << inputHeight << L" -> " 
                    << outputWidth << L"x" << outputHeight;
                LogToFile(oss.str());
            }
        }

        // Create linear sampler
        if (!g_linearSampler) {
            D3D11_SAMPLER_DESC sampDesc = {};
            sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
            sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
            sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
            sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
            sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
            sampDesc.MinLOD = 0;
            sampDesc.MaxLOD = D3D11_FLOAT32_MAX;

            HRESULT hr = g_device->CreateSamplerState(&sampDesc, g_linearSampler.GetAddressOf());
            if (SUCCEEDED(hr)) {
                LogToFile(L"[DX11Hook] Created linear sampler");
                
                // Bind sampler
                context->CSSetSamplers(0, 1, g_linearSampler.GetAddressOf());
                LogToFile(L"[DX11Hook] Bound linear sampler to slot 0");
            } else {
                LogToFile(L"[DX11Hook] Failed to create linear sampler");
            }
        } else {
            // Just bind it
            context->CSSetSamplers(0, 1, g_linearSampler.GetAddressOf());
        }
    }

} // namespace DX11Hook
