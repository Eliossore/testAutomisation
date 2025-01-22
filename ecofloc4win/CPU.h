#pragma once

#include <iostream>
#include <Windows.h>

namespace CPU
{
	uint64_t fromFileTime(const FILETIME& ft);
	uint64_t getCPUTime();
	uint64_t getPidTime(DWORD pid);
	bool getCurrentPower(double& power);
};

