#include "pch.h"

#include "Wrapper.h"
#include <vector>

extern "C" __declspec(dllexport) float* getCPUCoresPower(int* size) {
	ManagedBridge^ bridge = gcnew ManagedBridge();
	float^ power = bridge->getCPUCoresPower();
	delete bridge;
	
	*size = 1;
	float* powerArray = new float[*size];
	powerArray[0] = *power;

	return powerArray;
}