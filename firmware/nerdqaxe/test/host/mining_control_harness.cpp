#include "mining_control_state.h"
#include <cassert>
#include <cstring>
#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <atomic>

using namespace FiveTratumMining;
struct Hardware final : ControlHardware {
    OperationGate &gate;
    ControlInputs input;
    bool idleOkay = true, txOkay = true, offOkay = true, flushOkay = true, startOkay = true;
    bool pauseDuringStart = false, faultDuringStart = false;
    bool configureOkay = true, pauseDuringConfigure = false, faultDuringConfigure = false;
    unsigned offCalls = 0, starts = 0, retired = 0, waits = 0, txWaits = 0, pauseAtTxWait = 0;
    std::vector<ControlReport> reports;
    explicit Hardware(OperationGate &gate) : gate(gate) {}
    ControlInputs inputs() override { return input; }
    bool waitIdle() override {
        ++waits;
        if (!idleOkay) return false;
        for (int i = 0; i < 100 && !gate.idle(); ++i) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        return gate.idle();
    }
    bool waitTxIdle() override {
        if (++txWaits == pauseAtTxWait) input.requestedPaused = true;
        return txOkay;
    }
    bool powerOff() override { ++offCalls; return offOkay; }
    bool clearTransport() override { return flushOkay; }
    void retireWork() override { assert(gate.idle() && !gate.isOpen()); ++retired; }
    bool start() override {
        assert(!gate.isOpen() && gate.idle());
        ++starts;
        if (pauseDuringStart) { input.requestedPaused = true; return false; }
        if (faultDuringStart) { input.fault = true; return false; }
        return startOkay;
    }
    bool reconfigure() override {
        assert(!gate.isOpen() && gate.idle());
        if (pauseDuringConfigure) { input.requestedPaused = true; return false; }
        if (faultDuringConfigure) { input.fault = true; return false; }
        return configureOkay;
    }
    void publish(const ControlReport &report) override { reports.push_back(report); }
};

int main() {
    // Unknown boot never claims ASIC off; disabled/default policy can start
    // only after the actual off/drain sequence and runtime readiness.
    {
        OperationGate gate; gate.block(); MiningPowerState state(gate); Hardware hardware(gate);
        assert(!state.report().appliedPaused && state.report().transitionPending);
        state.reconcile(hardware);
        assert(state.report().appliedPaused && !gate.isOpen() && hardware.starts == 0);
        hardware.input.runtimeReady = true; state.reconcile(hardware);
        assert(!state.report().appliedPaused && !state.report().transitionPending && gate.isOpen());
        assert(hardware.offCalls == 1 && hardware.starts == 1 && hardware.retired == 1);
        assert(hardware.reports[2].transitionPending && !hardware.reports[2].appliedPaused);
        OperationLease result(gate); const auto oldGeneration = result.generation();
        assert(result.isCurrent());
        gate.block(); assert(!result.isCurrent() && gate.generation() > oldGeneration);
        assert(!gate.open()); // A copied result still owns its complete transaction.
    }
    // A pending RX/share lease must finish before retirement. After reopening,
    // its old generation cannot authenticate a result against new work.
    {
        OperationGate gate; MiningPowerState state(gate); Hardware hardware(gate);
        hardware.input.runtimeReady = true; state.reconcile(hardware);
        uint64_t copiedGeneration = 0;
        std::atomic_bool entered{false};
        std::thread result([&] { OperationLease lease(gate); copiedGeneration = lease.generation();
            entered.store(true);
            std::this_thread::sleep_for(std::chrono::milliseconds(10)); });
        while (!entered.load()) std::this_thread::yield();
        hardware.input.requestedPaused = true; state.reconcile(hardware); result.join();
        assert(state.report().appliedPaused && !state.report().faultLatched && gate.idle());
        hardware.input.requestedPaused = false; state.reconcile(hardware);
        assert(gate.isOpen() && !gate.current(copiedGeneration));
    }
    for (int failure = 0; failure < 5; ++failure) {
        OperationGate gate; gate.block(); MiningPowerState state(gate); Hardware hardware(gate);
        hardware.input.runtimeReady = true;
        if (failure == 0) hardware.idleOkay = false;
        if (failure == 1) hardware.txOkay = false;
        if (failure == 2) hardware.offOkay = false;
        if (failure == 3) hardware.flushOkay = false;
        if (failure == 4) hardware.startOkay = false; // Regulator/count/readback failures map here.
        state.reconcile(hardware);
        assert(state.report().faultLatched && state.report().error && !gate.isOpen());
        assert(state.report().appliedPaused == hardware.offOkay);
        const auto starts = hardware.starts;
        hardware.idleOkay = hardware.txOkay = hardware.offOkay = hardware.flushOkay = hardware.startOkay = true;
        hardware.input.requestedPaused = false; state.reconcile(hardware);
        assert(hardware.starts == starts && state.report().faultLatched && !gate.isOpen());
    }
    // A new manual/scheduled pause during the cooperative ramp is cancellation,
    // not permission to open work nor a fault that prevents a later safe resume.
    {
        OperationGate gate; gate.block(); MiningPowerState state(gate); Hardware hardware(gate);
        hardware.input.runtimeReady = true; hardware.pauseDuringStart = true;
        state.reconcile(hardware);
        assert(state.report().appliedPaused && !state.report().faultLatched && !gate.isOpen());
        hardware.pauseDuringStart = false; hardware.input.requestedPaused = false;
        state.reconcile(hardware); assert(gate.isOpen());
        hardware.input.fault = true; state.reconcile(hardware);
        hardware.input.fault = false; state.reconcile(hardware);
        assert(state.report().faultLatched && !gate.isOpen());
    }
    {
        OperationGate gate; gate.block(); MiningPowerState state(gate); Hardware hardware(gate);
        hardware.input.runtimeReady = true; hardware.faultDuringStart = true;
        state.reconcile(hardware);
        assert(state.report().faultLatched && state.report().appliedPaused && !gate.isOpen());
    }
    for (int outcome = 0; outcome < 6; ++outcome) {
        OperationGate gate; gate.block(); MiningPowerState state(gate); Hardware hardware(gate);
        hardware.input.runtimeReady = true; state.reconcile(hardware);
        const auto oldGeneration = gate.generation();
        const auto waitsBefore = hardware.waits;
        if (outcome == 1) hardware.configureOkay = false;
        if (outcome == 2) hardware.pauseDuringConfigure = true;
        if (outcome == 3) hardware.faultDuringConfigure = true;
        if (outcome == 4) hardware.pauseAtTxWait = hardware.txWaits + 2; // Pause during final TX drain.
        if (outcome == 5) hardware.idleOkay = false;
        const bool configured = state.reconfigure(hardware);
        assert(configured == (outcome == 0) && gate.generation() > oldGeneration);
        assert(gate.isOpen() == configured);
        if (outcome == 0) assert(!state.report().appliedPaused && !state.report().transitionPending);
        else assert(state.report().appliedPaused && !state.report().transitionPending);
        assert(state.report().faultLatched == (outcome == 1 || outcome == 3 || outcome == 5));
        if (outcome == 5) assert(hardware.waits == waitsBefore + 1); // Failed quiescence goes straight to off.
        if (outcome == 2 || outcome == 4) {
            hardware.pauseDuringConfigure = false; hardware.input.requestedPaused = false;
            state.reconcile(hardware); assert(gate.isOpen());
        }
    }
    std::cout << "Production mining control: boot/readiness, complete-operation drain, generation retirement, "
                 "timeouts, hardware failures, cancellation and non-clearable protection pass\n";
}
