#pragma once

#include <vector>
#include <string>
#include <windows.h>
#include <map>

struct IoEventInfo {
	DWORD pid;
	std::wstring processName;
	USHORT operationType;
	ULONG bytesTransferred = 0;  // Track total bytes transferred for each IRP
};

class MonitoringData
{
public:
	std::string name;
	std::vector<int> pids;
	std::map<ULONGLONG, IoEventInfo> irpMap;

	bool cpuEnabled = false;
	bool gpuEnabled = false;
	bool sdEnabled = false;
	bool nicEnabled = false;

	double cpuEnergy = 0.0;
	double gpuEnergy = 0.0;
	double sdEnergy = 0.0;
	double nicEnergy = 0.0;

	MonitoringData(const std::string& appName = "", const std::vector<int>& pids = {}) : name(appName), pids(pids) {}

	std::string getName() const;
	std::vector<int> getPids() const;

	void setCPUEnabled(bool enabled);
	void setGPUEnabled(bool enabled);
	void setSDEnabled(bool enabled);
	void setNICEnabled(bool enabled);

	bool isCPUEnabled() const;
	bool isGPUEnabled() const;
	bool isSDEnabled() const;
	bool isNICEnabled() const;

	void setCPUEnergy(double energy);
	void setGPUEnergy(double energy);
	void setSDEnergy(double energy);
	void setNICEnergy(double energy);

	void updateCPUEnergy(double energy);
	void updateGPUEnergy(double energy);
	void updateSDEnergy(double energy);
	void updateNICEnergy(double energy);

	void addIrp(ULONGLONG irpAddress, const IoEventInfo& info);
	void updateIrp(ULONGLONG irpAddress, ULONG bytesTransferred);
	void removeIrp(ULONGLONG irpAddress);
};

