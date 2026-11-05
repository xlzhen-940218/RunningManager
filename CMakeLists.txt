cmake_minimum_required(VERSION 3.16)
project(RunningManager VERSION 1.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS OFF)

# Fetch Dear ImGui
include(FetchContent)
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.90.1
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(imgui)

add_executable(RunningManager)

# Dear ImGui core
target_sources(RunningManager PRIVATE
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
)

# Shared UI
target_sources(RunningManager PRIVATE
    src/UIManager.cpp
    src/UIManager.h
)

target_include_directories(RunningManager PRIVATE
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
    src
)

if(WIN32)
    # ---------------------------------------------------------------------
    # Windows (Win32 + Direct3D 11)
    # ---------------------------------------------------------------------
    enable_language(RC)
    set_target_properties(RunningManager PROPERTIES WIN32_EXECUTABLE TRUE)

    target_sources(RunningManager PRIVATE
        src/main.cpp
        src/ProcessManager_win.cpp
        src/ProcessManager.h
        src/SystemMonitor_win.cpp
        src/SystemMonitor.h
        src/resource.h
        src/RunningManager.rc
        ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_dx11.cpp
    )

    target_compile_definitions(RunningManager PRIVATE
        UNICODE
        _UNICODE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )

    target_link_libraries(RunningManager PRIVATE
        d3d11
        d3dcompiler
        dwmapi
        psapi
        wtsapi32
        version
        pdh
    )
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # ---------------------------------------------------------------------
    # Linux (SDL2 + OpenGL 3, /proc based process and system information)
    # ---------------------------------------------------------------------
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
    find_package(OpenGL REQUIRED)
    find_package(Threads REQUIRED)

    target_sources(RunningManager PRIVATE
        src/main_linux.cpp
        src/ProcessManager_linux.cpp
        src/ProcessManager.h
        src/SystemMonitor_linux.cpp
        src/SystemMonitor.h
        ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl2.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
    )

    target_link_libraries(RunningManager PRIVATE
        PkgConfig::SDL2
        OpenGL::GL
        Threads::Threads
    )

    # EWMH integration for "Bring to Front" and GUI-process detection.
    find_package(X11)
    if(X11_FOUND)
        target_compile_definitions(RunningManager PRIVATE RM_HAVE_X11=1)
        target_link_libraries(RunningManager PRIVATE X11::X11)
    else()
        message(STATUS "X11 headers not found: window activation will use xdotool when available")
    endif()

    target_compile_options(RunningManager PRIVATE -Wall -Wextra)

    install(TARGETS RunningManager RUNTIME DESTINATION bin)
else()
    message(FATAL_ERROR "RunningManager currently supports Windows and Linux only")
endif()
