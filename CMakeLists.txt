cmake_minimum_required(VERSION 3.14)
project(RunningManager CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Avoid opening console window in Release mode
if(MSVC)
    add_link_options(/SUBSYSTEM:WINDOWS)
endif()

# Fetch Dear ImGui
include(FetchContent)
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.90.1
)
FetchContent_MakeAvailable(imgui)

# Create the executable
add_executable(RunningManager WIN32)

# Source files
target_sources(RunningManager PRIVATE
    src/main.cpp
    src/ProcessManager.cpp
    src/ProcessManager.h
    src/UIManager.cpp
    src/UIManager.h
    src/SystemMonitor.cpp
)

# ImGui sources (since ImGui doesn't provide a CMake target natively that includes backends by default)
target_sources(RunningManager PRIVATE
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_dx11.cpp
)

target_include_directories(RunningManager PRIVATE
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
    src
)

# Link Windows libraries
target_link_libraries(RunningManager PRIVATE
    d3d11
    d3dcompiler
    dwmapi
    psapi
    wtsapi32
    version
    pdh
)
