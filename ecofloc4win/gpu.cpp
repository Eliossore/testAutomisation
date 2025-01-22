#include "gpu.h"

#include <iostream>
#include <vector>
#include <Windows.h>

// Define NVML types and constants
typedef int nvmlReturn_t;
typedef void* nvmlDevice_t;
typedef struct nvmlUtilization_st 
{
    unsigned int gpu;
    unsigned int memory;
} nvmlUtilization_t;
typedef struct nvmlProcessInfo_st 
{
    unsigned int pid;
    unsigned long long usedGpuMemory;
} nvmlProcessInfo_t;

#define NVML_SUCCESS 0

// Define function pointers for NVML functions
typedef nvmlReturn_t(*NvmlInit_t)();
typedef nvmlReturn_t(*NvmlShutdown_t)();
typedef nvmlReturn_t(*NvmlDeviceGetCount_t)(unsigned int*);
typedef nvmlReturn_t(*NvmlDeviceGetHandleByIndex_t)(unsigned int, nvmlDevice_t*);
typedef nvmlReturn_t(*NvmlDeviceGetUtilizationRates_t)(nvmlDevice_t, nvmlUtilization_t*);
typedef nvmlReturn_t(*NvmlDeviceGetComputeRunningProcesses_t)(nvmlDevice_t, unsigned int*, nvmlProcessInfo_t*);
typedef nvmlReturn_t(*NvmlDeviceGetPowerUsage_t)(nvmlDevice_t, unsigned int*);
typedef const char* (*NvmlErrorString_t)(nvmlReturn_t);

class NVMLManager 
{
public:
    static NVMLManager& getInstance() 
    {
        static NVMLManager instance;
        return instance;
    }

    bool initialize() 
    {
        if (!nvmlLib) 
        {
            nvmlLib = LoadLibrary(L"nvml.dll");
            if (!nvmlLib) 
            {
                std::cerr << "NVML library not found. Ensure NVIDIA drivers are installed.\n";
                return false;
            }

            nvmlInit = reinterpret_cast<NvmlInit_t>(GetProcAddress(nvmlLib, "nvmlInit"));
            nvmlShutdown = reinterpret_cast<NvmlShutdown_t>(GetProcAddress(nvmlLib, "nvmlShutdown"));
            nvmlDeviceGetCount = reinterpret_cast<NvmlDeviceGetCount_t>(GetProcAddress(nvmlLib, "nvmlDeviceGetCount"));
            nvmlDeviceGetHandleByIndex = reinterpret_cast<NvmlDeviceGetHandleByIndex_t>(GetProcAddress(nvmlLib, "nvmlDeviceGetHandleByIndex"));
            nvmlDeviceGetUtilizationRates = reinterpret_cast<NvmlDeviceGetUtilizationRates_t>(GetProcAddress(nvmlLib, "nvmlDeviceGetUtilizationRates"));
            nvmlDeviceGetComputeRunningProcesses = reinterpret_cast<NvmlDeviceGetComputeRunningProcesses_t>(GetProcAddress(nvmlLib, "nvmlDeviceGetComputeRunningProcesses"));
            nvmlDeviceGetPowerUsage = reinterpret_cast<NvmlDeviceGetPowerUsage_t>(GetProcAddress(nvmlLib, "nvmlDeviceGetPowerUsage"));
            nvmlErrorString = reinterpret_cast<NvmlErrorString_t>(GetProcAddress(nvmlLib, "nvmlErrorString"));

            if (!nvmlInit || !nvmlShutdown || !nvmlDeviceGetCount || !nvmlDeviceGetHandleByIndex || !nvmlDeviceGetUtilizationRates || !nvmlDeviceGetComputeRunningProcesses || !nvmlDeviceGetPowerUsage || !nvmlErrorString) 
            {
                std::cerr << "Failed to locate NVML functions in the library.\n";
                FreeLibrary(nvmlLib);
                nvmlLib = nullptr;
                return false;
            }
        }
        return true;
    }

    void shutdown() 
    {
        if (nvmlLib) 
        {
            FreeLibrary(nvmlLib);
            nvmlLib = nullptr;
        }
    }

    NvmlInit_t nvmlInit = nullptr;
    NvmlShutdown_t nvmlShutdown = nullptr;
    NvmlDeviceGetCount_t nvmlDeviceGetCount = nullptr;
    NvmlDeviceGetHandleByIndex_t nvmlDeviceGetHandleByIndex = nullptr;
    NvmlDeviceGetUtilizationRates_t nvmlDeviceGetUtilizationRates = nullptr;
    NvmlDeviceGetComputeRunningProcesses_t nvmlDeviceGetComputeRunningProcesses = nullptr;
    NvmlDeviceGetPowerUsage_t nvmlDeviceGetPowerUsage = nullptr;
    NvmlErrorString_t nvmlErrorString = nullptr;

private:
    NVMLManager() = default;
    ~NVMLManager() 
    {
        shutdown();
    }

    HMODULE nvmlLib = nullptr;
};

namespace GPU 
{
    int initNVML() 
    {
        NVMLManager& nvml = NVMLManager::getInstance();
        if (!nvml.initialize()) 
        {
            return 1;
        }

        if (nvml.nvmlInit() == NVML_SUCCESS) 
        {
            std::cout << "NVML initialized successfully.\n";
            if (nvml.nvmlShutdown() == NVML_SUCCESS) 
            {
                std::cout << "NVML shutdown successfully.\n";
            }
            else 
            {
                std::cerr << "Failed to shut down NVML.\n";
            }
        }
        else 
        {
            std::cerr << "Failed to initialize NVML.\n";
        }

        return 0;
    }

    int gpu_usage(int pid) 
    {
        NVMLManager& nvml = NVMLManager::getInstance();
        if (!nvml.initialize()) 
        {
            return -1;
        }

        nvmlReturn_t result;
        unsigned int deviceCount;
        nvmlDevice_t device;
        unsigned int infoCount = 32;
        nvmlProcessInfo_t processInfo[32];
        nvmlUtilization_t utilization;
        int usage = -1; // Default to -1 if PID is not found

        // Initialize NVML
        result = nvml.nvmlInit();
        if (result != NVML_SUCCESS) 
        {
            std::cerr << "Failed to initialize NVML: " << nvml.nvmlErrorString(result) << std::endl;
            return -1;
        }

        // Get the number of devices
        result = nvml.nvmlDeviceGetCount(&deviceCount);
        if (result != NVML_SUCCESS) 
        {
            std::cerr << "Failed to get device count: " << nvml.nvmlErrorString(result) << std::endl;
            nvml.nvmlShutdown();
            return -1;
        }

        // Iterate over each device
        for (unsigned int i = 0; i < deviceCount; ++i) 
        {
            result = nvml.nvmlDeviceGetHandleByIndex(i, &device);
            if (result != NVML_SUCCESS) 
            {
                std::cerr << "Failed to get handle for device " << i << ": " << nvml.nvmlErrorString(result) << std::endl;
                continue;
            }

            // Get GPU utilization rates
            result = nvml.nvmlDeviceGetUtilizationRates(device, &utilization);
            if (result != NVML_SUCCESS) 
            {
                std::cerr << "Failed to get utilization rates for device " << i << ": " << nvml.nvmlErrorString(result) << std::endl;
                continue;
            }

            // Get running processes on the device
            result = nvml.nvmlDeviceGetComputeRunningProcesses(device, &infoCount, processInfo);
            if (result != NVML_SUCCESS) 
            {
                std::cerr << "Failed to get running processes for device " << i << ": " << nvml.nvmlErrorString(result) << std::endl;
                continue;
            }

            // Search for the process by PID
            for (unsigned int j = 0; j < infoCount; ++j) 
            {
                if (processInfo[j].pid == pid) 
                {
                    // Approximation: Report total GPU utilization
                    usage = utilization.gpu; // Aggregate GPU utilization
                    goto cleanup; // Exit after finding the PID
                }
            }
        }

    cleanup:
        // Cleanup NVML
        nvml.nvmlShutdown();
        return usage;
    }

    // Function to retrieve GPU usage for a list of PIDs
    std::vector<int> getGPUUsage(std::vector<int> pids) 
    {
        std::vector<int> results;
        for (int pid : pids) 
        {
            results.push_back(gpu_usage(pid));
        }
        return results;
    }

    // Function to retrieve GPU power usage
    int getGPUPower() 
    {
        NVMLManager& nvml = NVMLManager::getInstance();
        if (!nvml.initialize()) 
        {
            return -1;
        }

        nvmlReturn_t result;
        nvmlDevice_t device;

        // Initialize NVML
        result = nvml.nvmlInit();
        if (result != NVML_SUCCESS) 
        {
            std::cerr << "Failed to initialize NVML: " << nvml.nvmlErrorString(result) << std::endl;
            return -1;
        }

        // Get handle for GPU 0 (or another GPU by index)
        result = nvml.nvmlDeviceGetHandleByIndex(0, &device);
        if (result != NVML_SUCCESS) 
        {
            std::cerr << "Failed to get handle for GPU: " << nvml.nvmlErrorString(result) << std::endl;
            nvml.nvmlShutdown();
            return -1;
        }

        // Read power usage (in milliwatts)
        unsigned int power;
        result = nvml.nvmlDeviceGetPowerUsage(device, &power);
        if (result != NVML_SUCCESS) 
        {
            std::cerr << "Failed to read power usage: " << nvml.nvmlErrorString(result) << std::endl;
            nvml.nvmlShutdown();
            return -1;
        }

        // Cleanup NVML
        nvml.nvmlShutdown();

        // Power is given in milliwatts, so convert to watts
        return power / 1000.0;
    }

    int getGPUJoules(std::vector<int> pids, int ms) 
    {
        double interval_s = (double)ms / 1000.0;
        int results = 0;
        for (int pid : pids) 
        {
            int usage = gpu_usage(pid);
            int power = getGPUPower();
            double power_used = ((usage / 100.0) * power);
            results += power_used * interval_s;
        }
        return results;
    }
}
