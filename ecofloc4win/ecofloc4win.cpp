#define NOMINMAX
//#define WIN32_LEAN_AND_MEAN  // Prevent inclusion of unnecessary Windows headers

#define WIN32_WINNT 0x0600  // Vista and later

#include <winsock2.h>        // Include Winsock2 before windows.h to avoid conflicts
#include <WS2tcpip.h>
#include <windows.h>         // Windows core headers
#include <iphlpapi.h>        // Network management functions
#include <tcpestats.h>       // TCP extended stats
#include <psapi.h>           // For ProcessStatus API
#include <tchar.h>           // Generic text mappings for Unicode/ANSI
#include <locale>            // For localization and locale functions
#include <codecvt>           // For string conversions
#include <tlhelp32.h>        // For process and snapshot handling
#include <cctype>            // For character handling functions
#include <algorithm>         // For STL algorithms
#include <unordered_map>     // For unordered map functionality
#include <utility>           // For utility functions and data types
#include <vector>            // For vector container
#include <string>            // For string handling
#include <sstream>           // For string streams
#include <list>              // For list container
#include <mutex>             // For mutex support in multithreading
#include <Pdh.h>
#include <PdhMsg.h>
#include <cstring>
#include <iostream>
#include <tcpmib.h>

#include "process.h"         // Custom header for process handling
#include "gpu.h"             // Custom header for GPU monitoring
#include "CPU.h"
#include "MonitoringData.h"  // Custom header for monitoring data

#include "ftxui/component/screen_interactive.hpp"
#include "ftxui/component/component.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/dom/table.hpp"
#include "ftxui/dom/node.hpp"
#include "ftxui/screen/color.hpp"

using namespace ftxui;

std::vector<MonitoringData> monitoringData = {};
std::condition_variable new_data_cv;
std::mutex data_mutex;

// Function to get terminal size
int GetTerminalHeight() 
{
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi)) 
    {
		// Calculate the height of the terminal window.
		return csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
	}
	// Default to 24 rows if size cannot be determined.
	return 24;
}

string wstring_to_string(const wstring& wide_string)
{
    string str;
    size_t size;
    str.resize(wide_string.length());
    wcstombs_s(&size, &str[0], str.size() + 1, wide_string.c_str(), wide_string.size());
    return str;
}

void readCommand(string);

void addProcPid(string, string);
void addProcName(string, string);

void removeProcPid(string, string);
void removeProcName(string, string);

void enable(string);
void disable(string);

std::wstring getLocalizedCounterPath(const std::wstring& processName, const std::string& counterName);

unordered_map<string, int> actions = 
{
    {"enable", 1},
    {"disable", 2},
    {"add", 3},
    {"remove", 4},
    {"interval", 5},
    {"quit", 6},
};

wstring GetProcessNameByPID(DWORD processID) 
{
    TCHAR processName[MAX_PATH] = TEXT("<unknown>");

    // Get a handle to the process
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processID);

    // Check if we successfully got a handle to the process
    if (hProcess) {
        // Get the process name
        if (GetModuleBaseName(hProcess, nullptr, processName, sizeof(processName) / sizeof(TCHAR))) {
            CloseHandle(hProcess); // Close handle when done
            return processName;
        }
        CloseHandle(hProcess); // Close handle even if we fail
    }

    return L"<unknown>";
}

unordered_map<string, pair<vector<process>, bool>> comp = {{"CPU", {{}, false}}, {"GPU", {{}, false} }, {"SD", {{}, false }}, {"NIC", {{}, false }}};

int interval = 500;

/*
* This function gets the index of a counter in the registry based on its name.
*/
DWORD getCounterIndex(const std::string& counterName) 
{
    HKEY hKey;
    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Perflib\\009", 0, KEY_READ, &hKey) != ERROR_SUCCESS) 
    {
        std::cerr << "Failed to open registry key" << std::endl;
        return -1;
    }

    DWORD dataSize = 0;
    if (RegQueryValueEx(hKey, L"Counter", NULL, NULL, NULL, &dataSize) != ERROR_SUCCESS) 
    {
        std::cerr << "Failed to query registry value size" << std::endl;
        RegCloseKey(hKey);
        return -1;
    }

    // Allocate buffer for the Counter data
    std::unique_ptr<char[]> buffer(new char[dataSize]);
    if (RegQueryValueEx(hKey, L"Counter", NULL, NULL, reinterpret_cast<LPBYTE>(buffer.get()), &dataSize) != ERROR_SUCCESS) 
    {
        std::cerr << "Failed to query registry value" << std::endl;
        RegCloseKey(hKey);
        return -1;
    }

    RegCloseKey(hKey);

    // Parse the buffer
    std::string temp;
    std::string currentIndex;
    bool isIndex = true; // Tracks if the current string is an index or a name

    for (DWORD i = 0; i < dataSize; i += 2)  // Increment by 2 to skip null terminators
    {
        if (buffer[i] == '\0') {
            // Process accumulated string
            if (!temp.empty()) 
            {
                if (isIndex) 
                {
                    currentIndex = temp;  // Save the index
                }
                else 
                {
                    // Debug: Output the parsed index and name

                    // Check if the name matches the target counter name
                    if (temp == counterName) 
                    {
                        RegCloseKey(hKey);
                        return std::stoul(currentIndex);  // Convert index to integer
                    }
                }

                temp.clear();          // Reset for the next string
                isIndex = !isIndex;    // Toggle between index and name
            }
        }
        else 
        {
            temp += buffer[i];  // Append meaningful character (skip '\0')
        }
    }

    // Cleanup and return
    RegCloseKey(hKey);
    return -1;  // Counter name not found
}

/*
* This function gets the instance name for a given process ID.
*/
std::wstring GetInstanceForPID(int targetPID) 
{
    PDH_HQUERY query = nullptr;
    PDH_HCOUNTER pidCounter = nullptr;

    DWORD counterIndex = getCounterIndex("ID Process");
    DWORD processIndex = getCounterIndex("Process");

	std::wstring queryPath = getLocalizedCounterPath(L"*", "ID Process");


    // Open a query
    if (PdhOpenQuery(nullptr, 0, &query) != ERROR_SUCCESS) 
    {
        std::cerr << "Failed to open PDH query." << std::endl;
        return L"";
    }

    // Add the wildcard counter for all processes
    if (PdhAddCounter(query, queryPath.c_str(), 0, &pidCounter) != ERROR_SUCCESS) 
    {
        std::cerr << "Failed to add counter for process ID." << std::endl;
        PdhCloseQuery(query);
        return L"";
    }

    // Collect data
    if (PdhCollectQueryData(query) != ERROR_SUCCESS) 
    {
        std::cerr << "Failed to collect query data." << std::endl;
        PdhCloseQuery(query);
        return L"";
    }

    // Get counter info to enumerate instances
    DWORD bufferSize = 0;
    DWORD itemCount = 0;
    PdhGetRawCounterArray(pidCounter, &bufferSize, &itemCount, nullptr);

    std::vector<BYTE> buffer(bufferSize);
    PDH_RAW_COUNTER_ITEM* items = reinterpret_cast<PDH_RAW_COUNTER_ITEM*>(buffer.data());
    if (PdhGetRawCounterArray(pidCounter, &bufferSize, &itemCount, items) != ERROR_SUCCESS) 
    {
        std::cerr << "Failed to get counter array." << std::endl;
        PdhCloseQuery(query);
        return L"";
    }

    // Match the target PID with the instance name
    std::wstring matchedInstance;
    for (DWORD i = 0; i < itemCount; ++i) 
    {
        if (static_cast<int>(items[i].RawValue.FirstValue) == targetPID) 
        {
            matchedInstance = items[i].szName;
            break;
        }
    }

    PdhCloseQuery(query);
    return matchedInstance;
}

/*
* This function get the localized counter path for a given process name and counter name to be used in PDH functions
* and avoid hardcoding the counter path for each language.
*/
std::wstring getLocalizedCounterPath(const std::wstring& processName, const std::string& counterName)
{
	wchar_t localizedName[PDH_MAX_COUNTER_PATH];
	wchar_t localizedProcessName[PDH_MAX_COUNTER_PATH];
	DWORD size = PDH_MAX_COUNTER_PATH;
	DWORD counterIndex = getCounterIndex(counterName);
	DWORD processIndex = getCounterIndex("Process");

	if (PdhLookupPerfNameByIndex(NULL, counterIndex, localizedName, &size) != ERROR_SUCCESS) 
    {
		std::cerr << "Failed to get localized counter path" << std::endl;
		return L"";
	}

    if (PdhLookupPerfNameByIndex(NULL, processIndex, localizedProcessName, &size) != ERROR_SUCCESS) 
    {
        std::cerr << "Failed to get localized counter path" << std::endl;
        return L"";
    }

	std::wstring localizedProcessNameW(localizedProcessName);
	std::wstring localizedNameW(localizedName);
    return L"\\"+ localizedProcessNameW + L"(" + processName + L")\\" + localizedNameW;
}

auto CreateTableRows() -> std::vector<std::vector<std::string>> 
{
	std::vector<std::vector<std::string>> rows;
	std::lock_guard<std::mutex> lock(data_mutex);

	rows.emplace_back(std::vector<std::string>{"Application Name", "CPU", "GPU", "SD", "NIC"});
	for (const auto& data : monitoringData) 
    {
		rows.emplace_back(std::vector<std::string>
        {
			data.getName(),
				std::to_string(data.cpuEnergy),
				std::to_string(data.gpuEnergy),
				std::to_string(data.sdEnergy),
				std::to_string(data.nicEnergy)
		});
	}

	return rows;
}

auto RenderTable(int scroll_position) -> Element 
{
	auto table_data = CreateTableRows();
	int terminal_height = GetTerminalHeight();
	int visible_rows = terminal_height - 8; // Adjust for input box and borders

	// Prepare rows for the visible portion
	std::vector<std::vector<std::string>> visible_table_data;
	visible_table_data.push_back(table_data[0]); // Header row
	for (int i = scroll_position + 1; i < std::min(scroll_position + visible_rows + 1, (int)table_data.size()); ++i) 
    {
		visible_table_data.push_back(table_data[i]);
	}

	auto table = Table(visible_table_data);

	// Style the table
	table.SelectAll().Border(LIGHT);
	table.SelectRow(0).Decorate(bold);
	table.SelectRow(0).DecorateCells(center);
	table.SelectRow(0).SeparatorVertical(LIGHT);
	table.SelectRow(0).Border();
	table.SelectColumn(0).Decorate(flex);
	table.SelectColumns(0, -1).SeparatorVertical(LIGHT);
	auto content = table.SelectRows(1, -1);
	content.DecorateCellsAlternateRow(color(Color::Red), 3, 0);
	content.DecorateCellsAlternateRow(color(Color::RedLight), 3, 1);
	content.DecorateCellsAlternateRow(color(Color::White), 3, 2);

	return table.Render() | flex;
}

int main()
{
    std::string input;
	Component input_box = Input(&input, "Type here");
    input_box |= CatchEvent([&](Event event) 
        {
        if (event == Event::Return) 
        {
            if (!input.empty()) 
            {
                std::cout << "Command: " << input << std::endl;
                readCommand(input);
                input.clear();
            }
            return true;
        }
        return false;
        });

	auto screen = ScreenInteractive::Fullscreen();

	// State variables for scrolling
	int scroll_position = 0;

	// Component to handle input and update the scroll position
	auto component = Renderer(input_box, [&] 
        {
		    return vbox(
                {
			        RenderTable(scroll_position),
			        separator(),
			        hbox(
                    {
				        text("Command: "), input_box->Render()
			        }),
			    }) | border;
		});

	component = CatchEvent(component, [&](Event event) {
		int terminal_height = GetTerminalHeight();
		int visible_rows = terminal_height - 8;

		if ((int)monitoringData.size() <= visible_rows) 
        {
			scroll_position = 0; // Disable scrolling if all rows fit
			return false;
		}

		// Handle mouse wheel and arrow key events
		if (event.is_mouse()) 
        {
			if (event.mouse().button == Mouse::WheelDown) 
            {
				scroll_position = std::min(scroll_position + 1, (int)monitoringData.size() - visible_rows - 1);
				return true;
			}
			if (event.mouse().button == Mouse::WheelUp) 
            {
				scroll_position = std::max(scroll_position - 1, 0);
				return true;
			}
		}

		if (event == Event::ArrowDown) 
        {
			scroll_position = std::min(scroll_position + 1, (int)monitoringData.size() - visible_rows - 1);
			return true;
		}
		if (event == Event::ArrowUp) 
        {
			scroll_position = std::max(scroll_position - 1, 0);
			return true;
		}

		return false;
	});

	std::thread gpu_thread([&screen] 
    {
		while (true) 
        {
            {
			    std::unique_lock<std::mutex> lock(data_mutex);
                new_data_cv.wait(lock, [] { return !monitoringData.empty(); });
			    for (auto& data : monitoringData)
			    {
				    int gpu_joules = GPU::getGPUJoules(data.getPids(), 500);
				    data.updateGPUEnergy(gpu_joules);
			    }
            }

			screen.Post(Event::Custom);
			std::this_thread::sleep_for(std::chrono::milliseconds(500));
		}
	});

    std::thread sd_thread([&screen] 
    {
        PDH_HQUERY query;
        if (PdhOpenQuery(NULL, 0, &query) != ERROR_SUCCESS) 
        {
            std::cerr << "Failed to open PDH query." << std::endl;
            return;
        }

        std::map<std::wstring, std::pair<PDH_HCOUNTER, PDH_HCOUNTER>> process_counters;

        while (true) 
        {
            {
                std::unique_lock<std::mutex> lock(data_mutex);
                new_data_cv.wait(lock, [] { return !monitoringData.empty(); });

                for (auto& data : monitoringData) 
                {
                    std::wstring instanceName = GetInstanceForPID(data.getPids()[0]);

                    if (process_counters.find(instanceName) == process_counters.end()) 
                    {
                        PDH_HCOUNTER counterDiskRead, counterDiskWrite;
                        std::wstring readPath = getLocalizedCounterPath(instanceName, "IO Read Bytes/sec");
                        std::wstring writePath = getLocalizedCounterPath(instanceName, "IO Write Bytes/sec");

                        if (PdhAddCounter(query, readPath.c_str(), 0, &counterDiskRead) != ERROR_SUCCESS ||
                            PdhAddCounter(query, writePath.c_str(), 0, &counterDiskWrite) != ERROR_SUCCESS) 
                        {
                            std::cerr << "Failed to add PDH counters for: " << data.getName() << std::endl;
                            continue;
                        }

                           process_counters[instanceName] = { counterDiskRead, counterDiskWrite };
                    }
                }
            }

            if (PdhCollectQueryData(query) != ERROR_SUCCESS) 
            {
                std::cerr << "Failed to collect PDH query data." << std::endl;
                continue;
            }

            for (auto& [instanceName, counters] : process_counters) 
            {
                PDH_FMT_COUNTERVALUE diskReadValue, diskWriteValue;
                long readRate = 0, writeRate = 0;

                if (PdhGetFormattedCounterValue(counters.first, PDH_FMT_LONG, NULL, &diskReadValue) == ERROR_SUCCESS) 
                {
                    readRate = diskReadValue.longValue;
                }
                if (PdhGetFormattedCounterValue(counters.second, PDH_FMT_LONG, NULL, &diskWriteValue) == ERROR_SUCCESS) 
                {
                    writeRate = diskWriteValue.longValue;
                }

                double readPower = 2.2 * readRate / 5600000000;
                double writePower = 2.2 * writeRate / 5300000000;
                double averagePower = readPower + writePower;

                auto it = std::find_if(monitoringData.begin(), monitoringData.end(), [&](const auto& d) 
                {
                    return GetInstanceForPID(d.getPids()[0]) == instanceName;
                });
                if (it != monitoringData.end()) 
                {
                    it->updateSDEnergy(averagePower * 5);
                }
            }
            
            screen.Post(Event::Custom);
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }

        PdhCloseQuery(query);
        });

    std::thread nic_thread([&screen] 
    {
        while (true) 
        {
            std::vector<MonitoringData> localMonitoringData;

            {
                std::unique_lock<std::mutex> lock(data_mutex);
                new_data_cv.wait(lock, [] { return !monitoringData.empty(); });
                localMonitoringData = monitoringData;
            }

            for (auto& data : localMonitoringData) 
            {
                PMIB_TCPTABLE_OWNER_PID tcpTable = nullptr;
                ULONG ulSize = 0;

				// Get the size of the table
                if (GetExtendedTcpTable(nullptr, &ulSize, TRUE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0) != ERROR_INSUFFICIENT_BUFFER) {
                    continue;
                }

                std::unique_ptr<BYTE[]> buffer(new BYTE[ulSize]);
                tcpTable = reinterpret_cast<PMIB_TCPTABLE_OWNER_PID>(buffer.get());

				// Get the table data
                if (GetExtendedTcpTable(tcpTable, &ulSize, TRUE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0) != NO_ERROR) 
                {
                    continue;
                }

                for (DWORD i = 0; i < tcpTable->dwNumEntries; i++) 
                {
                    if (tcpTable->table[i].dwOwningPid == data.getPids()[0]) 
                    {
                        MIB_TCPROW_OWNER_PID row = tcpTable->table[i];

                        // Enable ESTATS for this connection
                        TCP_ESTATS_DATA_RW_v0 rwData = { 0 };
                        rwData.EnableCollection = TRUE;

                        if (SetPerTcpConnectionEStats(reinterpret_cast<PMIB_TCPROW>(&row), TcpConnectionEstatsData,
                            reinterpret_cast<PUCHAR>(&rwData), 0, sizeof(rwData), 0) != NO_ERROR) 
                        {
                            continue;
                        }

                        if (row.dwState == MIB_TCP_STATE_ESTAB && row.dwRemoteAddr != htonl(INADDR_LOOPBACK)) 
                        {
                            ULONG rodSize = sizeof(TCP_ESTATS_DATA_ROD_v0);
                            std::vector<BYTE> rodBuffer(rodSize);
                            PTCP_ESTATS_DATA_ROD_v0 dataRod = reinterpret_cast<PTCP_ESTATS_DATA_ROD_v0>(rodBuffer.data());

							// Get the ESTATS data for this connection
                            if (GetPerTcpConnectionEStats(reinterpret_cast<PMIB_TCPROW>(&row), TcpConnectionEstatsData,
                                nullptr, 0, 0, nullptr, 0, 0,
                                reinterpret_cast<PUCHAR>(dataRod), 0, rodSize) == NO_ERROR) 
                            {

								std::cout << "DataBytesIn: " << dataRod->DataBytesIn << std::endl;
								std::cout << "DataBytesOut: " << dataRod->DataBytesOut << std::endl;

                                // Calculate Bytes In and Bytes Out
                                double bytesIn = static_cast<double>(dataRod->DataBytesIn);
                                double bytesOut = static_cast<double>(dataRod->DataBytesOut);

								double intervalSec = 500.0 / 1000.0; // Interval in seconds (will be changed in the future)

								long downloadRate = bytesIn / intervalSec;
								long uploadRate = bytesOut / intervalSec;

								double downloadPower = 1.138 * ((double)downloadRate / 300000); // 1.138 is the power consumption per byte for download (will be changed in the future using the config)
								double uploadPower = 1.138 * ((double)uploadRate / 300000); // 1.138 is the power consumption per byte for upload (will be changed in the future using the config)

								double averagePower = downloadPower + uploadPower;

								double intervalEnergy = averagePower * intervalSec;

                                {
                                    std::lock_guard<std::mutex> lock(data_mutex);
                                    auto it = std::find_if(monitoringData.begin(), monitoringData.end(), [&](const auto& d) 
                                        {
                                            return d.getPids()[0] == data.getPids()[0];
                                        });
                                    if (it != monitoringData.end()) 
                                    {
                                        it->updateNICEnergy(intervalEnergy);
                                    }
                                }
                            }
                        }
                    }
                }
            }

            screen.Post(Event::Custom);
            std::this_thread::sleep_for(std::chrono::milliseconds(500)); // interval based on user input (will be changed in the future)
        }
        });

    std::thread cpu_thread([&screen] 
    {
        while (true) 
        {
            double totalEnergy = 0.0;
            double startTotalPower = 0.0;
            double endTotalPower = 0.0;
            double avgPowerInterval = 0.0;
            std::vector<MonitoringData> localMonitoringData;

            {
                std::unique_lock<std::mutex> lock(data_mutex);
                new_data_cv.wait(lock, [] { return !monitoringData.empty(); });
                localMonitoringData = monitoringData;
            }

            for (auto& data : localMonitoringData) 
            {
				uint64_t startCPUTime = CPU::getCPUTime();
                uint64_t startPidTime = CPU::getPidTime(data.getPids()[0]);

                CPU::getCurrentPower(startTotalPower);

                std::this_thread::sleep_for(std::chrono::milliseconds(interval));

                CPU::getCurrentPower(endTotalPower);

                avgPowerInterval = (startTotalPower + endTotalPower) / 2;

                uint64_t endCPUTime = CPU::getCPUTime();
                uint64_t endPidTime = CPU::getPidTime(data.getPids()[0]);

                double pidTimeDiff = static_cast<double>(endPidTime) - static_cast<double>(startPidTime);
                double cpuTimeDiff = static_cast<double>(endCPUTime) - static_cast<double>(startCPUTime);

                if (pidTimeDiff > cpuTimeDiff) 
                {
                    std::cerr << "Error: Process time is greater than CPU time." << std::endl;
                    return 1;
                }

                double cpuUsage = (pidTimeDiff / cpuTimeDiff);

                double intervalEnergy = avgPowerInterval * cpuUsage * interval / 1000;

                totalEnergy += intervalEnergy;

                startCPUTime = endCPUTime;
                startPidTime = endPidTime;
                startTotalPower = 0.0;
                endTotalPower = 0.0;

				{
					std::lock_guard<std::mutex> lock(data_mutex);
					auto it = std::find_if(monitoringData.begin(), monitoringData.end(), [&](const auto& d) 
                        {
						    return d.getPids()[0] == data.getPids()[0];
						});
					if (it != monitoringData.end()) 
                    {
						it->updateCPUEnergy(totalEnergy);
					}
				}
            }

            screen.Post(Event::Custom);
        }
        });


	// Run the application
	screen.Loop(component);

	gpu_thread.join();
	sd_thread.join();
	nic_thread.join();
	cpu_thread.join();
	return 0;
}

void readCommand(string commandHandle)
{
    istringstream tokenStream(commandHandle);

    vector<string> chain;

    while (getline(tokenStream, commandHandle, ' ')) 
    {
        chain.push_back(commandHandle);
    }

    switch (actions[chain[0]])  //see line 70
    {
        case 1: //to do
            if (chain.size() == 2)
            {
                enable(chain[1]);
            }
            else
            {
                cout << "error, need 2 in total for enable and disable" << endl;
            }
            break;

        case 2://to do
            if (chain.size() == 2)
            {
                disable(chain[1]);
            }
            else
            {
                cout << "error, need 2 in total for enable and disable" << endl;
            }
            break;

        case 3:
            if (chain.size() == 4)
            {
                if (chain[1] == "-p")
                {
                    if (all_of(chain[2].begin(), chain[2].end(), ::isdigit))
                    {
                        if (chain[3] == "CPU" || chain[3] == "GPU" || chain[3] == "SD" || chain[3] == "NIC")
                        {
                            addProcPid(chain[2], chain[3]);
                        }
                        else
                        {
                            cout << "error fourth argument (must be CPU, GPU, SD or NIC)" << endl;
                        }
                    }
                    else
                    {
                        cout << "error third argument (must be an integer)" << endl;
                    }
                }
                else if (chain[1] == "-n")
                {
                    if (chain[3] == "CPU" || chain[3] == "GPU" || chain[3] == "SD" || chain[3] == "NIC")
                    {
                        addProcName(chain[2], chain[3]);
                    }
                    else
                    {
                        cout << "error fourth argument (must be CPU, GPU, SD or NIC)" << endl;
                    }
                }
                else
                {
                    cout << "error second argument (-p for pid / -n for name)" << endl;
                }
            }
            else
            {
                cout << "error, need 4 in total for add and remove" << endl;
            }
            break;

        case 4:
            if (chain.size() == 4)
            {
                if (chain[1] == "-p")
                {
                    if (all_of(chain[2].begin(), chain[2].end(), ::isdigit))
                    {
                        if (chain[3] == "CPU" || chain[3] == "GPU" || chain[3] == "SD" || chain[3] == "NIC")
                        {
                            removeProcPid(chain[2], chain[3]); //to do: check if component
                        }
                        else
                        {
                            cout << "error fourth argument (must be CPU, GPU, SD or NIC)" << endl;
                        }
                    }
                    else
                    {
                        cout << "error third argument (must be an integer)" << endl;
                    }
                }
                else if (chain[1] == "-n")
                {
                    if (chain[3] == "CPU" || chain[3] == "GPU" || chain[3] == "SD" || chain[3] == "NIC")
                    {
                        removeProcName(chain[2], chain[3]); //to do: check if component
                    }
                    else
                    {
                        cout << "error fourth argument (must be CPU, GPU, SD or NIC)" << endl;
                    }
                }
                else
                {
                    cout << "error second argument (-p for pid / -n for name)" << endl;
                }
            }
            else
            {
                cout << "error, need 4 in total for add and remove" << endl;
            }
            break;

        case 5:
            if (chain.size() == 2)
            {
                if (all_of(chain[1].begin(), chain[1].end(), ::isdigit))
                {
                    interval = stoi(chain[1]);
                    cout << "Interval has been changed" << endl;
                }
                else
                {
                    cout << "error second argument (must be an integer)" << endl;
                }
            }
            else
            {
                cout << "error, need 2 in total for interval" << endl;
            }
            break;

        case 6:
            // to do
            break;

        default:
            cout << "error first argument (list command: add/remove/enable/disable/interval/start/quit)" << endl;
            break;
    }
}

void addProcPid(string pid, string component)
{
    wstring processName = GetProcessNameByPID(stoi(pid));

    {
        std::unique_lock<std::mutex> lock(data_mutex);
        MonitoringData data(wstring_to_string(processName), { stoi(pid) });
        monitoringData.push_back(data);
		new_data_cv.notify_all();
    }
}

void addProcName(string name, string component)
{
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    
    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);

    wstring wstr(name.begin(), name.end());

    if (Process32First(hSnapshot, &pe32)) 
    {
        do 
        {
            if (_wcsicmp(pe32.szExeFile, wstr.c_str()) == 0) // Case-insensitive comparison
            { 
                std::stringstream ss;
                ss << pe32.th32ProcessID;
                auto it = find_if(comp[component].first.begin(), comp[component].first.end(), [&ss](process o) {return o.getPid() == ss.str(); });
                if (it >= comp[component].first.end())
                {
                    comp[component].first.push_back(process(ss.str(), name));
                    cout << name << " (Pid: " << ss.str() << ") has been added" << endl;
                }
            }
        } while (Process32Next(hSnapshot, &pe32));
    }

    CloseHandle(hSnapshot);
}

void removeProcPid(string pid, string component)
{
    auto it = find_if(comp[component].first.begin(), comp[component].first.end(), [&pid](process o) { return o.getPid() == pid; });

    if (it != comp[component].first.end())
    {
        comp[component].first.erase(it);

        cout << "The process has been removed" << endl;
    }
    else
    {
        cout << "No process has been found" << endl;
    }
}

void removeProcName(string name, string component)
{
    for (auto it = comp[component].first.begin(); it != comp[component].first.end();)
    {
        if (it->getName() == name)
        {
            cout << it->getName() << " (Pid: " << it->getPid() << ") has been removed" << endl;
            it = comp[component].first.erase(it);
        }
        else
        {
            ++it;
        }
    }
    //to do remove via name
}

void enable(string component)
{
    if (!comp[component].second)
    {
        comp[component].second = true;
        cout << component << " is now enable" << endl;
    }
    else
    {
        cout << component << " was already enable" << endl;
    }
    //to do enable component
}

void disable(string component)
{
    if (comp[component].second)
    {
        comp[component].second = false;
        cout << component << " is now disable" << endl;
    }
    else
    {
        cout << component << " was already disable" << endl;
    }
    //to do disable component
}