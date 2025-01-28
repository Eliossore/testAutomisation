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
#include <atomic>

#include "process.h"         // Custom header for process handling
#include "gpu.h"             // Custom header for GPU monitoring
#include "CPU.h"
#include "MonitoringData.h"  // Custom header for monitoring data
#include "Utils.h"

#include "ftxui/component/screen_interactive.hpp"
#include "ftxui/component/component.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/dom/table.hpp"
#include "ftxui/dom/node.hpp"
#include "ftxui/screen/color.hpp"
#include <unordered_set>

using namespace ftxui;

std::vector<MonitoringData> monitoringData = {};
std::condition_variable new_data_cv;
std::mutex data_mutex;
std::atomic<bool> new_data_cpu(false);
std::atomic<bool> new_data_gpu(false);
std::atomic<bool> new_data_sd(false);
std::atomic<bool> new_data_nic(false);

void readCommand(string);

void addProcPid(const string&, const string&);
void addProcName(const string&, const string&);

void removeProcByLineNumber(const string&) noexcept;
void removeProcPid(string, string);

void enable(const string&, const string&);
void disable(const string&, const string&);

std::wstring getLocalizedCounterPath(const std::wstring& processName, const std::string& counterName);

// Actions map for command handling
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

unordered_map<string, pair<vector<process>, bool>> comp = { {"CPU", {{}, false}}, {"GPU", {{}, false} }, {"SD", {{}, false }}, {"NIC", {{}, false }} };

std::atomic<int> interval = 500;

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
	return L"\\" + localizedProcessNameW + L"(" + processName + L")\\" + localizedNameW;
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
	int terminal_height = Utils::GetTerminalHeight();
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
		int terminal_height = Utils::GetTerminalHeight();
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
			std::vector<MonitoringData> localMonitoringData;
			while (true)
			{
				// check if new_data is false and localMonitoringData is empty
				if (new_data_gpu.load(std::memory_order_release) == false && localMonitoringData.empty())
				{
					std::this_thread::sleep_for(std::chrono::milliseconds(interval));
					continue;
				}

				if (new_data_gpu)
				{
					std::unique_lock<std::mutex> lock(data_mutex);
					localMonitoringData = monitoringData;
					new_data_gpu.store(false, std::memory_order_release);
				}

				for (auto& data : localMonitoringData)
				{
					if (!data.isGPUEnabled() || data.getPids().empty())
					{
						continue;
					}

					int gpu_joules = GPU::getGPUJoules(data.getPids(), interval);
					{
						std::lock_guard<std::mutex> lock(data_mutex);
						auto it = std::find_if(monitoringData.begin(), monitoringData.end(),
							[&](const auto& d) { return d.getPids() == data.getPids(); });

						if (it != monitoringData.end())
						{
							it->updateGPUEnergy(gpu_joules);
						}
					}
				}

				screen.Post(Event::Custom);
				std::this_thread::sleep_for(std::chrono::milliseconds(interval));
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

			std::vector<MonitoringData> localMonitoringData;
			while (true)
			{

				// check if new_data is false and localMonitoringData is empty
				if (new_data_sd.load(std::memory_order_release) == false && localMonitoringData.empty())
				{
					std::this_thread::sleep_for(std::chrono::milliseconds(interval));
					continue;
				}

				if (new_data_sd)
				{
					std::unique_lock<std::mutex> lock(data_mutex);
					localMonitoringData = monitoringData;
					new_data_sd.store(false, std::memory_order_release);
				}


				for (auto& data : localMonitoringData)
				{
					if (!data.isSDEnabled())
					{
						continue;
					}

					if (data.getPids().empty())
					{
						continue;
					}

					std::wstring instanceName = GetInstanceForPID(data.getPids()[0]);
					if (instanceName.empty())
					{
						continue;
					}
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

						double intervalEnergy = averagePower * interval / 1000;

						{
							std::lock_guard<std::mutex> lock(data_mutex);
							auto it = std::find_if(monitoringData.begin(), monitoringData.end(), [&](const auto& d)
								{
									return !d.getPids().empty();
								});
							if (it != monitoringData.end())
							{
								it->updateSDEnergy(intervalEnergy);
							}
						}
					}
				}

				screen.Post(Event::Custom);
				std::this_thread::sleep_for(std::chrono::milliseconds(interval));
			}

			PdhCloseQuery(query);
		});

	std::thread nic_thread([&screen]
		{
			std::vector<MonitoringData> localMonitoringData;
			while (true)
			{
				// check if new_data is false and localMonitoringData is empty
				if (new_data_nic.load(std::memory_order_release) == false && localMonitoringData.empty())
				{
					std::this_thread::sleep_for(std::chrono::milliseconds(interval));
					continue;
				}

				if (new_data_nic)
				{
					std::unique_lock<std::mutex> lock(data_mutex);
					localMonitoringData = monitoringData;
					new_data_nic.store(false, std::memory_order_release);
				}

				PMIB_TCPTABLE_OWNER_PID tcpTable = nullptr;
				ULONG ulSize = 0;

				// Get the size of the table
				if (GetExtendedTcpTable(nullptr, &ulSize, TRUE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0) != ERROR_INSUFFICIENT_BUFFER) {
					continue;
				}

				std::unique_ptr<BYTE[]> buffer(new BYTE[ulSize]);
				tcpTable = reinterpret_cast<PMIB_TCPTABLE_OWNER_PID>(buffer.get());

				// Get the table data
				if (GetExtendedTcpTable(tcpTable, &ulSize, TRUE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0) != NO_ERROR) {
					continue;
				}

				for (auto& data : localMonitoringData) {

					if (!data.isNICEnabled()) {
						continue;
					}


					for (DWORD i = 0; i < tcpTable->dwNumEntries; i++) {
						// Loop inside the pid list of data
						for (int& pid : data.getPids())
						{
							if (tcpTable->table[i].dwOwningPid == pid)
							{
								MIB_TCPROW_OWNER_PID row = tcpTable->table[i];

								// Enable ESTATS for this connection
								TCP_ESTATS_DATA_RW_v0 rwData = { 0 };
								rwData.EnableCollection = TRUE;

								if (SetPerTcpConnectionEStats(reinterpret_cast<PMIB_TCPROW>(&row), TcpConnectionEstatsData,
									reinterpret_cast<PUCHAR>(&rwData), 0, sizeof(rwData), 0) != NO_ERROR) {
									continue;
								}

								if (row.dwState == MIB_TCP_STATE_ESTAB && row.dwRemoteAddr != htonl(INADDR_LOOPBACK)) {
									ULONG rodSize = sizeof(TCP_ESTATS_DATA_ROD_v0);
									std::vector<BYTE> rodBuffer(rodSize);
									PTCP_ESTATS_DATA_ROD_v0 dataRod = reinterpret_cast<PTCP_ESTATS_DATA_ROD_v0>(rodBuffer.data());

									// Get the ESTATS data for this connection
									if (GetPerTcpConnectionEStats(reinterpret_cast<PMIB_TCPROW>(&row), TcpConnectionEstatsData,
										nullptr, 0, 0, nullptr, 0, 0,
										reinterpret_cast<PUCHAR>(dataRod), 0, rodSize) == NO_ERROR) {

										// Calculate Bytes In and Bytes Out
										double bytesIn = static_cast<double>(dataRod->DataBytesIn);
										double bytesOut = static_cast<double>(dataRod->DataBytesOut);

										double intervalSec = interval / 1000.0; // Interval in seconds (will be changé in the future)

										long downloadRate = bytesIn / intervalSec;
										long uploadRate = bytesOut / intervalSec;

										double downloadPower = 1.138 * ((double)downloadRate / 300000); // 1.138 is the power consumption per byte for download (will be changé in the future using the config)
										double uploadPower = 1.138 * ((double)uploadRate / 300000); // 1.138 is the power consumption per byte for upload (will be changé in the future using the config)

										double averagePower = downloadPower + uploadPower;

										{
											std::lock_guard<std::mutex> lock(data_mutex);
											// Update the NIC energy for the process
											auto it = std::find_if(monitoringData.begin(), monitoringData.end(), [&](const MonitoringData& d)
												{
													return !d.getPids().empty();
												});
											if (it != monitoringData.end())
											{
												it->updateNICEnergy(averagePower);
											}
										}
									}
								}
							}
						}
					}
				}

				screen.Post(Event::Custom);
				std::this_thread::sleep_for(std::chrono::milliseconds(interval)); // interval based on user input (will be changé in the future)
			}
		});

	std::thread cpu_thread([&screen]
		{
			std::vector<MonitoringData> localMonitoringData;
			while (true)
			{
				double totalEnergy = 0.0;
				double startTotalPower = 0.0;
				double endTotalPower = 0.0;
				double avgPowerInterval = 0.0;

				// check if new_data is false and localMonitoringData is empty
				if (new_data_cpu.load(std::memory_order_release) == false && localMonitoringData.empty())
				{
					std::cout << "No new data" << std::endl;
					std::this_thread::sleep_for(std::chrono::milliseconds(interval));
					continue;
				}

				if (new_data_cpu)
				{
					std::unique_lock<std::mutex> lock(data_mutex);
					localMonitoringData = monitoringData;
					new_data_cpu.store(false, std::memory_order_release);
				}

				// Process each monitoring data entry
				for (auto& data : localMonitoringData)
				{
					if (!data.isCPUEnabled())
					{
						continue;
					}

					// Ensure the PID list is not empty
					if (data.getPids().empty())
					{
						std::cerr << "Error: No PIDs available for monitoring data." << std::endl;
						continue;
					}

					// Get initial CPU and process times
					uint64_t startCPUTime = CPU::getCPUTime();
					uint64_t startPidTime = CPU::getPidTime(data.getPids()[0]);

					CPU::getCurrentPower(startTotalPower);

					// Monitor for the specified interval
					std::this_thread::sleep_for(std::chrono::milliseconds(interval));

					CPU::getCurrentPower(endTotalPower);

					avgPowerInterval = (startTotalPower + endTotalPower) / 2;

					uint64_t endCPUTime = CPU::getCPUTime();
					uint64_t endPidTime = CPU::getPidTime(data.getPids()[0]);

					// Calculate time differences
					double pidTimeDiff = static_cast<double>(endPidTime) - static_cast<double>(startPidTime);
					double cpuTimeDiff = static_cast<double>(endCPUTime) - static_cast<double>(startCPUTime);

					// Validate time differences
					if (pidTimeDiff > cpuTimeDiff)
					{
						std::cerr << "Error: Process time is greater than CPU time." << std::endl;
						continue;
					}

					// Calculate CPU usage and energy consumption
					double cpuUsage = (pidTimeDiff / cpuTimeDiff);
					double intervalEnergy = avgPowerInterval * cpuUsage * interval / 1000;

					totalEnergy += intervalEnergy;

					// Update monitoring data safely
					{
						std::lock_guard<std::mutex> lock(data_mutex);
						auto it = std::find_if(monitoringData.begin(), monitoringData.end(),
							[&](const auto& d) { return d.getPids() == data.getPids(); });

						if (it != monitoringData.end()) {
							it->updateCPUEnergy(totalEnergy);
						}
					}
				}

				// Notify the screen of an update
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

	switch (actions[chain[0]])
	{
	case 1:
		if (chain.size() == 3)
		{
			enable(chain[1], chain[2]);
		}
		else
		{
			cout << "error, need 2 in total for enable and disable" << endl;
		}
		break;

	case 2:
		if (chain.size() == 3)
		{
			disable(chain[1], chain[2]);
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
		if (chain.size() == 2)
		{
			if (all_of(chain[1].begin(), chain[1].end(), ::isdigit))
			{
				removeProcByLineNumber(chain[1]);
			}
			else
			{
				cout << "error third argument (must be an integer)" << endl;
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
		break;

	default:
		cout << "error first argument (list command: add/remove/enable/disable/interval/start/quit)" << endl;
		break;
	}
}

void addProcPid(const std::string& pid, const std::string& component)
{
	enum ComponentType { CPU, GPU, SD, NIC };

	std::unordered_map<std::string, ComponentType> componentMap = {
		{ "CPU", ComponentType::CPU },
		{ "GPU", ComponentType::GPU },
		{ "SD", ComponentType::SD },
		{ "NIC", ComponentType::NIC }
	};

	try
	{
		int processId = std::stoi(pid); // Convert PID to integer
		std::wstring processName = GetProcessNameByPID(processId);

		// Check if the process name is valid
		if (processName.empty()) {
			std::cerr << "Error: Invalid PID or process not found." << std::endl;
			return;
		}

		{
			std::unique_lock<std::mutex> lock(data_mutex);

			// Check for duplicate before adding
			auto it = std::find_if(monitoringData.begin(), monitoringData.end(),
				[processId](const MonitoringData& data) {
					return std::find(data.getPids().begin(), data.getPids().end(), processId) != data.getPids().end();
				});

			if (it == monitoringData.end()) {
				MonitoringData data(Utils::wstringToString(processName), { processId });
				data.enableComponent(component);
				monitoringData.push_back(data);
				auto it2 = componentMap.find(component);
				if (it2 != componentMap.end())
				{
					it2->second;
				}
				
				switch (it2->second)
				{
				case CPU:
					new_data_cpu.store(true, std::memory_order_release);
					break;
				case GPU:
					new_data_gpu.store(true, std::memory_order_release);
					break;
				case SD:
					new_data_sd.store(true, std::memory_order_release);
					break;
				case NIC:
					new_data_nic.store(true, std::memory_order_release);
					break;
				}
			}
			else {
				std::cerr << "Warning: Process with PID " << pid << " is already being monitored." << std::endl;
			}
		}
	}
	catch (const std::exception& ex)
	{
		std::cerr << "Error: Exception while adding PID " << pid << ": " << ex.what() << std::endl;
	}
}


void addProcName(const std::string& name, const std::string& component)
{
	enum ComponentType { CPU, GPU, SD, NIC };

	std::unordered_map<std::string, ComponentType> componentMap = {
		{ "CPU", ComponentType::CPU },
		{ "GPU", ComponentType::GPU },
		{ "SD", ComponentType::SD },
		{ "NIC", ComponentType::NIC }
	};

	HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

	if (hSnapshot == INVALID_HANDLE_VALUE) {
		std::cerr << "Error: Unable to create process snapshot." << std::endl;
		return;
	}

	PROCESSENTRY32 pe32;
	pe32.dwSize = sizeof(PROCESSENTRY32);

	std::wstring wstr(name.begin(), name.end());
	std::vector<int> pids;

	if (Process32First(hSnapshot, &pe32))
	{
		do
		{
			if (_wcsicmp(pe32.szExeFile, wstr.c_str()) == 0) // Case-insensitive comparison
			{
				int processId = pe32.th32ProcessID;

				// Check for duplicates in `comp[component].first`
				auto it = std::find_if(comp[component].first.begin(), comp[component].first.end(),
					[&processId](const process& o) {
						return o.getPid() == std::to_string(processId);
					});

				if (it == comp[component].first.end())
				{
					pids.push_back(processId);
					comp[component].first.push_back(process(std::to_string(processId), name));
				}
			}
		} while (Process32Next(hSnapshot, &pe32));
	}

	CloseHandle(hSnapshot);

	// Add valid processes to monitoringData
	if (!pids.empty())
	{
		{
			std::unique_lock<std::mutex> lock(data_mutex);
			MonitoringData data(name, pids);
			data.enableComponent(component);
			monitoringData.push_back(data);
			auto it2 = componentMap.find(component);
			if (it2 != componentMap.end())
			{
				it2->second;
			}

			switch (it2->second)
			{
			case CPU:
				new_data_cpu.store(true, std::memory_order_release);
				break;
			case GPU:
				new_data_gpu.store(true, std::memory_order_release);
				break;
			case SD:
				new_data_sd.store(true, std::memory_order_release);
				break;
			case NIC:
				new_data_nic.store(true, std::memory_order_release);
				break;
			}
		}
	}
	else
	{
		std::cerr << "Error: No processes found with name " << name << "." << std::endl;
	}
}


void removeProcByLineNumber(const std::string& lineNumber) noexcept
{
	try
	{
		int line = std::stoi(lineNumber);
		if (line < 0)
		{
			std::cerr << "Error: Line number cannot be negative." << std::endl;
			return;
		}

		std::unique_lock<std::mutex> lock(data_mutex);
		if (line >= monitoringData.size())
		{
			std::cerr << "Error: Line number is out of range." << std::endl;
			return;
		}

		// Store PIDs to remove before modifying containers
		const auto& data = monitoringData[line];
		std::unordered_set<unsigned int> pidsToRemove;
		pidsToRemove.reserve(data.getPids().size());  // Pre-allocate space

		for (const auto& pid : data.getPids())
		{
			pidsToRemove.insert(pid);
		}

		// Track components that will be affected for more detailed logging
		std::vector<std::string> affectedComponents;

		// More efficient removal from comp
		for (auto& [key, value] : comp)
		{
			auto& processes = value.first;
			auto originalSize = processes.size();

			// Use erase-remove idiom with an unordered_set for O(1) lookup
			processes.erase(
				std::remove_if(processes.begin(), processes.end(),
					[&pidsToRemove](const process& p) {
						return pidsToRemove.count(std::stoi(p.getPid())) > 0;
					}),
				processes.end()
			);

			// If processes were removed from this component, log it
			if (processes.size() < originalSize)
			{
				affectedComponents.push_back(key);
			}
		}

		// Remove from monitoringData
		monitoringData.erase(monitoringData.begin() + line);

		// Optional: Reduce memory footprint
		monitoringData.shrink_to_fit();

		// Atomic updates with memory ordering
		new_data_cpu.store(true, std::memory_order_release);
		new_data_gpu.store(true, std::memory_order_release);
		new_data_sd.store(true, std::memory_order_release);
		new_data_nic.store(true, std::memory_order_release);

		// Enhanced logging
		std::cout << "Process has been removed from line " << line
			<< ". Affected components: ";
		for (const auto& component : affectedComponents)
		{
			std::cout << component << " ";
		}
		std::cout << std::endl;
	}
	catch (const std::invalid_argument& e)
	{
		std::cerr << "Error: Invalid line number. Must be a number." << std::endl;
	}
	catch (const std::out_of_range& e)
	{
		std::cerr << "Error: Line number is too large." << std::endl;
	}
	catch (...)
	{
		std::cerr << "Unexpected error during process removal." << std::endl;
	}
}

void enable(const string& lineNumber, const string& component)
{
	enum ComponentType { CPU, GPU, SD, NIC };
	std::unordered_map<std::string, ComponentType> comp = {
		{ "CPU", ComponentType::CPU },
		{ "GPU", ComponentType::GPU },
		{ "SD", ComponentType::SD },
		{ "NIC", ComponentType::NIC }
	};

	try {
		int line = std::stoi(lineNumber);
		if (line < 0)
		{
			std::cerr << "Error: Line number cannot be negative." << std::endl;
			return;
		}

		std::unique_lock<std::mutex> lock(data_mutex);
		if (line >= monitoringData.size())
		{
			std::cerr << "Error: Line number is out of range." << std::endl;
			return;
		}

		auto& data = monitoringData[line];
		data.enableComponent(component);

		auto it = comp.find(component);
		if (it != comp.end())
		{
			it->second;
		}

		switch (it->second)
		{
		case CPU:
			new_data_cpu.store(true, std::memory_order_release);
			break;
		case GPU:
			new_data_gpu.store(true, std::memory_order_release);
			break;
		case SD:
			new_data_sd.store(true, std::memory_order_release);
			break;
		case NIC:
			new_data_nic.store(true, std::memory_order_release);
			break;
		}

		std::cout << "Component " << component << " has been enabled for process " << data.getName() << " at line " << line << std::endl;

	}
	catch (const std::invalid_argument& e)
	{
		std::cerr << "Error: Invalid line number. Must be a number." << std::endl;
	}
	catch (const std::out_of_range& e)
	{
		std::cerr << "Error: Line number is too large." << std::endl;
	}
	catch (...)
	{
		std::cerr << "Unexpected error during process removal." << std::endl;
	}
}

void disable(const string& lineNumber, const string& component)
{
	enum ComponentType { CPU, GPU, SD, NIC };
	std::unordered_map<std::string, ComponentType> comp = {
		{ "CPU", ComponentType::CPU },
		{ "GPU", ComponentType::GPU },
		{ "SD", ComponentType::SD },
		{ "NIC", ComponentType::NIC }
	};

	try {
		int line = std::stoi(lineNumber);
		if (line < 0)
		{
			std::cerr << "Error: Line number cannot be negative." << std::endl;
			return;
		}

		std::unique_lock<std::mutex> lock(data_mutex);
		if (line >= monitoringData.size())
		{
			std::cerr << "Error: Line number is out of range." << std::endl;
			return;
		}

		auto& data = monitoringData[line];
		data.disableComponent(component);

		auto it = comp.find(component);
		if (it != comp.end())
		{
			it->second;
		}

		switch (it->second)
		{
		case CPU:
			new_data_cpu.store(true, std::memory_order_release);
			break;
		case GPU:
			new_data_gpu.store(true, std::memory_order_release);
			break;
		case SD:
			new_data_sd.store(true, std::memory_order_release);
			break;
		case NIC:
			new_data_nic.store(true, std::memory_order_release);
			break;
		}

		std::cout << "Component " << component << " has been enabled for process " << data.getName() << " at line " << line << std::endl;

	}
	catch (const std::invalid_argument& e)
	{
		std::cerr << "Error: Invalid line number. Must be a number." << std::endl;
	}
	catch (const std::out_of_range& e)
	{
		std::cerr << "Error: Line number is too large." << std::endl;
	}
	catch (...)
	{
		std::cerr << "Unexpected error during process removal." << std::endl;
	}
}