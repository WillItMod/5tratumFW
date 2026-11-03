#pragma once

#include <stdint.h>

struct BuckTuningTelemetry {
    float vin = 0, vout = 0, iout = 0, pin = 0, temperatureC = 0;
    bool measuredInputPower = false;
};

class BuckConverter {
  public:
    virtual ~BuckConverter() = default;

    virtual bool init(int num_phases, int imax, float ifault) = 0;
    virtual void clear_faults() = 0;

    virtual float get_temperature() = 0;
    virtual float get_pin() = 0;
    virtual float get_pout() = 0;
    virtual float get_vin() = 0;
    virtual float get_iin() = 0;
    virtual float get_iout() = 0;
    virtual float get_vout() = 0;
    virtual bool uses_external_vr_temperature() { return true; }

    // Strict, current acquisitions only. Default denies unreviewed converters.
    virtual bool readTuningTelemetry(BuckTuningTelemetry &) { return false; }
    // Recovery preflight: fresh input/temperature/status only; output may be off.
    virtual bool readRecoveryTelemetry(BuckTuningTelemetry &) { return false; }
    // Tuning must never clear faults, enable a stopped rail or change limits.
    virtual bool setTuningVoltage(float, bool (*)()) { return false; }
    virtual bool set_vout(float volts) = 0;
    virtual bool disable_vout() { return true; }

    virtual void status() = 0;

    virtual uint8_t get_status_byte() = 0;
    virtual uint8_t get_status_iout() = 0;
    virtual uint8_t get_status_vout() = 0;
    virtual uint8_t get_status_input() = 0;
    virtual uint8_t get_status_temp() = 0;
};
