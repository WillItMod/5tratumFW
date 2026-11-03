#pragma once
#include <pthread.h>
namespace FiveTratumAutotune {
// One physical command frame is ordered against a manual/OTA revocation.
// Never hold this across ramp delays, NVS, a config mutex or sensor polling.
inline pthread_mutex_t writeGate = PTHREAD_MUTEX_INITIALIZER;
struct WriteGuard {
    WriteGuard() { pthread_mutex_lock(&writeGate); }
    ~WriteGuard() { pthread_mutex_unlock(&writeGate); }
    WriteGuard(const WriteGuard &) = delete;
    WriteGuard &operator=(const WriteGuard &) = delete;
};
bool transitionAllowed();
}
