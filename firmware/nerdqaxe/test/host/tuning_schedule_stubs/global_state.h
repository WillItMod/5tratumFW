#pragma once
#include "http_utils.h"
struct PowerManagementTask {
    bool shutdown=false,protectionReady=true;
    void lock() {}
    void unlock() {}
    bool isShutdown(){return shutdown;}
    bool refreshProtectionForMining(){return protectionReady;}
};
extern PowerManagementTask POWER_MANAGEMENT_MODULE;
template<class T> struct LockGuard {T &object;explicit LockGuard(T &value):object(value){object.lock();}~LockGuard(){object.unlock();}};
