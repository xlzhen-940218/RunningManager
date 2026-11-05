# RunningManager

一个使用 Dear ImGui 编写的进程管理器 / 任务管理器，支持 Windows 与 Linux（x86_64 / arm64）。

![RunningManager 界面](docs/screenshot.png)

## 功能

- 实时进程列表：PID、名称、状态、用户名、管理员权限、CPU、内存
- 按 CPU / 内存 / 名称 / PID 等列排序，支持多列排序
- 按进程名、用户名、完整路径、PID、状态搜索
- 右键菜单：窗口置前、打开文件位置、结束进程
- 系统概览：CPU、内存、磁盘占用、GPU
- 浅蓝 / 浅粉两套主题
- 自动查找 Noto CJK / 文泉驿等字体，可正确显示中文进程名
- Windows：Win32 + Direct3D 11；Linux：SDL2 + OpenGL 3

> 已在 Debian GNU/Linux 13 (trixie) aarch64 上完成构建、运行与交互验证；
> Windows 代码已使用 MinGW-w64 x86_64 交叉编译验证。

## Linux 构建（Debian / Ubuntu）

依赖：

```bash
sudo apt install build-essential cmake pkg-config \
    libsdl2-dev libgl1-mesa-dev libx11-dev \
    fonts-noto-cjk
```

说明：

- 需要 CMake ≥ 3.16、支持 C++17 的编译器、SDL2（建议 ≥ 2.0.16）和 OpenGL。
- `libx11-dev` 用于 EWMH 的“窗口置前”和“GUI 进程识别”。未安装时仍可编译，
  置前会退化为调用 `xdotool`（如果系统中有）。
- `fonts-noto-cjk` 用于中文显示；也可以使用 `fonts-wqy-microhei` 等字体。
- 首次配置会通过 CMake `FetchContent` 从 GitHub 拉取 Dear ImGui v1.90.1，需要网络。
  离线构建可指定本地源码：`-DFETCHCONTENT_SOURCE_DIR_IMGUI=/path/to/imgui`。

构建与运行：

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
./build/RunningManager
```

安装到系统（只安装可执行文件）：

```bash
sudo cmake --install build --prefix /usr/local
RunningManager
```

## Windows 构建

使用 Visual Studio 或 MinGW：

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
```

生成的 `RunningManager.exe` 位于 `build/Release/`。

## 平台差异

| 功能 | Windows | Linux |
| --- | --- | --- |
| 进程枚举 | Toolhelp32 + PSAPI | `/proc` |
| CPU | `GetSystemTimes` / `GetProcessTimes` | `/proc/stat` / `/proc/<pid>/stat` |
| 内存 | `GlobalMemoryStatusEx` | `/proc/meminfo` |
| 磁盘 | `GetLogicalDriveStrings` | `/proc/self/mounts` + `statvfs` |
| GPU 名称 | DXGI | `glGetString(GL_RENDERER)` |
| GPU 占用 | PDH GPU Engine 计数器 | AMD `gpu_busy_percent` / NVIDIA `nvidia-smi`，否则显示 N/A |
| 进程图标 | 可执行文件图标 | 无系统级图标，使用首字母/汉字彩色徽标 |
| 管理员 | UAC 提升状态 | 有效 UID 为 0（root） |
| 结束进程 | `TerminateProcess` | `SIGKILL` |
| 打开文件位置 | 资源管理器 `/select` | `xdg-open` 打开所在目录 |
| 窗口置前 | `SetForegroundWindow` | X11 EWMH `_NET_ACTIVE_WINDOW`，回退 `xdotool` |

Linux 说明：

- 内核线程会被过滤，不显示在列表中。
- 进程状态显示为 Running / Sleeping / Disk Wait / Zombie / Stopped 等。
- 在没有硬件 GPU、使用 llvmpipe 等软件渲染时，界面只在有输入或每秒刷新数据时重绘，
  以降低 CPU 占用。
- 窗口设置保存在 `~/.config/runningmanager/imgui.ini`（遵循 `XDG_CONFIG_HOME`）。
- 程序会优先把窗口放到第二块显示器中央，并保持置顶；只有一块显示器时在主屏居中。

## 常见问题

**配置时报 `Could NOT find SDL2`**

安装 `libsdl2-dev`，并确认 `pkg-config --modversion sdl2` 能输出版本号。

**启动时报 `SDL_GL_CreateContext failed`**

当前会话可能没有可用的 OpenGL 上下文。先运行 `glxinfo -B` 检查渲染器；
无硬件加速时通常仍会提供 llvmpipe 软件渲染。远程/无显示环境下需要 X11 或 Wayland 会话。

**中文进程名显示为方块**

安装任意中文字体即可，例如 `fonts-noto-cjk`、`fonts-wqy-microhei` 或
`fonts-wqy-zenhei`。程序会自动从常见路径中选择。

**GPU 显示 N/A**

Linux 没有统一的 GPU 占用接口。程序支持 AMD 的
`/sys/class/drm/card*/device/gpu_busy_percent` 和 NVIDIA 的 `nvidia-smi`；
其他驱动（包括 llvmpipe 软件渲染）会如实显示 N/A。

## 项目结构

```
src/
  main.cpp                  Windows 入口（Win32 + DX11）
  main_linux.cpp            Linux 入口（SDL2 + OpenGL3）
  ProcessManager.h          跨平台进程数据接口
  ProcessManager_win.cpp    Windows 进程实现
  ProcessManager_linux.cpp  Linux /proc 实现
  SystemMonitor.h           跨平台系统信息接口
  SystemMonitor_win.cpp     Windows 系统信息实现
  SystemMonitor_linux.cpp   Linux 系统信息实现
  UIManager.cpp/.h          共享 ImGui 界面
docs/
  screenshot.png            运行截图
```

## 许可证

见 [LICENSE](LICENSE)。
