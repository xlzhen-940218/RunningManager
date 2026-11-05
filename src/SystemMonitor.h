#pragma once

#include <cstdint>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <pdh.h>
#include <d3d11.h>
#endif

struct DiskInfo {
    std::string name;
    uint64_t    totalBytes       = 0;
    uint64_t    freeBytes        = 0;
    double      usagePercentage  = 0.0;
};

class SystemMonitor {
public:
    SystemMonitor();
    ~SystemMonitor();

#ifdef _WIN32
    void Init(ID3D11Device* device);
#else
    void Init();
#endif
    void Update();

    double GetTotalCpuUsage() const { return m_totalCpuUsage; }
    double GetTotalMemoryUsage() const { return m_totalMemoryUsage; }
    const std::vector<DiskInfo>& GetDisks() const { return m_disks; }
    double GetTotalGpuUsage() const { return m_totalGpuUsage; }
    bool   IsGpuUsageAvailable() const { return m_gpuUsageAvailable; }

    uint64_t GetTotalMemoryBytes() const { return m_totalMemoryBytes; }
    uint64_t GetUsedMemoryBytes() const { return m_usedMemoryBytes; }

    const std::string& GetCpuName() const { return m_cpuName; }
    const std::string& GetGpuName() const { return m_gpuName; }
    uint64_t GetGpuDedicatedMemory() const { return m_gpuDedicatedMemoryBytes; }

private:
    // CPU
    double      m_totalCpuUsage = 0.0;
    std::string m_cpuName;

    // Memory
    double   m_totalMemoryUsage = 0.0;
    uint64_t m_totalMemoryBytes = 0;
    uint64_t m_usedMemoryBytes  = 0;

    // Disk
    std::vector<DiskInfo> m_disks;

    // GPU
    double      m_totalGpuUsage           = 0.0;
    bool        m_gpuUsageAvailable       = false;
    std::string m_gpuName;
    uint64_t    m_gpuDedicatedMemoryBytes = 0;

    void UpdateCpu();
    void UpdateMemory();
    void UpdateDisks();
    void UpdateGpu();

#ifdef _WIN32
    ULARGE_INTEGER m_prevSysIdle   = {};
    ULARGE_INTEGER m_prevSysKernel = {};
    ULARGE_INTEGER m_prevSysUser   = {};
    PDH_HQUERY     m_pdhQuery      = nullptr;
    PDH_HCOUNTER   m_gpuCounter    = nullptr;
    bool           m_pdhInitialized = false;
#else
    uint64_t m_prevCpuTotal = 0;
    uint64_t m_prevCpuIdle  = 0;
#endif
};
