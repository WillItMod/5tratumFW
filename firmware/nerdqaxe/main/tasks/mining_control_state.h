#pragma once

// Production coordination core, shared with desktop sanitizer tests. A task
// lease covers its complete UART/result transaction, never just a flag check.
#include <cstdint>
#include <limits>
#include <pthread.h>

namespace FiveTratumMining {

class OperationGate {
    pthread_mutex_t m_mutex = PTHREAD_MUTEX_INITIALIZER;
    bool m_open = true;
    uint64_t m_generation = 1;
    unsigned m_active = 0;
public:
    uint64_t enter() {
        pthread_mutex_lock(&m_mutex);
        const uint64_t generation = m_open ? m_generation : 0;
        if (generation) ++m_active;
        pthread_mutex_unlock(&m_mutex);
        return generation;
    }
    void leave(uint64_t generation) {
        if (!generation) return;
        pthread_mutex_lock(&m_mutex);
        if (m_active) --m_active;
        pthread_mutex_unlock(&m_mutex);
    }
    bool current(uint64_t generation) {
        pthread_mutex_lock(&m_mutex);
        const bool valid = generation && m_open && generation == m_generation;
        pthread_mutex_unlock(&m_mutex);
        return valid;
    }
    bool block() {
        pthread_mutex_lock(&m_mutex);
        bool valid = true;
        if (m_open) {
            m_open = false;
            if (m_generation == std::numeric_limits<uint64_t>::max()) valid = false;
            else ++m_generation;
        }
        pthread_mutex_unlock(&m_mutex);
        return valid;
    }
    bool open() {
        pthread_mutex_lock(&m_mutex);
        const bool safe = !m_active && m_generation != std::numeric_limits<uint64_t>::max();
        if (safe) m_open = true;
        pthread_mutex_unlock(&m_mutex);
        return safe;
    }
    bool idle() {
        pthread_mutex_lock(&m_mutex);
        const bool idle = m_active == 0;
        pthread_mutex_unlock(&m_mutex);
        return idle;
    }
    bool isOpen() {
        pthread_mutex_lock(&m_mutex);
        const bool open = m_open;
        pthread_mutex_unlock(&m_mutex);
        return open;
    }
    uint64_t generation() {
        pthread_mutex_lock(&m_mutex);
        const uint64_t generation = m_generation;
        pthread_mutex_unlock(&m_mutex);
        return generation;
    }
};

class OperationLease {
    OperationGate &m_gate;
    uint64_t m_generation;
public:
    explicit OperationLease(OperationGate &gate) : m_gate(gate), m_generation(gate.enter()) {}
    ~OperationLease() { m_gate.leave(m_generation); }
    OperationLease(const OperationLease &) = delete;
    OperationLease &operator=(const OperationLease &) = delete;
    explicit operator bool() const { return m_generation != 0; }
    bool isCurrent() const { return m_gate.current(m_generation); }
    uint64_t generation() const { return m_generation; }
};

struct ControlInputs {
    bool requestedPaused = false;
    bool runtimeReady = false;
    bool fault = false;
};
struct ControlReport {
    bool appliedPaused = false; // Unknown at boot until the real off operation succeeds.
    bool transitionPending = true;
    bool faultLatched = false;
    const char *error = nullptr;
};

class ControlHardware {
public:
    virtual ~ControlHardware() = default;
    virtual ControlInputs inputs() = 0;
    virtual bool waitIdle() = 0;
    virtual bool waitTxIdle() = 0;
    virtual bool powerOff() = 0;
    virtual bool clearTransport() = 0;
    virtual void retireWork() = 0;
    virtual bool start() = 0;
    virtual bool reconfigure() { return false; }
    virtual const char *startError() { return "ASIC initialization failed; restart required"; }
    virtual void publish(const ControlReport &) = 0;
};

class MiningPowerState {
    OperationGate &m_gate;
    ControlReport m_report;
    bool m_known = false;

    bool finishStop(ControlHardware &hardware, bool epochSafe, bool idle, bool txIdle) {
        const bool off = hardware.powerOff(); // Always attempt safe off, including timeout/fault.
        const bool drained = off && txIdle && hardware.clearTransport();
        if (drained) hardware.retireWork();
        m_report.appliedPaused = off;
        m_report.transitionPending = !off;
        m_known = off;
        if (!epochSafe || !idle || !txIdle || !off || !drained) {
            m_report.faultLatched = true;
            m_report.error = !epochSafe ? "Mining generation exhausted" : !idle ? "ASIC operations did not quiesce" :
                !txIdle ? "ASIC UART transmit did not drain" : !off ? "ASIC power-off/reset failed" : "ASIC UART flush failed";
        }
        hardware.publish(m_report);
        return drained && epochSafe;
    }
    bool stop(ControlHardware &hardware) {
        const bool epochSafe = m_gate.block();
        m_report.transitionPending = true;
        hardware.publish(m_report);
        const bool idle = hardware.waitIdle();
        const bool txIdle = idle && hardware.waitTxIdle();
        return finishStop(hardware, epochSafe, idle, txIdle);
    }
public:
    explicit MiningPowerState(OperationGate &gate) : m_gate(gate) {}
    const ControlReport &report() const { return m_report; }

    bool reconfigure(ControlHardware &hardware) {
        if (!m_known || m_report.appliedPaused || m_report.faultLatched) return false;
        const bool epochSafe = m_gate.block();
        m_report.transitionPending = true;
        hardware.publish(m_report);
        const bool idle = hardware.waitIdle();
        const bool drained = idle && hardware.waitTxIdle();
        const bool flushed = drained && hardware.clearTransport();
        if (!epochSafe || !idle || !drained || !flushed) {
            m_report.faultLatched = true;
            m_report.error = !epochSafe ? "Mining generation exhausted" : !idle ? "ASIC operations did not quiesce" :
                !drained ? "ASIC UART transmit did not drain" : "ASIC UART flush failed";
            finishStop(hardware, epochSafe, idle, drained); // Do not repeat a timed-out quiescence wait.
            return false;
        }
        hardware.retireWork();
        const bool configured = hardware.reconfigure();
        const auto after = hardware.inputs();
        if (!configured || after.requestedPaused || !after.runtimeReady || after.fault) {
            if (after.fault) {
                m_report.faultLatched = true;
                m_report.error = "Hardware protection latched; restart required";
            } else if (!after.requestedPaused && after.runtimeReady) {
                m_report.faultLatched = true;
                m_report.error = hardware.startError();
            }
            stop(hardware);
            return false;
        }
        const bool transportReady = hardware.waitTxIdle() && hardware.clearTransport();
        const auto latest = hardware.inputs();
        if (!transportReady || latest.requestedPaused || !latest.runtimeReady || latest.fault) {
            if (latest.fault) {
                m_report.faultLatched = true;
                m_report.error = "Hardware protection latched; restart required";
            } else if (!transportReady) {
                m_report.faultLatched = true;
                m_report.error = "ASIC reconfiguration transport did not drain";
            }
            stop(hardware);
            return false;
        }
        if (!m_gate.open()) {
            m_report.faultLatched = true;
            m_report.error = "ASIC admission could not reopen";
            stop(hardware);
            return false;
        }
        m_report.transitionPending = false;
        hardware.publish(m_report);
        return true;
    }

    void reconcile(ControlHardware &hardware) {
        const auto input = hardware.inputs();
        if (input.fault) {
            m_report.faultLatched = true;
            m_report.error = "Hardware protection latched; restart required";
            hardware.publish(m_report);
        }
        const bool wantsStop = input.requestedPaused || !input.runtimeReady || input.fault || m_report.faultLatched;
        if (!m_known) {
            if (!stop(hardware)) return;
        }
        if (wantsStop) {
            if (!m_report.appliedPaused) stop(hardware);
            return;
        }
        if (!m_report.appliedPaused) return;
        // Once startup may energize the rail, confirmed ASIC-off no longer
        // applies. Pending must stay true until the complete start succeeds.
        m_report.appliedPaused = false;
        m_report.transitionPending = true;
        hardware.publish(m_report);
        // Admission remains closed throughout the actual initialization.
        const bool started = hardware.start();
        const auto after = hardware.inputs();
        if (!started || after.requestedPaused || !after.runtimeReady || after.fault) {
            if (after.fault) {
                m_report.faultLatched = true;
                m_report.error = "Hardware protection latched; restart required";
            }
            if (!after.requestedPaused && after.runtimeReady && !after.fault) {
                m_report.faultLatched = true;
                m_report.error = hardware.startError();
            }
            stop(hardware);
            return;
        }
        if (!m_gate.open()) {
            m_report.faultLatched = true;
            m_report.error = "ASIC admission could not reopen";
            stop(hardware);
            return;
        }
        m_report.appliedPaused = false;
        m_report.transitionPending = false;
        m_report.error = nullptr;
        hardware.publish(m_report);
    }
};

} // namespace FiveTratumMining
