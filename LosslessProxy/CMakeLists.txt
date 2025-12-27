cmake_minimum_required(VERSION 3.14)
project(LosslessProxy)

set(CMAKE_CXX_STANDARD 17)

# Define UNICODE for Windows GUI API
add_definitions(-DUNICODE -D_UNICODE)

include(FetchContent)
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        docking
)
FetchContent_MakeAvailable(imgui)

add_library(Lossless SHARED 
    src/main.cpp 
    src/addon_manager.cpp
    src/gui_manager.cpp
    src/shader_hook.cpp
    src/dx11_hook.cpp
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_dx11.cpp
)

target_include_directories(Lossless PRIVATE ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
target_link_libraries(Lossless d3d11 d3dcompiler)

# We need to copy the original Lossless.dll to Lossless_original.dll manually or via script
# But for the build, we just produce Lossless.dll

# Ensure we export the symbols
# The #pragma comment in main.cpp handles forwarding on MSVC.
# For MinGW, we would need a .def file.
if(MSVC)
    # No special flags needed for pragma
else()
    # Add linker flags for MinGW if needed
endif()
