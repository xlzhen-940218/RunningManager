#pragma once

#include <windows.h>
#include <pdh.h>
#include <d3d11.h>
#include <vector>
#include <string>

struct DiskInfo {
    std::string name;
    SIZE_T totalBytes;
    SIZE_T freeBytes;
    double usagePercentage;
};

class SystemMonitor {
public:
    SystemMonitor();
    ~SystemMonitor();

    void Init(ID3D11Device* device);
    void Update();

    double GetTotalCpuUsage() const { return m_totalCpuUsage; }
    double GetTotalMemoryUsage() const { return m_totalMemoryUsage; }
    const std::vector<DiskInfo>& GetDisks() const { return m_disks; }
    double GetTotalGpuUsage() const { return m_totalGpuUsage; }
    
    SIZE_T GetTotalMemoryBytes() const { return m_totalMemoryBytes; }
    SIZE_T GetUsedMemoryBytes() const { return m_usedMemoryBytes; }

    const std::string& GetCpuName() const { return m_cpuName; }
    const std::string& GetGpuName() const { return m_gpuName; }
    SIZE_T GetGpuDedicatedMemory() const { return m_gpuDedicatedMemoryBytes; }

private:
    // CPU
    ULARGE_INTEGER m_prevSysIdle;
    ULARGE_INTEGER m_prevSysKernel;
    ULARGE_INTEGER m_prevSysUser;
    double m_totalCpuUsage;
    std::string m_cpuName;

    // Memory
    double m_totalMemoryUsage;
    SIZE_T m_totalMemoryBytes;
    SIZE_T m_usedMemoryBytes;

    // Disk & GPU
    PDH_HQUERY m_pdhQuery;
    PDH_HCOUNTER m_gpuCounter;
    bool m_pdhInitialized;
    double m_totalGpuUsage;
    std::vector<DiskInfo> m_disks;
    
    std::string m_gpuName;
    SIZE_T m_gpuDedicatedMemoryBytes;
    
    void UpdateCpu();
    void UpdateMemory();
    void UpdateDisks();
    void UpdatePdh();
};
