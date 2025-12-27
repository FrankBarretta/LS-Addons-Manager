#include "gui_manager.hpp"
#include "addon_manager.hpp"
#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"
#include <d3d11.h>
#include <dxgi.h>
#include <fstream>
#include <sstream>
#include <string>
#include <tchar.h>
#include <vector>

// Link against d3d11.lib and d3dcompiler.lib
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")

// Data
static ID3D11Device *g_pd3dDevice = nullptr;
static ID3D11DeviceContext *g_pd3dDeviceContext = nullptr;
static IDXGISwapChain *g_pSwapChain = nullptr;
static ID3D11RenderTargetView *g_mainRenderTargetView = nullptr;
static AddonManager *g_manager = nullptr;

// Config Editor State
static bool g_showConfigEditor = false;
static std::string g_configEditorContent;
static std::wstring g_configEditorPath;
static const size_t g_configBufferSize = 1024 * 1024; // 1MB buffer
static std::vector<char> g_configBuffer;

// Forward declarations of helper functions
bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
void OpenConfigEditor(const std::wstring &path);
void SaveConfigEditor();

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd,
                                                             UINT msg,
                                                             WPARAM wParam,
                                                             LPARAM lParam);

// Helper to convert wstring to string (UTF-8)
std::string WStringToString(const std::wstring &wstr) {
  if (wstr.empty())
    return std::string();
  int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(),
                                        NULL, 0, NULL, NULL);
  std::string strTo(size_needed, 0);
  WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0],
                      size_needed, NULL, NULL);
  return strTo;
}

// Helper to convert string (UTF-8) to wstring
std::wstring StringToWString(const std::string &str) {
  if (str.empty())
    return std::wstring();
  int size_needed =
      MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
  std::wstring wstrTo(size_needed, 0);
  MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0],
                      size_needed);
  return wstrTo;
}

void OpenConfigEditor(const std::wstring &path) {
  g_configEditorPath = path;
  std::ifstream file(path);
  if (file.is_open()) {
    std::stringstream buffer;
    buffer << file.rdbuf();
    g_configEditorContent = buffer.str();

    // Resize buffer if needed
    if (g_configBuffer.size() < g_configBufferSize) {
      g_configBuffer.resize(g_configBufferSize);
    }

    // Copy content to buffer
    strncpy_s(g_configBuffer.data(), g_configBuffer.size(),
              g_configEditorContent.c_str(), _TRUNCATE);

    g_showConfigEditor = true;
  }
}

void SaveConfigEditor() {
  if (!g_configEditorPath.empty()) {
    std::ofstream file(g_configEditorPath);
    if (file.is_open()) {
      file << g_configBuffer.data();
      file.close();
      g_showConfigEditor = false;
    }
  }
}

void GuiManager::StartGuiThread(AddonManager *manager) {
  g_manager = manager;
  CreateThread(NULL, 0, GuiThread, NULL, 0, NULL);
}

void SetupImGuiStyle() {
  ImGuiStyle &style = ImGui::GetStyle();
  style.WindowRounding = 8.0f;
  style.FrameRounding = 6.0f;
  style.PopupRounding = 6.0f;
  style.ScrollbarRounding = 6.0f;
  style.GrabRounding = 6.0f;
  style.TabRounding = 6.0f;
  style.ChildRounding = 8.0f;

  ImVec4 *colors = style.Colors;
  // Dark background matching the screenshot (approx)
  ImVec4 bgDark = ImVec4(0.11f, 0.09f, 0.09f, 1.00f);  // Very dark reddish grey
  ImVec4 panelBg = ImVec4(0.16f, 0.14f, 0.14f, 1.00f); // Slightly lighter
  ImVec4 accentRed = ImVec4(0.95f, 0.20f, 0.20f, 1.00f); // Bright red
  ImVec4 textWhite = ImVec4(0.95f, 0.95f, 0.95f, 1.00f);
  ImVec4 textDisabled = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);

  colors[ImGuiCol_WindowBg] = bgDark;
  colors[ImGuiCol_ChildBg] = panelBg;
  colors[ImGuiCol_PopupBg] = panelBg;
  colors[ImGuiCol_Border] = ImVec4(0.25f, 0.20f, 0.20f, 0.50f);
  colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);

  colors[ImGuiCol_Header] = ImVec4(0.25f, 0.20f, 0.20f, 1.00f);
  colors[ImGuiCol_HeaderHovered] = ImVec4(0.30f, 0.25f, 0.25f, 1.00f);
  colors[ImGuiCol_HeaderActive] = ImVec4(0.35f, 0.30f, 0.30f, 1.00f);

  colors[ImGuiCol_Button] = ImVec4(0.25f, 0.20f, 0.20f, 1.00f);
  colors[ImGuiCol_ButtonHovered] = ImVec4(0.30f, 0.25f, 0.25f, 1.00f);
  colors[ImGuiCol_ButtonActive] = ImVec4(0.35f, 0.30f, 0.30f, 1.00f);

  colors[ImGuiCol_FrameBg] = ImVec4(0.20f, 0.18f, 0.18f, 1.00f);
  colors[ImGuiCol_FrameBgHovered] = ImVec4(0.25f, 0.22f, 0.22f, 1.00f);
  colors[ImGuiCol_FrameBgActive] = ImVec4(0.30f, 0.25f, 0.25f, 1.00f);

  colors[ImGuiCol_CheckMark] = accentRed;
  colors[ImGuiCol_SliderGrab] = accentRed;
  colors[ImGuiCol_SliderGrabActive] = ImVec4(1.00f, 0.30f, 0.30f, 1.00f);

  colors[ImGuiCol_Text] = textWhite;
  colors[ImGuiCol_TextDisabled] = textDisabled;

  colors[ImGuiCol_TitleBg] = bgDark;
  colors[ImGuiCol_TitleBgActive] = bgDark;
  colors[ImGuiCol_TitleBgCollapsed] = bgDark;
}

DWORD WINAPI GuiManager::GuiThread(LPVOID lpParam) {
  if (g_manager) {
    g_manager->LoadAddons();
  }

  WNDCLASSEXW wc = {sizeof(wc),
                    CS_CLASSDC,
                    WndProc,
                    0L,
                    0L,
                    GetModuleHandle(NULL),
                    NULL,
                    NULL,
                    NULL,
                    NULL,
                    L"LosslessAddonManagerClass",
                    NULL};
  RegisterClassExW(&wc);
  HWND hwnd = CreateWindowW(
      wc.lpszClassName, L"Lossless Scaling Addons Manager", WS_OVERLAPPEDWINDOW,
      100, 100, 1000, 700, NULL, NULL, wc.hInstance, NULL);

  if (!CreateDeviceD3D(hwnd)) {
    CleanupDeviceD3D();
    UnregisterClassW(wc.lpszClassName, wc.hInstance);
    return 1;
  }

  ShowWindow(hwnd, SW_SHOWDEFAULT);
  UpdateWindow(hwnd);

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();

  // Initialize Addons with ImGui Context
  if (g_manager) {
    g_manager->InitializeAddons(ImGui::GetCurrentContext());
  }

  ImGuiIO &io = ImGui::GetIO();
  (void)io;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

  SetupImGuiStyle();

  ImGui_ImplWin32_Init(hwnd);
  ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

  bool show_demo_window = false;
  ImVec4 clear_color = ImVec4(0.11f, 0.09f, 0.09f, 1.00f);

  bool done = false;
  while (!done) {
    MSG msg;
    while (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
      if (msg.message == WM_QUIT)
        done = true;
    }
    if (done)
      break;

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    // Main Window
    {
      ImGui::SetNextWindowPos(ImVec2(0, 0));
      ImGui::SetNextWindowSize(io.DisplaySize);
      ImGui::Begin("Main", nullptr,
                   ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                       ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                       ImGuiWindowFlags_NoBringToFrontOnFocus);

      // Left Sidebar
      ImGui::BeginChild("Sidebar", ImVec2(200, 0), true);
      ImGui::Dummy(ImVec2(0, 10));
      ImGui::TextDisabled("   PROFILI DI GIOCO");
      ImGui::Dummy(ImVec2(0, 5));

      // Highlight "Predefinito" to look like selected
      ImGui::PushStyleColor(ImGuiCol_Header,
                            ImVec4(0.20f, 0.18f, 0.18f, 1.00f));
      if (ImGui::Selectable("   Predefinito", true)) {
      }
      ImGui::PopStyleColor();

      ImGui::Dummy(ImVec2(0, 20));
      ImGui::TextDisabled("   ADDONS");
      ImGui::Dummy(ImVec2(0, 5));
      if (ImGui::Selectable("   Manager", false)) {
      }

      ImGui::Dummy(ImVec2(0, 20));
      ImGui::Separator();
      ImGui::Dummy(ImVec2(0, 10));
      if (ImGui::Button("   +   ", ImVec2(40, 30))) {
      }
      ImGui::SameLine();
      if (ImGui::Button("   E   ", ImVec2(40, 30))) {
      } // Edit icon placeholder
      ImGui::SameLine();
      if (ImGui::Button("   D   ", ImVec2(40, 30))) {
      } // Delete icon placeholder

      ImGui::EndChild();

      ImGui::SameLine();

      // Main Content Area
      ImGui::BeginGroup();
      ImGui::Dummy(ImVec2(0, 10));
      ImGui::Text("   Profilo: \"Predefinito\"");
      ImGui::Dummy(ImVec2(0, 20));

      // Addons Panel
      ImGui::BeginChild("ContentScroll", ImVec2(0, 0), false);

      // Panel 1: Addons List
      ImGui::BeginChild("AddonsPanel", ImVec2(0, 250), true);
      ImGui::Text("Addons Installati");
      ImGui::Separator();
      ImGui::Dummy(ImVec2(0, 5));

      if (g_manager) {
        auto &addons = g_manager->GetAddons();
        for (int i = 0; i < addons.size(); ++i) {
          bool enabled = addons[i].enabled;
          ImGui::PushID(i);

          // Custom toggle switch style
          if (ImGui::Checkbox("", &enabled)) {
            g_manager->ToggleAddon(i, enabled);
          }
          ImGui::SameLine();
          ImGui::Text("%s", WStringToString(addons[i].name).c_str());

          // Config Button if path exists
          if (!addons[i].configPath.empty()) {
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Button,
                                  ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
            if (ImGui::SmallButton("Config")) {
              OpenConfigEditor(addons[i].configPath);
            }
            ImGui::PopStyleColor();
          }

          if (addons[i].capabilities & ADDON_CAP_HAS_SETTINGS) {
            ImGui::SameLine();
            if (ImGui::Button("Settings")) {
              addons[i].showSettings = !addons[i].showSettings;
            }
          }

          ImGui::SameLine(ImGui::GetWindowWidth() - 100);
          ImGui::TextDisabled(addons[i].hModule ? "Loaded" : "Unloaded");

          ImGui::PopID();
        }
      }
      ImGui::EndChild();

      ImGui::Dummy(ImVec2(0, 10));

      // Panel 3: Options & Actions
      ImGui::BeginChild("OptionsPanel", ImVec2(0, 150), true);
      ImGui::Text("Opzioni & Debug");
      ImGui::Separator();
      ImGui::Dummy(ImVec2(0, 10));

      ImGui::Columns(2, "OptionsCols", false);

      ImGui::NextColumn();

      // Red button style for Reload
      ImGui::PushStyleColor(ImGuiCol_Button,
                            ImVec4(0.80f, 0.10f, 0.10f, 1.00f));
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                            ImVec4(0.90f, 0.15f, 0.15f, 1.00f));
      ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                            ImVec4(1.00f, 0.20f, 0.20f, 1.00f));
      if (ImGui::Button("Reload All Addons", ImVec2(-1, 30))) {
        if (g_manager)
          g_manager->ReloadAddons();
      }
      ImGui::PopStyleColor(3);

      ImGui::Columns(1);
      ImGui::EndChild();

      ImGui::EndChild(); // ContentScroll
      ImGui::EndGroup();

      ImGui::End();
    }

    // Config Editor Window
    if (g_showConfigEditor) {
      ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiCond_FirstUseEver);
      if (ImGui::Begin("Config Editor", &g_showConfigEditor)) {
        if (ImGui::Button("Save & Close")) {
          SaveConfigEditor();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
          g_showConfigEditor = false;
        }

        ImGui::Separator();

        // InputTextMultiline
        ImGui::InputTextMultiline(
            "##source", g_configBuffer.data(), g_configBuffer.size(),
            ImVec2(-FLT_MIN, -FLT_MIN), ImGuiInputTextFlags_AllowTabInput);

        ImGui::End();
      }
    }

    // Render Addon Settings Windows
    if (g_manager) {
      auto &addons = g_manager->GetAddons();
      for (int i = 0; i < addons.size(); ++i) {
        if (addons[i].showSettings) {
          std::string windowName =
              "Settings: " + WStringToString(addons[i].name);
          ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_FirstUseEver);
          if (ImGui::Begin(windowName.c_str(), &addons[i].showSettings)) {
            g_manager->RenderAddonSettings(i);
          }
          ImGui::End();
        }
      }
    }

    ImGui::Render();
    const float clear_color_with_alpha[4] = {
        clear_color.x * clear_color.w, clear_color.y * clear_color.w,
        clear_color.z * clear_color.w, clear_color.w};
    g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, NULL);
    g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView,
                                               clear_color_with_alpha);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    g_pSwapChain->Present(1, 0);
  }

  ImGui_ImplDX11_Shutdown();
  ImGui_ImplWin32_Shutdown();
  ImGui::DestroyContext();

  CleanupDeviceD3D();
  DestroyWindow(hwnd);
  UnregisterClassW(wc.lpszClassName, wc.hInstance);

  return 0;
}

bool CreateDeviceD3D(HWND hWnd) {
  DXGI_SWAP_CHAIN_DESC sd;
  ZeroMemory(&sd, sizeof(sd));
  sd.BufferCount = 2;
  sd.BufferDesc.Width = 0;
  sd.BufferDesc.Height = 0;
  sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  sd.BufferDesc.RefreshRate.Numerator = 60;
  sd.BufferDesc.RefreshRate.Denominator = 1;
  sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
  sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  sd.OutputWindow = hWnd;
  sd.SampleDesc.Count = 1;
  sd.SampleDesc.Quality = 0;
  sd.Windowed = TRUE;
  sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

  UINT createDeviceFlags = 0;
  // createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
  D3D_FEATURE_LEVEL featureLevel;
  const D3D_FEATURE_LEVEL featureLevelArray[2] = {
      D3D_FEATURE_LEVEL_11_0,
      D3D_FEATURE_LEVEL_10_0,
  };

  // Get System Directory (where valid d3d11.dll and dxgi.dll reside)
  wchar_t systemPath[MAX_PATH];
  if (!GetSystemDirectoryW(systemPath, MAX_PATH))
    return false;
  std::wstring sysDir = systemPath;

  // 1. Create Clean DXGI Factory from System DLL
  // This bypasses any local dxgi.dll wrapper that Reshade might use.
  HMODULE hDXGI = LoadLibraryW((sysDir + L"\\dxgi.dll").c_str());
  if (!hDXGI)
    return false;

  typedef HRESULT(WINAPI * CreateDXGIFactory1Func)(REFIID, void **);
  CreateDXGIFactory1Func createFactoryFunc =
      (CreateDXGIFactory1Func)GetProcAddress(hDXGI, "CreateDXGIFactory1");
  if (!createFactoryFunc)
    return false;

  IDXGIFactory1 *pFactory = nullptr;
  if (FAILED(createFactoryFunc(__uuidof(IDXGIFactory1), (void **)&pFactory)))
    return false;

  // 2. Create D3D11 Device from System DLL
  // This bypasses any local d3d11.dll wrapper.
  HMODULE hD3D11 = LoadLibraryW((sysDir + L"\\d3d11.dll").c_str());
  if (!hD3D11) {
    pFactory->Release();
    return false;
  }

  typedef HRESULT(WINAPI * D3D11CreateDeviceFunc)(
      IDXGIAdapter *, D3D_DRIVER_TYPE, HMODULE, UINT, const D3D_FEATURE_LEVEL *,
      UINT, UINT, ID3D11Device **, D3D_FEATURE_LEVEL *, ID3D11DeviceContext **);
  D3D11CreateDeviceFunc createDeviceFunc =
      (D3D11CreateDeviceFunc)GetProcAddress(hD3D11, "D3D11CreateDevice");
  if (!createDeviceFunc) {
    pFactory->Release();
    return false;
  }

  // Use default adapter (NULL) with Hardware Driver.
  // We use the function pointer from the system DLL.
  if (FAILED(createDeviceFunc(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL,
                              createDeviceFlags, featureLevelArray, 2,
                              D3D11_SDK_VERSION, &g_pd3dDevice, &featureLevel,
                              &g_pd3dDeviceContext))) {
    pFactory->Release();
    return false;
  }

  // 3. Create SwapChain using the Clean Factory
  // This is the critical step. Creating swapchain via the factory we manually
  // loaded ensures Reshade doesn't get a chance to hook it (unless it hooked
  // the system process somehow, which is less likely for local installs).
  if (FAILED(pFactory->CreateSwapChain(g_pd3dDevice, &sd, &g_pSwapChain))) {
    g_pd3dDevice->Release();
    g_pd3dDeviceContext->Release();
    pFactory->Release();
    return false;
  }

  pFactory->Release();

  CreateRenderTarget();
  return true;
}

void CleanupDeviceD3D() {
  CleanupRenderTarget();
  if (g_pSwapChain) {
    g_pSwapChain->Release();
    g_pSwapChain = nullptr;
  }
  if (g_pd3dDeviceContext) {
    g_pd3dDeviceContext->Release();
    g_pd3dDeviceContext = nullptr;
  }
  if (g_pd3dDevice) {
    g_pd3dDevice->Release();
    g_pd3dDevice = nullptr;
  }
}

void CreateRenderTarget() {
  ID3D11Texture2D *pBackBuffer;
  g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
  g_pd3dDevice->CreateRenderTargetView(pBackBuffer, NULL,
                                       &g_mainRenderTargetView);
  pBackBuffer->Release();
}

void CleanupRenderTarget() {
  if (g_mainRenderTargetView) {
    g_mainRenderTargetView->Release();
    g_mainRenderTargetView = nullptr;
  }
}

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
    return true;

  switch (msg) {
  case WM_SIZE:
    if (g_pd3dDevice != nullptr && wParam != SIZE_MINIMIZED) {
      CleanupRenderTarget();
      g_pSwapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam),
                                  DXGI_FORMAT_UNKNOWN, 0);
      CreateRenderTarget();
    }
    return 0;
  case WM_SYSCOMMAND:
    if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
      return 0;
    break;
  case WM_DESTROY:
    PostQuitMessage(0);
    return 0;
  }
  return DefWindowProcW(hWnd, msg, wParam, lParam);
}
