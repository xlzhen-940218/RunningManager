#include "SystemMonitor.h"

#include <GL/gl.h>
#include <glob.h>
#include <mntent.h>
#include <sys/statvfs.h>
#include <sys/utsname.h>
#include <unistd.h>

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <set>
#include <sstream>
#include <string>

namespace {

std::string Trim(const std::string& input) {
    const auto first = input.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    const auto last = input.find_last_not_of(" \t\r\n");
    return input.substr(first, last - first + 1);
}

bool ReadUint64File(const std::string& path, uint64_t& value) {
    std::ifstream file(path);
    if (!(file >> value)) return false;
    return true;
}

std::string ArmCpuPartName(uint64_t part) {
    switch (part) {
        case 0xd03: return "Cortex-A53";
        case 0xd04: return "Cortex-A35";
        case 0xd05: return "Cortex-A55";
        case 0xd06: return "Cortex-A65";
        case 0xd07: return "Cortex-A57";
        case 0xd08: return "Cortex-A72";
        case 0xd09: return "Cortex-A73";
        case 0xd0a: return "Cortex-A75";
        case 0xd0b: return "Cortex-A76";
        case 0xd0c: return "Neoverse-N1";
        case 0xd0d: return "Cortex-A77";
        case 0xd40: return "Neoverse-V1";
        case 0xd41: return "Cortex-A78";
        case 0xd44: return "Cortex-X1";
        case 0xd46: return "Cortex-A510";
        case 0xd47: return "Cortex-A710";
        case 0xd48: return "Cortex-X2";
        case 0xd49: return "Neoverse-N2";
        case 0xd4d: return "Cortex-A715";
        case 0xd4e: return "Cortex-X3";
        case 0xd80: return "Cortex-A520";
        default: return {};
    }
}

std::string ReadCpuName() {
    std::ifstream cpuinfo("/proc/cpuinfo");
    std::string line;
    std::string modelName;
    std::string hardware;
    uint64_t cpuPart = 0;
    bool haveCpuPart = false;

    while (std::getline(cpuinfo, line)) {
        const auto colon = line.find(':');
        if (colon == std::string::npos) continue;
        const std::string key = Trim(line.substr(0, colon));
        const std::string value = Trim(line.substr(colon + 1));

        if (key == "model name" || key == "Model" || key == "Processor") {
            if (modelName.empty()) modelName = value;
        } else if (key == "Hardware") {
            if (hardware.empty()) hardware = value;
        } else if (key == "CPU part" && !haveCpuPart) {
            char* end = nullptr;
            const unsigned long long parsed = std::strtoull(value.c_str(), &end, 0);
            if (end != value.c_str()) {
                cpuPart = parsed;
                haveCpuPart = true;
            }
        }
    }

    if (!modelName.empty()) return modelName;
    if (!hardware.empty()) return hardware;

    const long cores = std::max(1L, sysconf(_SC_NPROCESSORS_ONLN));

    if (haveCpuPart) {
        const std::string partName = ArmCpuPartName(cpuPart);
        if (!partName.empty())
            return "ARM " + partName + " \xC3\x97 " + std::to_string(cores);
    }

    struct utsname uts = {};
    if (uname(&uts) == 0)
        return std::string(uts.machine) + " \xC3\x97 " + std::to_string(cores);

    return "Unknown CPU";
}

bool ReadCpuTimes(uint64_t& total, uint64_t& idle) {
    std::ifstream file("/proc/stat");
    std::string line;
    if (!std::getline(file, line)) return false;

    std::istringstream iss(line);
    std::string label;
    iss >> label; // "cpu"

    uint64_t values[8] = {};
    for (int i = 0; i < 8; ++i) {
        if (!(iss >> values[i])) return false;
    }

    total = 0;
    for (uint64_t value : values) total += value;
    idle = values[3] + values[4]; // idle + iowait
    return total > 0;
}

bool ReadMemoryInfo(uint64_t& total, uint64_t& used) {
    std::ifstream file("/proc/meminfo");
    if (!file) return false;

    uint64_t memTotalKb = 0;
    uint64_t memAvailableKb = 0;
    uint64_t memFreeKb = 0;
    uint64_t buffersKb = 0;
    uint64_t cachedKb = 0;

    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string key;
        uint64_t value = 0;
        std::string unit;
        if (!(iss >> key >> value)) continue;

        if (key == "MemTotal:") memTotalKb = value;
        else if (key == "MemAvailable:") memAvailableKb = value;
        else if (key == "MemFree:") memFreeKb = value;
        else if (key == "Buffers:") buffersKb = value;
        else if (key == "Cached:") cachedKb = value;
    }

    if (memTotalKb == 0) return false;

    if (memAvailableKb == 0)
        memAvailableKb = memFreeKb + buffersKb + cachedKb;
    if (memAvailableKb > memTotalKb) memAvailableKb = memTotalKb;

    total = memTotalKb * 1024ull;
    used = (memTotalKb - memAvailableKb) * 1024ull;
    return true;
}

bool IsRealFilesystem(const char* type) {
    static const char* const kTypes[] = {
        "ext2", "ext3", "ext4", "xfs", "btrfs", "f2fs", "vfat", "exfat",
        "ntfs", "ntfs3", "fuseblk", "zfs", "reiserfs", "jfs",
    };
    for (const char* candidate : kTypes) {
        if (std::strcmp(type, candidate) == 0) return true;
    }
    return false;
}

void AddDisk(const std::string& name, const std::string& mountPoint,
             std::vector<DiskInfo>& disks) {
    struct statvfs stats = {};
    if (statvfs(mountPoint.c_str(), &stats) != 0) return;
    if (stats.f_blocks == 0) return;

    const uint64_t blockSize = stats.f_frsize ? stats.f_frsize : stats.f_bsize;
    const uint64_t total = static_cast<uint64_t>(stats.f_blocks) * blockSize;
    const uint64_t free = static_cast<uint64_t>(stats.f_bavail) * blockSize;
    if (total == 0) return;

    DiskInfo info;
    info.name = name;
    info.totalBytes = total;
    info.freeBytes = free;
    info.usagePercentage =
        static_cast<double>(total - std::min(free, total)) / static_cast<double>(total) * 100.0;
    disks.push_back(std::move(info));
}

void ReadDisks(std::vector<DiskInfo>& disks) {
    disks.clear();

    FILE* mounts = setmntent("/proc/self/mounts", "r");
    if (mounts) {
        std::set<std::string> seenDevices;
        struct mntent* entry = nullptr;
        while ((entry = getmntent(mounts)) != nullptr) {
            if (!entry->mnt_fsname || !entry->mnt_dir || !entry->mnt_type) continue;
            if (std::strncmp(entry->mnt_fsname, "/dev/", 5) != 0) continue;
            if (!IsRealFilesystem(entry->mnt_type)) continue;
            if (!seenDevices.insert(entry->mnt_fsname).second) continue;

            AddDisk(entry->mnt_dir, entry->mnt_dir, disks);
        }
        endmntent(mounts);
    }

    // Containers and unusual setups may not expose block devices. Fall back to
    // the root filesystem so the panel still has useful information.
    if (disks.empty()) AddDisk("/", "/", disks);
}

std::string GetGlRenderer() {
    const GLubyte* renderer = glGetString(GL_RENDERER);
    if (!renderer) return {};
    std::string name = reinterpret_cast<const char*>(renderer);
    return Trim(name);
}

uint64_t ReadDrmVramBytes() {
    glob_t globResult = {};
    uint64_t total = 0;
    if (glob("/sys/class/drm/card*/device/mem_info_vram_total", GLOB_NOSORT, nullptr,
             &globResult) == 0) {
        for (size_t i = 0; i < globResult.gl_pathc; ++i) {
            uint64_t value = 0;
            if (ReadUint64File(globResult.gl_pathv[i], value)) {
                total = std::max(total, value);
            }
        }
    }
    globfree(&globResult);
    return total;
}

bool IsSoftwareRenderer(const std::string& renderer) {
    std::string lowered = renderer;
    std::transform(lowered.begin(), lowered.end(), lowered.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return lowered.find("llvmpipe") != std::string::npos ||
           lowered.find("softpipe") != std::string::npos ||
           lowered.find("swrast") != std::string::npos;
}

bool ReadAmdGpuUsage(double& usage) {
    glob_t globResult = {};
    bool found = false;
    if (glob("/sys/class/drm/card*/device/gpu_busy_percent", GLOB_NOSORT, nullptr,
             &globResult) == 0) {
        for (size_t i = 0; i < globResult.gl_pathc; ++i) {
            uint64_t value = 0;
            if (ReadUint64File(globResult.gl_pathv[i], value)) {
                usage = static_cast<double>(std::min<uint64_t>(value, 100));
                found = true;
                break;
            }
        }
    }
    globfree(&globResult);
    return found;
}

bool ReadNvidiaGpuUsage(double& usage) {
    if (access("/dev/nvidiactl", F_OK) != 0) return false;

    FILE* pipe = popen(
        "nvidia-smi --query-gpu=utilization.gpu --format=csv,noheader,nounits 2>/dev/null", "r");
    if (!pipe) return false;

    char buffer[64] = {};
    const bool readOk = std::fgets(buffer, sizeof(buffer), pipe) != nullptr;
    pclose(pipe);
    if (!readOk) return false;

    char* end = nullptr;
    const double value = std::strtod(buffer, &end);
    if (end == buffer) return false;
    usage = std::clamp(value, 0.0, 100.0);
    return true;
}

} // namespace

SystemMonitor::SystemMonitor() = default;

SystemMonitor::~SystemMonitor() = default;

void SystemMonitor::Init() {
    m_cpuName = ReadCpuName();
    m_gpuName = GetGlRenderer();
    if (m_gpuName.empty()) m_gpuName = "Unknown GPU";
    m_gpuDedicatedMemoryBytes = ReadDrmVramBytes();

    uint64_t total = 0;
    uint64_t idle = 0;
    if (ReadCpuTimes(total, idle)) {
        m_prevCpuTotal = total;
        m_prevCpuIdle = idle;
    }

    Update(); // Initial fetch
}

void SystemMonitor::Update() {
    UpdateCpu();
    UpdateMemory();
    UpdateDisks();
    UpdateGpu();
}

void SystemMonitor::UpdateCpu() {
    uint64_t total = 0;
    uint64_t idle = 0;
    if (!ReadCpuTimes(total, idle)) return;

    if (m_prevCpuTotal != 0 && total > m_prevCpuTotal) {
        const uint64_t totalDelta = total - m_prevCpuTotal;
        const uint64_t idleDelta = idle >= m_prevCpuIdle ? idle - m_prevCpuIdle : 0;
        if (totalDelta > 0) {
            const double busy = static_cast<double>(totalDelta - std::min(idleDelta, totalDelta));
            m_totalCpuUsage = std::clamp(busy / static_cast<double>(totalDelta) * 100.0, 0.0, 100.0);
        }
    }

    m_prevCpuTotal = total;
    m_prevCpuIdle = idle;
}

void SystemMonitor::UpdateMemory() {
    uint64_t total = 0;
    uint64_t used = 0;
    if (!ReadMemoryInfo(total, used)) return;

    m_totalMemoryBytes = total;
    m_usedMemoryBytes = used;
    m_totalMemoryUsage = total > 0 ? static_cast<double>(used) / static_cast<double>(total) * 100.0
                                   : 0.0;
}

void SystemMonitor::UpdateDisks() {
    ReadDisks(m_disks);
}

void SystemMonitor::UpdateGpu() {
    double usage = 0.0;
    if (ReadAmdGpuUsage(usage)) {
        m_totalGpuUsage = usage;
        m_gpuUsageAvailable = true;
        return;
    }

    static const bool softwareRenderer = IsSoftwareRenderer(m_gpuName);
    if (softwareRenderer) {
        m_gpuUsageAvailable = false;
        m_totalGpuUsage = 0.0;
        return;
    }

    if (ReadNvidiaGpuUsage(usage)) {
        m_totalGpuUsage = usage;
        m_gpuUsageAvailable = true;
        return;
    }

    m_gpuUsageAvailable = false;
    m_totalGpuUsage = 0.0;
}
