#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_sdl2.h"

#include <SDL.h>
#include <SDL_opengl.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

#include "ProcessManager.h"
#include "SystemMonitor.h"
#include "UIManager.h"

#ifdef RM_HAVE_X11
#include <SDL_syswm.h>
#include <X11/Xlib.h>
#endif

namespace {

#ifdef RM_HAVE_X11
int HandleXError(Display* display, XErrorEvent* event) {
    // Window teardown races (SDL/XInput2, window manager) should not abort the
    // process. Anything more serious is still reported for debugging.
    if (event->error_code == BadWindow || event->error_code == BadDrawable ||
        event->error_code == BadMatch || event->error_code == BadValue) {
        return 0;
    }

    char text[256] = {};
    XGetErrorText(display, event->error_code, text, sizeof(text));
    std::fprintf(stderr, "RunningManager: X error: %s (request %d.%d)\n", text,
                 event->request_code, event->minor_code);
    return 0;
}
#endif

constexpr int kWindowWidth = 1020;
constexpr int kWindowHeight = 720;
constexpr int kIconSize = 64;

float RoundedRectCoverage(float px, float py, float left, float top, float right, float bottom,
                          float radius) {
    const float centerX = (left + right) * 0.5f;
    const float centerY = (top + bottom) * 0.5f;
    const float halfW = (right - left) * 0.5f;
    const float halfH = (bottom - top) * 0.5f;

    const float qx = std::fabs(px - centerX) - (halfW - radius);
    const float qy = std::fabs(py - centerY) - (halfH - radius);
    const float outsideX = std::max(qx, 0.0f);
    const float outsideY = std::max(qy, 0.0f);
    const float distance =
        std::sqrt(outsideX * outsideX + outsideY * outsideY) + std::min(std::max(qx, qy), 0.0f) -
        radius;

    // 1px anti-aliasing band.
    return std::clamp(0.5f - distance, 0.0f, 1.0f);
}

void BlendPixel(std::vector<unsigned char>& pixels, int width, int x, int y, float r, float g,
                float b, float a) {
    if (x < 0 || y < 0 || x >= width) return;
    unsigned char* pixel = &pixels[(static_cast<size_t>(y) * width + x) * 4];
    const float dstA = pixel[3] / 255.0f;
    const float outA = a + dstA * (1.0f - a);
    if (outA <= 0.0f) {
        pixel[0] = pixel[1] = pixel[2] = pixel[3] = 0;
        return;
    }
    pixel[0] = static_cast<unsigned char>(
        std::clamp((r * a + pixel[0] / 255.0f * dstA * (1.0f - a)) / outA, 0.0f, 1.0f) * 255.0f);
    pixel[1] = static_cast<unsigned char>(
        std::clamp((g * a + pixel[1] / 255.0f * dstA * (1.0f - a)) / outA, 0.0f, 1.0f) * 255.0f);
    pixel[2] = static_cast<unsigned char>(
        std::clamp((b * a + pixel[2] / 255.0f * dstA * (1.0f - a)) / outA, 0.0f, 1.0f) * 255.0f);
    pixel[3] = static_cast<unsigned char>(outA * 255.0f);
}

std::vector<unsigned char> MakeAppIconPixels() {
    const int size = kIconSize;
    std::vector<unsigned char> pixels(static_cast<size_t>(size) * size * 4, 0);

    const float inset = 1.0f;
    const float outerRadius = size * 0.22f;

    // Three ascending white bars, like the fallback drawn by UIManager.
    const float barWidth = size * 0.14f;
    const float gap = size * 0.10f;
    const float barLeft = (size - (barWidth * 3.0f + gap * 2.0f)) * 0.5f;
    const float barBottom = size * 0.74f;
    const float barHeights[3] = {size * 0.26f, size * 0.40f, size * 0.54f};

    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            const float px = x + 0.5f;
            const float py = y + 0.5f;

            const float bgCoverage = RoundedRectCoverage(
                px, py, inset, inset, size - inset, size - inset, outerRadius);
            if (bgCoverage <= 0.0f) continue;

            const float t = py / size;
            const float r = 0.24f + 0.12f * t;
            const float g = 0.55f + 0.10f * t;
            const float b = 0.94f - 0.10f * t;
            BlendPixel(pixels, size, x, y, r, g, b, bgCoverage);

            for (int i = 0; i < 3; ++i) {
                const float left = barLeft + i * (barWidth + gap);
                const float right = left + barWidth;
                const float bottom = barBottom;
                const float top = bottom - barHeights[i];
                const float barCoverage =
                    RoundedRectCoverage(px, py, left, top, right, bottom, barWidth * 0.35f);
                if (barCoverage > 0.0f) {
                    BlendPixel(pixels, size, x, y, 1.0f, 1.0f, 1.0f,
                               barCoverage * bgCoverage);
                }
            }
        }
    }

    return pixels;
}

ImTextureID CreateTextureFromPixels(int width, int height,
                                    const std::vector<unsigned char>& pixels) {
    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 pixels.data());
    glBindTexture(GL_TEXTURE_2D, 0);
    return (ImTextureID)(intptr_t)texture;
}

void SetWindowIcon(SDL_Window* window) {
    const std::vector<unsigned char> pixels = MakeAppIconPixels();
    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormatFrom(
        const_cast<unsigned char*>(pixels.data()), kIconSize, kIconSize, 32, kIconSize * 4,
        SDL_PIXELFORMAT_RGBA32);
    if (surface) {
        SDL_SetWindowIcon(window, surface);
        SDL_FreeSurface(surface);
    }
}

float GetUiScale(int displayIndex) {
    float diagonalDpi = 0.0f;
    float horizontalDpi = 0.0f;
    float verticalDpi = 0.0f;
    if (SDL_GetDisplayDPI(displayIndex, &diagonalDpi, &horizontalDpi, &verticalDpi) == 0) {
        float dpi = horizontalDpi > 0.0f ? horizontalDpi : diagonalDpi;
        if (dpi <= 0.0f) return 1.0f;
        const float scale = dpi / 96.0f;
        if (scale >= 1.25f) return std::clamp(scale, 1.0f, 3.0f);
    }
    return 1.0f;
}

std::string GetSettingsPath() {
    const char* configHome = std::getenv("XDG_CONFIG_HOME");
    std::string base;
    if (configHome && *configHome) {
        base = configHome;
    } else {
        const char* home = std::getenv("HOME");
        if (!home || !*home) return {};
        base = std::string(home) + "/.config";
    }

    base += "/runningmanager";
    mkdir(base.c_str(), 0700);
    return base + "/imgui.ini";
}

bool IsSoftwareRenderer() {
    const GLubyte* renderer = glGetString(GL_RENDERER);
    if (!renderer) return false;
    std::string name = reinterpret_cast<const char*>(renderer);
    std::transform(name.begin(), name.end(), name.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return name.find("llvmpipe") != std::string::npos ||
           name.find("softpipe") != std::string::npos ||
           name.find("swrast") != std::string::npos;
}

} // namespace

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

#ifdef RM_HAVE_X11
    XSetErrorHandler(HandleXError);
#endif

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        std::fprintf(stderr, "RunningManager: SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    const char* glslVersion = "#version 130";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

    int displayIndex = 0;
    if (SDL_GetNumVideoDisplays() > 1) {
        // Match the Windows behaviour: prefer the first secondary monitor.
        displayIndex = 1;
    }

    const float uiScale = GetUiScale(displayIndex);
    const int windowWidth = static_cast<int>(kWindowWidth * uiScale);
    const int windowHeight = static_cast<int>(kWindowHeight * uiScale);

    SDL_Rect displayBounds = {0, 0, windowWidth, windowHeight};
    SDL_GetDisplayBounds(displayIndex, &displayBounds);
    const int windowX = displayBounds.x + (displayBounds.w - windowWidth) / 2;
    const int windowY = displayBounds.y + (displayBounds.h - windowHeight) / 2;

    SDL_Window* window = SDL_CreateWindow(
        "RunningManager - Process Explorer", windowX, windowY, windowWidth, windowHeight,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!window) {
        std::fprintf(stderr, "RunningManager: SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_GLContext glContext = SDL_GL_CreateContext(window);
    if (!glContext) {
        // Fall back to OpenGL 2.1 for older drivers.
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
        glContext = SDL_GL_CreateContext(window);
        glslVersion = "#version 120";
    }
    if (!glContext) {
        std::fprintf(stderr, "RunningManager: SDL_GL_CreateContext failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    SDL_GL_MakeCurrent(window, glContext);
    SDL_GL_SetSwapInterval(1);
    SDL_SetWindowMinimumSize(window, static_cast<int>(900 * uiScale),
                             static_cast<int>(600 * uiScale));
    SetWindowIcon(window);

    // The original application always keeps its window on top.
    SDL_SetWindowAlwaysOnTop(window, SDL_TRUE);

#ifdef RM_HAVE_X11
    // Used to notice if the window is destroyed behind SDL's back (for example
    // by a window manager crash or a forced close).
    Display* xDisplay = nullptr;
    Window xWindow = 0;
    SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);
    if (SDL_GetWindowWMInfo(window, &wmInfo) &&
        wmInfo.subsystem == SDL_SYSWM_X11) {
        xDisplay = wmInfo.info.x11.display;
        xWindow = wmInfo.info.x11.window;
    }
#endif

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    const std::string settingsPath = GetSettingsPath();
    io.IniFilename = settingsPath.empty() ? nullptr : settingsPath.c_str();

    if (uiScale > 1.0f) ImGui::GetStyle().ScaleAllSizes(uiScale);
    const UIFonts fonts = UIManager::LoadFonts(io, uiScale);

    ImGui_ImplSDL2_InitForOpenGL(window, glContext);
    ImGui_ImplOpenGL3_Init(glslVersion);

    const std::vector<unsigned char> iconPixels = MakeAppIconPixels();
    const ImTextureID appIcon = CreateTextureFromPixels(kIconSize, kIconSize, iconPixels);

    ProcessManager processManager;
    processManager.Init();
    SystemMonitor systemMonitor;
    systemMonitor.Init();
    UIManager uiManager(&processManager, &systemMonitor, appIcon, fonts, uiScale);

    processManager.RefreshProcesses();
    auto lastRefresh = std::chrono::steady_clock::now();

    // The window is only redrawn after input or a data refresh. This keeps the
    // application close to idle when nothing is happening, which matters a lot
    // on software OpenGL (llvmpipe) or power-constrained ARM devices.
    const bool softwareRenderer = IsSoftwareRenderer();
    const auto targetFrameTime = std::chrono::milliseconds(softwareRenderer ? 33 : 16);
    const auto idleTimeout = std::chrono::milliseconds(900);
    auto lastActivity = std::chrono::steady_clock::now();

    bool done = false;
    while (!done) {
        bool hadEvent = false;
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            hadEvent = true;
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT) done = true;
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE &&
                event.window.windowID == SDL_GetWindowID(window)) {
                done = true;
            }
        }
        if (done) break;

#ifdef RM_HAVE_X11
        if (xDisplay && xWindow) {
            XWindowAttributes attributes;
            if (!XGetWindowAttributes(xDisplay, xWindow, &attributes)) {
                // SDL_DestroyWindow() waits for the window's DestroyNotify; if the
                // window was destroyed behind SDL's back it would wait forever.
                std::_Exit(0);
            }
        }
#endif

        const auto now = std::chrono::steady_clock::now();
        if (hadEvent) lastActivity = now;

        bool refreshed = false;
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastRefresh).count() >=
            1000) {
            processManager.RefreshProcesses();
            systemMonitor.Update();
            lastRefresh = now;
            refreshed = true;
        }

        if (SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED) {
            SDL_Delay(20);
            continue;
        }

        if (!hadEvent && !refreshed && now - lastActivity >= idleTimeout) {
            SDL_Delay(20);
            continue;
        }

        const auto frameStart = std::chrono::steady_clock::now();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        io.DeltaTime = std::min(io.DeltaTime, 1.0f / 30.0f);
        ImGui::NewFrame();

        uiManager.Render();

        ImGui::Render();

        int drawableWidth = 0;
        int drawableHeight = 0;
        SDL_GL_GetDrawableSize(window, &drawableWidth, &drawableHeight);
        glViewport(0, 0, drawableWidth, drawableHeight);
        const ImVec4 clearColor = uiManager.GetBackgroundColor();
        glClearColor(clearColor.x, clearColor.y, clearColor.z, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        SDL_GL_SwapWindow(window);

        const auto frameElapsed = std::chrono::steady_clock::now() - frameStart;
        if (frameElapsed < targetFrameTime) {
            const auto remaining = targetFrameTime - frameElapsed;
            SDL_Delay(static_cast<Uint32>(
                std::chrono::duration_cast<std::chrono::milliseconds>(remaining).count()));
        }
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    if (appIcon) {
        const GLuint texture = static_cast<GLuint>(reinterpret_cast<intptr_t>(appIcon));
        glDeleteTextures(1, &texture);
    }

    SDL_GL_DeleteContext(glContext);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
