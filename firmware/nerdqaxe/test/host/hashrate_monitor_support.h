#pragma once
#include "mining_control_state.h"

namespace FiveTratumMining {
inline OperationGate &operationGate() { static OperationGate gate; return gate; }
using MiningOperation = OperationLease;
}

#include <atomic>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <memory>
#include <pthread.h>
#include <thread>

// Hardware/RTOS boundaries only. The runner compiles the production monitor's
// method bodies unchanged, with no real task, serial port or register traffic.
#define private public
#include "hashrate_monitor_task.h"
#undef private

extern std::atomic<int64_t> hostNow;
extern std::atomic<int> hostTaskResult;

class PThreadGuard {
    pthread_mutex_t &mutex;
public:
    explicit PThreadGuard(pthread_mutex_t &value) : mutex(value) { pthread_mutex_lock(&mutex); }
    ~PThreadGuard() { pthread_mutex_unlock(&mutex); }
};

class Board {
public:
    int count = 4;
    int getAsicCount() { return count; }
};
class Asic {
public:
    void resetCounter(uint8_t) {}
    void readCounter(uint8_t) {}
};
struct HostSystem {
    Board *board = nullptr;
    Board *getBoard() { return board; }
};
struct HostPower { bool isShutdown() { return false; } };
extern HostSystem SYSTEM_MODULE;
extern HostPower POWER_MANAGEMENT_MODULE;

template<typename... Args> inline void hostLog(Args...) {}
#define ESP_LOGE(...) hostLog(__VA_ARGS__)
#define ESP_LOGI(...) hostLog(__VA_ARGS__)
#define ESP_LOGW(...) hostLog(__VA_ARGS__)
inline BaseType_t xTaskCreatePSRAM(TaskFunction_t, const char *, uint32_t, void *, UBaseType_t, TaskHandle_t *) {
    return hostTaskResult.load();
}

inline void releaseHostSamples(HashrateMonitor &monitor) {
    delete[] monitor.m_chipHashrate;
    delete[] monitor.m_prevResponse;
    delete[] monitor.m_prevCounter;
    delete[] monitor.m_chipSampleAvailable;
    monitor.m_chipHashrate = nullptr;
    monitor.m_prevResponse = nullptr;
    monitor.m_prevCounter = nullptr;
    monitor.m_chipSampleAvailable = nullptr;
    monitor.m_asicCount = 0;
}
