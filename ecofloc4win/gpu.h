#pragma once

#include <vector>

namespace GPU
{
	std::vector<int> getGPUUsage(std::vector<int> pids);
	int getGPUPower();
	int getGPUJoules(std::vector<int> pids, int ms);
};
