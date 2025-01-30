#pragma once

#include <msclr/marshal_cppstd.h>
#include <iostream>
#include <ctime>

using namespace System;
using namespace System::Collections::Generic;
using namespace LibreHardwareMonitor::Hardware;

public ref class ManagedBridge
{
public:
    ManagedBridge() {
        try {
            computer = gcnew Computer();
            computer->IsCpuEnabled = true;
            computer->Open();
            lastUpdate = DateTime::Now - updateInterval;

            InitializeSensors();
        }
        catch (Exception^ ex) {
            Console::WriteLine("Error in ManagedBridge constructor: " + ex->Message);
        }
    }

    float^ getCPUCoresPower() {
        try {
            RefreshHardware();
            return cpuPower;
        }
        catch (Exception^ ex) {
            Console::WriteLine("Error in getCPUCoresPower: " + ex->Message);
            return nullptr;
        }
    }

private:
    Computer^ computer;
    float^ cpuPower = gcnew float();
    List<ISensor^>^ powerSensors = gcnew List<ISensor^>();
    DateTime lastUpdate;
    TimeSpan updateInterval = TimeSpan::FromSeconds(2); // 2-second refresh interval

    void InitializeSensors() {
        try {
            for (int i = 0; i < computer->Hardware->Count; i++) {
                IHardware^ hardware = computer->Hardware[i];
                if (hardware->HardwareType == HardwareType::Cpu) {
                    hardware->Update(); // Initialize sensor list

                    for (int j = 0; j < hardware->Sensors->Length; j++) {
                        ISensor^ sensor = hardware->Sensors[j];
                        if (sensor->Name->Contains("CPU Cores")) {
                            if (sensor->SensorType == SensorType::Power) {
                                powerSensors->Add(sensor);
                            }
                        }
                    }
                }
            }
        }
        catch (Exception^ ex) {
            Console::WriteLine("Error initializing sensors: " + ex->Message);
        }
    }

    void RefreshHardware() {
        try {
            DateTime now = DateTime::Now;
            if ((now - lastUpdate) >= updateInterval) {
                lastUpdate = now;

                cpuPower = 0.0f;

                for each (ISensor ^ sensor in powerSensors) {
                    if (sensor->Value.HasValue) {
                        cpuPower = sensor->Value.Value;
                    }
                }
            }
        }
        catch (Exception^ ex) {
            Console::WriteLine("Error in RefreshHardware: " + ex->Message);
        }
    }
};

