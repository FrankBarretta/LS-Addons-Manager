# LS-Addons-Manager (LosslessProxy)

**LS-Addons-Manager** is a proxy DLL loader designed for **Lossless Scaling**. It intercepts function calls intended for the original `Lossless.dll` and forwards them, while simultaneously injecting a custom Addon Manager. This allows for the execution of custom code, rendering of overlays via ImGui, and modification of the application's behavior through a modular addon system.

## Features

*   **DLL Proxying:** Seamlessly replaces the original `Lossless.dll` by forwarding exports to `Lossless_original.dll`.
*   **Addon System:** Modular architecture to load external plugins (`.dll`) from an `addons` directory.
*   **ImGui Integration:** Provides a shared ImGui context to addons, allowing them to render settings windows and overlays easily.
*   **DirectX 11 Hooking:** Hooks `Present` and `ResizeBuffers` to inject custom rendering and logic.
*   **API for Developers:** Simple C-interface (`addon_api.hpp`) for creating custom addons.

## Prerequisites

*   **Operating System:** Windows 10/11 (64-bit)
*   **Build Tools:**
    *   [CMake](https://cmake.org/download/) (3.14 or later)
    *   [Visual Studio 2019/2022](https://visualstudio.microsoft.com/downloads/) with "Desktop development with C++" workload.

## Build Instructions

1.  **Clone the repository:**
    ```bash
    git clone https://github.com/yourusername/LS-Addons-Manager.git
    cd LS-Addons-Manager
    ```

2.  **Configure with CMake:**
    ```bash
    cd LosslessProxy
    mkdir build
    cd build
    cmake ..
    ```

3.  **Build the project:**
    ```bash
    cmake --build . --config Release
    ```
    
    This will generate `Lossless.dll` in the `LosslessProxy/build/Release` directory.

## Installation

1.  Navigate to your **Lossless Scaling** installation directory (e.g., via Steam: Right-click -> Manage -> Browse local files).
2.  Locate the existing `Lossless.dll` file.
3.  **Rename** `Lossless.dll` to `Lossless_original.dll`.
    *   *Note: This step is critical. The proxy DLL depends on finding `Lossless_original.dll` to function correctly.*
4.  Copy the **newly built** `Lossless.dll` (from the build folder) into the Lossless Scaling directory.
5.  Create a new folder named `addons` in the same directory.

## Usage

1.  Launch **Lossless Scaling**.
2.  The Addon Manager should initialize automatically.
3.  To install addons, place them in the `addons` folder.
    *   Structure: `addons/MyAddon/MyAddon.dll`

## Developing Addons

Refer to `src/addon_api.hpp` for the interface definition. An addon is a DLL that exports specific functions like `AddonInitialize`, `AddonRenderSettings`, etc.



⚠️ Disclaimer

This project is an unofficial add-on manager for Lossless Scaling.

It is NOT affiliated with, endorsed by, or supported by
Lossless Scaling Developers in any way.

This project is based on reverse engineering and independent analysis
of the software behavior. No proprietary source code, assets, or
confidential materials from Lossless Scaling are included.


Use at your own risk.

Using this project may violate the terms of service or license
agreement of Lossless Scaling.

The author assumes no responsibility for any damage, data loss,
account bans, or legal consequences resulting from the use of this
software.
