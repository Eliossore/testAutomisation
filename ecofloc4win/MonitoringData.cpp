#include "MonitoringData.h"

std::string MonitoringData::getName() const
{
	return name;
}

std::vector<int> MonitoringData::getPids() const
{
	return pids;
}

void MonitoringData::setCPUEnabled(bool enabled)
{
	cpuEnabled = enabled;
}

void MonitoringData::setGPUEnabled(bool enabled)
{
	gpuEnabled = enabled;
}

void MonitoringData::setSDEnabled(bool enabled)
{
	sdEnabled = enabled;
}

void MonitoringData::setNICEnabled(bool enabled)
{
	nicEnabled = enabled;
}

bool MonitoringData::isCPUEnabled() const
{
	return cpuEnabled;
}

bool MonitoringData::isGPUEnabled() const
{
	return gpuEnabled;
}

bool MonitoringData::isSDEnabled() const
{
	return sdEnabled;
}

bool MonitoringData::isNICEnabled() const
{
	return nicEnabled;
}

void MonitoringData::setCPUEnergy(double energy)
{
	cpuEnergy = energy;
}

void MonitoringData::setGPUEnergy(double energy)
{
	gpuEnergy = energy;
}

void MonitoringData::setSDEnergy(double energy)
{
	sdEnergy = energy;
}

void MonitoringData::setNICEnergy(double energy)
{
	nicEnergy = energy;
}

void MonitoringData::updateCPUEnergy(double energy)
{
	cpuEnergy += energy;
}

void MonitoringData::updateGPUEnergy(double energy)
{
	gpuEnergy += energy;
}

void MonitoringData::updateSDEnergy(double energy)
{
	sdEnergy += energy;
}

void MonitoringData::updateNICEnergy(double energy)
{
	nicEnergy += energy;
}

void MonitoringData::addIrp(ULONGLONG irpAddress, const IoEventInfo& info) {
	irpMap[irpAddress] = info;
}

void MonitoringData::updateIrp(ULONGLONG irpAddress, ULONG bytesTransferred) {
	if (irpMap.find(irpAddress) != irpMap.end()) {
		irpMap[irpAddress].bytesTransferred += bytesTransferred;
	}
}

void MonitoringData::removeIrp(ULONGLONG irpAddress) {
	irpMap.erase(irpAddress);
}