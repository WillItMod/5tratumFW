#pragma once
#include <cstdint>
struct TuningSensors {
    float asicTempC = 0, vrmTempC = 0, powerW = 0, vin = 0, coreVoltageMv = 0, outputCurrentA = 0;
    int64_t capturedAtUs = 0;
    bool measuredInputPower = false;
};
