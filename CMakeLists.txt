cmake_minimum_required(VERSION 3.20)
project(SteamForge VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ============================================================
# Fetch Dependencies
# ============================================================
include(FetchContent)

# Dear ImGui
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.91.8
)
FetchContent_MakeAvailable(imgui)

# nlohmann/json
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
)
FetchContent_MakeAvailable(json)

# cpp-httplib (header-only HTTP client)
FetchContent_Declare(
    httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG        v0.18.3
)
FetchContent_MakeAvailable(httplib)

# ============================================================
# ImGui Library (core + DirectX11 + Win32 backend)
# ============================================================
add_library(imgui_lib STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_dx11.cpp
)
target_include_directories(imgui_lib PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)
target_link_libraries(imgui_lib PUBLIC d3d11 d3dcompiler dxgi)

# ============================================================
# SteamForge Executable
# ============================================================
file(GLOB_RECURSE STEAMFORGE_SOURCES
    src/*.cpp
    src/*.h
)

add_executable(SteamForge WIN32 ${STEAMFORGE_SOURCES})

target_include_directories(SteamForge PRIVATE
    src
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
    "C:/Program Files/MySQL/MySQL Server 8.0/include"
)

target_link_libraries(SteamForge PRIVATE
    imgui_lib
    nlohmann_json::nlohmann_json
    httplib::httplib
    d3d11
    dxgi
    d3dcompiler
    winhttp
    dwmapi
    urlmon
    windowscodecs
    ole32
    mf
    mfplat
    mfuuid
    "C:/Program Files/MySQL/MySQL Server 8.0/lib/libmysql.lib"
    bcrypt
    psapi
    iphlpapi
)

target_compile_options(SteamForge PRIVATE /utf-8)

target_compile_definitions(SteamForge PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    CPPHTTPLIB_OPENSSL_SUPPORT=0
)

# Copy resources to build dir
add_custom_command(TARGET SteamForge POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/resources
    $<TARGET_FILE_DIR:SteamForge>/resources
)

# Copy libmysql.dll to output dir
add_custom_command(TARGET SteamForge POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "C:/Program Files/MySQL/MySQL Server 8.0/lib/libmysql.dll"
    $<TARGET_FILE_DIR:SteamForge>/libmysql.dll
)
