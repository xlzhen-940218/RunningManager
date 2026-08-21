#include "SystemMonitor.h"
#include <iostream>
#include <pdhmsg.h>
#include <dxgi.h>

#pragma comment(lib, "pdh.lib")
#pragma comment(lib, "dxgi.lib")

static ULONGLONG FileTimeToULL(const FILETIME& ft) {
    ULARGE_INTEGER uli;
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;
    return uli.QuadPart;
}

SystemMonitor::SystemMonitor() 
    : m_totalCpuUsage(0.0), m_totalMemoryUsage(0.0), m_totalMemoryBytes(0), m_usedMemoryBytes(0),
      m_pdhQuery(NULL), m_gpuCounter(NULL), m_pdhInitialized(false),
      m_totalGpuUsage(0.0), m_gpuDedicatedMemoryBytes(0)
{
    m_prevSysIdle.QuadPart = 0;
    m_prevSysKernel.QuadPart = 0;
    m_prevSysUser.QuadPart = 0;
}

SystemMonitor::~SystemMonitor() {
    if (m_pdhQuery) {
        PdhCloseQuery(m_pdhQuery);
    }
}

void SystemMonitor::Init(ID3D11Device* device) {
    // 1. Fetch CPU Name from Registry
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        char buffer[256];
        DWORD bufferSize = sizeof(buffer);
        if (RegQueryValueExA(hKey, "ProcessorNameString", NULL, NULL, (LPBYTE)buffer, &bufferSize) == ERROR_SUCCESS) {
            m_cpuName = buffer;
            // Optionally clean up trailing spaces
            m_cpuName.erase(m_cpuName.find_last_not_of(" \n\r\t") + 1);
        }
        RegCloseKey(hKey);
    }

    // 2. Fetch GPU Name and VRAM from DXGI
    if (device) {
        IDXGIDevice* dxgiDevice = nullptr;
        if (SUCCEEDED(device->QueryInterface(__uuidof(IDXGIDevice), (void**)&dxgiDevice))) {
            IDXGIAdapter* adapter = nullptr;
            if (SUCCEEDED(dxgiDevice->GetAdapter(&adapter))) {
                DXGI_ADAPTER_DESC desc;
                if (SUCCEEDED(adapter->GetDesc(&desc))) {
                    char tempGpuName[256];
                    WideCharToMultiByte(CP_UTF8, 0, desc.Description, -1, tempGpuName, sizeof(tempGpuName), NULL, NULL);
                    m_gpuName = tempGpuName;
                    m_gpuDedicatedMemoryBytes = desc.DedicatedVideoMemory;
                }
                adapter->Release();
            }
            dxgiDevice->Release();
        }
    }
    if (m_gpuName.empty()) {
        m_gpuName = "Unknown GPU";
    }

    // 3. Initial CPU times
    FILETIME idle, kernel, user;
    if (GetSystemTimes(&idle, &kernel, &user)) {
        m_prevSysIdle.QuadPart = FileTimeToULL(idle);
        m_prevSysKernel.QuadPart = FileTimeToULL(kernel);
        m_prevSysUser.QuadPart = FileTimeToULL(user);
    }

    // Initialize PDH
    if (PdhOpenQuery(NULL, 0, &m_pdhQuery) == ERROR_SUCCESS) {
        bool addedAny = false;
        if (PdhAddEnglishCounterW(m_pdhQuery, L"\\GPU Engine(*)\\Utilization Percentage", 0, &m_gpuCounter) == ERROR_SUCCESS) {
            addedAny = true;
        }
        
        if (addedAny) {
            PdhCollectQueryData(m_pdhQuery);
            m_pdhInitialized = true;
        }
    }
    
    Update(); // Initial fetch
}

void SystemMonitor::Update() {
    UpdateCpu();
    UpdateMemory();
    UpdateDisks();
    UpdatePdh();
}

void SystemMonitor::UpdateCpu() {
    FILETIME idle, kernel, user;
    if (GetSystemTimes(&idle, &kernel, &user)) {
        ULONGLONG curIdle = FileTimeToULL(idle);
        ULONGLONG curKernel = FileTimeToULL(kernel);
        ULONGLONG curUser = FileTimeToULL(user);

        ULONGLONG sysIdleDiff = curIdle - m_prevSysIdle.QuadPart;
        ULONGLONG sysKernelDiff = curKernel - m_prevSysKernel.QuadPart;
        ULONGLONG sysUserDiff = curUser - m_prevSysUser.QuadPart;

        ULONGLONG sysTotalDiff = sysKernelDiff + sysUserDiff;

        if (sysTotalDiff > 0) {
            m_totalCpuUsage = (sysTotalDiff - sysIdleDiff) * 100.0 / sysTotalDiff;
            if (m_totalCpuUsage < 0.0) m_totalCpuUsage = 0.0;
            if (m_totalCpuUsage > 100.0) m_totalCpuUsage = 100.0;
        }

        m_prevSysIdle.QuadPart = curIdle;
        m_prevSysKernel.QuadPart = curKernel;
        m_prevSysUser.QuadPart = curUser;
    }
}

void SystemMonitor::UpdateMemory() {
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    if (GlobalMemoryStatusEx(&memInfo)) {
        m_totalMemoryBytes = memInfo.ullTotalPhys;
        m_usedMemoryBytes = memInfo.ullTotalPhys - memInfo.ullAvailPhys;
        m_totalMemoryUsage = (double)m_usedMemoryBytes / (double)m_totalMemoryBytes * 100.0;
    }
}

void SystemMonitor::UpdateDisks() {
    m_disks.clear();
    char driveBuffer[256];
    DWORD len = GetLogicalDriveStringsA(sizeof(driveBuffer), driveBuffer);
    if (len > 0 && len <= sizeof(driveBuffer)) {
        char* drive = driveBuffer;
        while (*drive) {
            if (GetDriveTypeA(drive) == DRIVE_FIXED) {
                ULARGE_INTEGER freeBytesAvailable, totalNumberOfBytes, totalNumberOfFreeBytes;
                if (GetDiskFreeSpaceExA(drive, &freeBytesAvailable, &totalNumberOfBytes, &totalNumberOfFreeBytes)) {
                    DiskInfo info;
                    info.name = drive;
                    info.totalBytes = totalNumberOfBytes.QuadPart;
                    info.freeBytes = totalNumberOfFreeBytes.QuadPart;
                    SIZE_T used = info.totalBytes - info.freeBytes;
                    info.usagePercentage = (double)used / (double)info.totalBytes * 100.0;
                    m_disks.push_back(info);
                }
            }
            drive += strlen(drive) + 1;
        }
    }
}

void SystemMonitor::UpdatePdh() {
    if (!m_pdhInitialized) return;

    if (PdhCollectQueryData(m_pdhQuery) != ERROR_SUCCESS) return;

    // GPU
    if (m_gpuCounter) {
        DWORD bufferSize = 0;
        DWORD itemCount = 0;
        PDH_STATUS status = PdhGetFormattedCounterArrayW(m_gpuCounter, PDH_FMT_DOUBLE, &bufferSize, &itemCount, NULL);
        if (status == PDH_MORE_DATA && bufferSize > 0) {
            std::vector<BYTE> buffer(bufferSize);
            PDH_FMT_COUNTERVALUE_ITEM_W* items = (PDH_FMT_COUNTERVALUE_ITEM_W*)buffer.data();
            if (PdhGetFormattedCounterArrayW(m_gpuCounter, PDH_FMT_DOUBLE, &bufferSize, &itemCount, items) == ERROR_SUCCESS) {
                double totalGpu = 0.0;
                for (DWORD i = 0; i < itemCount; ++i) {
                    totalGpu += items[i].FmtValue.doubleValue;
                }
                m_totalGpuUsage = totalGpu;
                if (m_totalGpuUsage > 100.0) m_totalGpuUsage = 100.0;
            }
        }
    }
}
