#include "ProcessManager.h"

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <fstream>
#include <map>
#include <pwd.h>
#include <set>
#include <signal.h>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

#ifdef RM_HAVE_X11
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#endif

namespace {

std::string Trim(const std::string& input) {
    const auto first = input.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    const auto last = input.find_last_not_of(" \t\r\n");
    return input.substr(first, last - first + 1);
}

bool ReadWholeFile(const std::string& path, std::string& out) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return false;
    std::ostringstream ss;
    ss << file.rdbuf();
    out = ss.str();
    return true;
}

std::string ReadLink(const std::string& path) {
    char buffer[4096];
    const ssize_t length = readlink(path.c_str(), buffer, sizeof(buffer) - 1);
    if (length <= 0) return {};
    buffer[length] = '\0';
    return std::string(buffer);
}

std::string BaseName(const std::string& path) {
    if (path.empty()) return {};
    const auto slash = path.find_last_of('/');
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

std::string StripDeletedSuffix(const std::string& path) {
    static const std::string kDeleted = " (deleted)";
    if (path.size() > kDeleted.size() &&
        path.compare(path.size() - kDeleted.size(), kDeleted.size(), kDeleted) == 0) {
        return path.substr(0, path.size() - kDeleted.size());
    }
    return path;
}

std::string GetUsernameForUid(uid_t uid) {
    static std::map<uid_t, std::string> cache;
    const auto cached = cache.find(uid);
    if (cached != cache.end()) return cached->second;

    std::string name;
    struct passwd pwd;
    struct passwd* result = nullptr;
    char buffer[4096];
    if (getpwuid_r(uid, &pwd, buffer, sizeof(buffer), &result) == 0 && result != nullptr) {
        name = result->pw_name ? result->pw_name : "";
    }
    if (name.empty()) name = std::to_string(static_cast<unsigned long>(uid));

    cache[uid] = name;
    return name;
}

uint64_t ReadTotalJiffies() {
    std::ifstream file("/proc/stat");
    std::string line;
    if (!std::getline(file, line)) return 0;

    std::istringstream iss(line);
    std::string cpuLabel;
    iss >> cpuLabel; // "cpu"

    uint64_t total = 0;
    uint64_t value = 0;
    // user nice system idle iowait irq softirq steal
    for (int i = 0; i < 8 && (iss >> value); ++i) total += value;
    return total;
}

struct ProcTimes {
    uint64_t total = 0;
};

bool ReadProcTimes(pid_t pid, ProcTimes& out) {
    std::ifstream file("/proc/" + std::to_string(pid) + "/stat");
    std::string line;
    if (!std::getline(file, line)) return false;

    const auto left = line.find('(');
    const auto right = line.rfind(')');
    if (left == std::string::npos || right == std::string::npos || right + 2 >= line.size())
        return false;

    std::istringstream iss(line.substr(right + 2));
    std::string token;

    iss >> token; // state (field 3)
    // fields 4..13: ppid pgrp session tty_nr tpgid flags minflt cminflt majflt cmajflt
    for (int i = 0; i < 10; ++i) iss >> token;

    uint64_t utime = 0;
    uint64_t stime = 0;
    if (!(iss >> utime >> stime)) return false;

    out.total = utime + stime;
    return true;
}

struct ProcStatus {
    std::string name;
    char state = 0;
    uid_t uid = 0;
    uint64_t rssBytes = 0;
};

bool ReadProcStatus(pid_t pid, ProcStatus& out) {
    std::ifstream file("/proc/" + std::to_string(pid) + "/status");
    if (!file) return false;

    bool haveUid = false;
    std::string line;
    while (std::getline(file, line)) {
        if (line.rfind("Name:", 0) == 0) {
            out.name = Trim(line.substr(5));
        } else if (line.rfind("State:", 0) == 0) {
            const std::string value = Trim(line.substr(6));
            if (!value.empty()) out.state = value[0];
        } else if (line.rfind("Uid:", 0) == 0) {
            std::istringstream iss(line.substr(4));
            std::vector<unsigned long> uids;
            unsigned long value = 0;
            while (iss >> value) uids.push_back(value);
            if (!uids.empty()) {
                out.uid = static_cast<uid_t>(uids.size() > 1 ? uids[1] : uids[0]);
                haveUid = true;
            }
        } else if (line.rfind("VmRSS:", 0) == 0) {
            std::istringstream iss(line.substr(6));
            uint64_t kilobytes = 0;
            if (iss >> kilobytes) out.rssBytes = kilobytes * 1024ull;
        }
    }
    return haveUid || !out.name.empty();
}

#ifdef RM_HAVE_X11

bool IsSkippableWindowType(Display* display, Window window) {
    const Atom typeAtom = XInternAtom(display, "_NET_WM_WINDOW_TYPE", True);
    if (typeAtom == None) return false;

    Atom actualType = None;
    int actualFormat = 0;
    unsigned long itemCount = 0;
    unsigned long bytesAfter = 0;
    unsigned char* data = nullptr;
    if (XGetWindowProperty(display, window, typeAtom, 0, 32, False, XA_ATOM, &actualType,
                           &actualFormat, &itemCount, &bytesAfter, &data) != Success ||
        data == nullptr) {
        return false;
    }

    const Atom dockAtom = XInternAtom(display, "_NET_WM_WINDOW_TYPE_DOCK", True);
    const Atom desktopAtom = XInternAtom(display, "_NET_WM_WINDOW_TYPE_DESKTOP", True);
    const Atom tooltipAtom = XInternAtom(display, "_NET_WM_WINDOW_TYPE_TOOLTIP", True);
    const Atom notificationAtom = XInternAtom(display, "_NET_WM_WINDOW_TYPE_NOTIFICATION", True);

    bool skip = false;
    const Atom* types = reinterpret_cast<const Atom*>(data);
    for (unsigned long i = 0; i < itemCount; ++i) {
        if (types[i] == dockAtom || types[i] == desktopAtom || types[i] == tooltipAtom ||
            types[i] == notificationAtom) {
            skip = true;
            break;
        }
    }
    XFree(data);
    return skip;
}

bool GetWindowPid(Display* display, Window window, pid_t& pid) {
    const Atom pidAtom = XInternAtom(display, "_NET_WM_PID", True);
    if (pidAtom == None) return false;

    Atom actualType = None;
    int actualFormat = 0;
    unsigned long itemCount = 0;
    unsigned long bytesAfter = 0;
    unsigned char* data = nullptr;
    if (XGetWindowProperty(display, window, pidAtom, 0, 1, False, XA_CARDINAL, &actualType,
                           &actualFormat, &itemCount, &bytesAfter, &data) != Success ||
        data == nullptr) {
        return false;
    }

    bool ok = false;
    if (itemCount > 0) {
        pid = static_cast<pid_t>(*reinterpret_cast<unsigned long*>(data));
        ok = pid > 0;
    }
    XFree(data);
    return ok;
}

std::vector<Window> GetClientWindows(Display* display) {
    std::vector<Window> windows;
    const Atom clientListAtom = XInternAtom(display, "_NET_CLIENT_LIST", True);
    if (clientListAtom == None) return windows;

    Atom actualType = None;
    int actualFormat = 0;
    unsigned long itemCount = 0;
    unsigned long bytesAfter = 0;
    unsigned char* data = nullptr;
    if (XGetWindowProperty(display, DefaultRootWindow(display), clientListAtom, 0, (~0L), False,
                           XA_WINDOW, &actualType, &actualFormat, &itemCount, &bytesAfter,
                           &data) != Success ||
        data == nullptr) {
        return windows;
    }

    const Window* list = reinterpret_cast<const Window*>(data);
    windows.assign(list, list + itemCount);
    XFree(data);
    return windows;
}

Window FindWindowForPid(Display* display, pid_t pid) {
    Window fallback = 0;
    for (Window window : GetClientWindows(display)) {
        XWindowAttributes attributes;
        if (!XGetWindowAttributes(display, window, &attributes)) continue;
        if (IsSkippableWindowType(display, window)) continue;

        pid_t windowPid = 0;
        if (!GetWindowPid(display, window, windowPid) || windowPid != pid) continue;

        // Prefer a normal, visible top-level window. Windows that are
        // minimized/unmapped are still kept as a fallback so they can be
        // restored by the window manager.
        if (!attributes.override_redirect && attributes.map_state == IsViewable) return window;
        if (!fallback) fallback = window;
    }
    return fallback;
}

std::set<pid_t> CollectGuiPids() {
    std::set<pid_t> pids;
    Display* display = XOpenDisplay(nullptr);
    if (!display) return pids;

    for (Window window : GetClientWindows(display)) {
        XWindowAttributes attributes;
        if (!XGetWindowAttributes(display, window, &attributes)) continue;
        if (attributes.map_state != IsViewable) continue;
        if (IsSkippableWindowType(display, window)) continue;

        pid_t pid = 0;
        if (GetWindowPid(display, window, pid)) pids.insert(pid);
    }

    XCloseDisplay(display);
    return pids;
}

#else

std::set<pid_t> CollectGuiPids() { return {}; }

#endif // RM_HAVE_X11

} // namespace

ProcessManager::ProcessManager() = default;

ProcessManager::~ProcessManager() = default;

void ProcessManager::Init() {
    m_prevTotalJiffies = ReadTotalJiffies();
}

void ProcessManager::RefreshProcesses() {
    const uint64_t totalJiffies = ReadTotalJiffies();
    const std::set<pid_t> guiPids = CollectGuiPids();

    std::map<uint32_t, ProcessInfo> newProcessMap;

    DIR* procDir = opendir("/proc");
    if (!procDir) return;

    while (dirent* entry = readdir(procDir)) {
        const char* name = entry->d_name;
        if (!name || !*name) continue;

        bool numeric = true;
        for (const char* p = name; *p; ++p) {
            if (*p < '0' || *p > '9') {
                numeric = false;
                break;
            }
        }
        if (!numeric) continue;

        const pid_t pid = static_cast<pid_t>(std::strtol(name, nullptr, 10));
        if (pid <= 0) continue;

        const std::string procBase = std::string("/proc/") + name;

        ProcStatus status;
        if (!ReadProcStatus(pid, status)) continue;

        const std::string exe = StripDeletedSuffix(ReadLink(procBase + "/exe"));
        std::string cmdline;
        ReadWholeFile(procBase + "/cmdline", cmdline);

        // Kernel threads have neither an executable nor a command line. They
        // are not interesting to a user-facing process explorer.
        if (exe.empty() && cmdline.empty()) continue;

        ProcessInfo info;
        info.pid = static_cast<uint32_t>(pid);

        std::string displayName = BaseName(exe);
        if (displayName.empty()) displayName = status.name;
        if (displayName.empty()) {
            std::string comm;
            ReadWholeFile(procBase + "/comm", comm);
            displayName = Trim(comm);
        }
        if (displayName.empty()) displayName = "pid " + std::to_string(pid);

        info.name = displayName;
        info.fullPath = exe;
        info.username = GetUsernameForUid(status.uid);
        info.isAdmin = (status.uid == 0);
        info.memoryUsageBytes = status.rssBytes;
        info.state = status.state;
        info.isNotResponding = (status.state == 'Z');
        info.hasMainWindow = guiPids.find(pid) != guiPids.end();

        ProcTimes times;
        if (ReadProcTimes(pid, times)) {
            info.lastProcessTime = times.total;
            info.lastSystemTime = totalJiffies;
            info.validTime = true;

            const auto previous = m_processMap.find(info.pid);
            if (previous != m_processMap.end() && previous->second.validTime &&
                totalJiffies > m_prevTotalJiffies &&
                times.total >= previous->second.lastProcessTime) {
                const uint64_t systemDelta = totalJiffies - m_prevTotalJiffies;
                const uint64_t processDelta = times.total - previous->second.lastProcessTime;
                if (systemDelta > 0) {
                    info.cpuUsage = static_cast<double>(processDelta) /
                                    static_cast<double>(systemDelta) * 100.0;
                    info.cpuUsage = std::clamp(info.cpuUsage, 0.0, 100.0);
                }
            }
        }

        newProcessMap[info.pid] = std::move(info);
    }

    closedir(procDir);

    m_processMap = std::move(newProcessMap);
    m_prevTotalJiffies = totalJiffies;

    // Build list. std::map keeps the list ordered by PID.
    m_processList.clear();
    m_processList.reserve(m_processMap.size());
    for (const auto& pair : m_processMap) m_processList.push_back(pair.second);
}

const std::vector<ProcessInfo>& ProcessManager::GetProcesses() const {
    return m_processList;
}

void ProcessManager::KillProcess(uint32_t pid) {
    // Never let the UI take down init or itself/its parent by accident.
    if (pid <= 1) return;
    if (kill(static_cast<pid_t>(pid), SIGKILL) != 0) {
        std::fprintf(stderr, "RunningManager: could not kill pid %u: %s\n",
                     static_cast<unsigned>(pid), std::strerror(errno));
    }
}

void ProcessManager::BringToFront(uint32_t pid) {
    if (pid <= 1) return;

#ifdef RM_HAVE_X11
    Display* display = XOpenDisplay(nullptr);
    if (display) {
        const Window window = FindWindowForPid(display, static_cast<pid_t>(pid));
        if (window) {
            const Atom activeWindowAtom = XInternAtom(display, "_NET_ACTIVE_WINDOW", False);
            XEvent event = {};
            event.xclient.type = ClientMessage;
            event.xclient.window = window;
            event.xclient.message_type = activeWindowAtom;
            event.xclient.format = 32;
            event.xclient.data.l[0] = 2; // source indication: pager
            event.xclient.data.l[1] = CurrentTime;
            event.xclient.data.l[2] = 0;

            // Ask the window manager to activate/restore the window, then make
            // sure input focus actually follows. Some WMs ignore
            // _NET_ACTIVE_WINDOW when the request carries CurrentTime.
            XSendEvent(display, DefaultRootWindow(display), False,
                       SubstructureRedirectMask | SubstructureNotifyMask, &event);
            XMapRaised(display, window);
            XSetInputFocus(display, window, RevertToPointerRoot, CurrentTime);
            XFlush(display);
            XCloseDisplay(display);
            return;
        }
        XCloseDisplay(display);
    }
#endif

    // X11/Wayland fallback when the process was not found through EWMH.
    const std::string probe = "command -v xdotool >/dev/null 2>&1";
    if (std::system(probe.c_str()) == 0) {
        const std::string command = "xdotool search --onlyvisible --pid " +
                                    std::to_string(pid) +
                                    " windowactivate --sync >/dev/null 2>&1";
        (void)std::system(command.c_str());
    }
}

void ProcessManager::OpenFileLocation(const std::string& path) {
    if (path.empty()) return;

    std::string directory = path;
    const auto slash = directory.find_last_of('/');
    if (slash != std::string::npos)
        directory = directory.substr(0, slash);
    else
        directory.clear();
    if (directory.empty()) directory = "/";

    // Double-fork so the file manager is reparented to init and we do not
    // accumulate zombie processes.
    const pid_t child = fork();
    if (child == 0) {
        const pid_t grandChild = fork();
        if (grandChild == 0) {
            setsid();
            execlp("xdg-open", "xdg-open", directory.c_str(), static_cast<char*>(nullptr));
            _exit(127);
        }
        _exit(0);
    }
    if (child > 0) waitpid(child, nullptr, 0);
}
