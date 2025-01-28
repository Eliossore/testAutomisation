#include "CPU.h"

/// Typedef for a function pointer to retrieve CPU voltages.
typedef float* (*get_cpu_voltages_func)(int* size);

/// Typedef for a function pointer to retrieve CPU clocks.
typedef float* (*get_cpu_clocks_func)(int* size);

/// Typedef for a function pointer to retrieve CPU cores' power.
typedef float* (*get_cpu_cores_power_func)(int* size);

/// Namespace for CPU-related functionalities.
namespace CPU
{
    /**
     * @brief Converts a FILETIME structure to a uint64_t.
     *
     * @param ft The FILETIME structure to convert.
     * @return uint64_t The converted value.
     */
    uint64_t fromFileTime(const FILETIME& ft)
    {
        ULARGE_INTEGER uli = { 0 };
        uli.LowPart = ft.dwLowDateTime;
        uli.HighPart = ft.dwHighDateTime;
        return uli.QuadPart;
    }

    /**
     * @brief Retrieves the total CPU time including idle, kernel, and user times.
     *
     * @return uint64_t The total CPU time in 100-nanosecond intervals. Returns -1 on failure.
     */
    uint64_t getCPUTime()
    {
        FILETIME idle_time, kernel_time, user_time;

        if (GetSystemTimes(&idle_time, &kernel_time, &user_time))
        {
            uint64_t cpu_time = fromFileTime(kernel_time) + fromFileTime(user_time) + fromFileTime(idle_time);
            return cpu_time;
        }
        else
        {
            std::cerr << "Failed to get CPU Time: Error " << GetLastError() << std::endl;
            return -1;
        }
    }

    /**
     * @brief Retrieves the total time spent by a process.
     *
     * @param pid The process ID of the target process.
     * @return uint64_t The total process time (kernel + user) in 100-nanosecond intervals. Returns -1 on failure.
     */
    uint64_t getPidTime(DWORD pid)
    {
        HANDLE h_process = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
        if (h_process == NULL)
        {
            std::cerr << "Failed to open process handle. Error: " << GetLastError() << std::endl;
            return -1;
        }

        FILETIME creation_time, exit_time, kernel_time, user_time;

        if (GetProcessTimes(h_process, &creation_time, &exit_time, &kernel_time, &user_time))
        {
            uint64_t kernel = fromFileTime(kernel_time);
            uint64_t user = fromFileTime(user_time);

            uint64_t total_time = kernel + user;
            CloseHandle(h_process);
            return total_time;
        }
        else
        {
            std::cerr << "Failed to get process times. Error: " << GetLastError() << std::endl;
            CloseHandle(h_process);
            return -1;
        }
    }

    /**
     * @brief Retrieves the current power consumption of the CPU.
     *
     * This function uses a DLL (`Wrapper.dll`) to retrieve the power consumption of CPU cores.
     *
     * @param power A reference to a double where the calculated power will be stored.
     * @return bool True if the power was successfully retrieved, false otherwise.
     */
    bool getCurrentPower(double& power)
    {
        HMODULE h_module = LoadLibrary(L"Wrapper.dll");
        if (!h_module)
        {
            std::cerr << "Failed to load Wrapper.dll. Error code: " << GetLastError() << std::endl;
            return false;
        }

        get_cpu_cores_power_func get_cpu_cores_power = (get_cpu_cores_power_func)GetProcAddress(h_module, "getCPUCoresPower");
        if (!get_cpu_cores_power)
        {
            std::cerr << "Failed to get function address for getCPUCoresPower. Error code: " << GetLastError() << std::endl;
            FreeLibrary(h_module);
            return false;
        }

        int power_size = 0;
        int cpu_count = 0;
        float* power_array = get_cpu_cores_power(&power_size);

        if (power_array)
        {
            for (int i = 0; i < power_size; i++)
            {
                power += power_array[i];
                cpu_count++;
            }
            delete[] power_array;
        }
        else
        {
            std::cerr << "Failed to retrieve CPU power." << std::endl;
        }

        if (cpu_count > 0)
        {
            power = power / cpu_count;
        }
        else
        {
            power = 0.0;
        }

        FreeLibrary(h_module);

        return true;
    }
}
