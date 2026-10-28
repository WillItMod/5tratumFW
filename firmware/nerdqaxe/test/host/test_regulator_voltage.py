#!/usr/bin/env python3
"""Actual TPS53647/667 voltage methods with a mocked PMBus boundary.

No I2C adapter, GPIO, miner or network access. The constructors, VID encoder,
write framing, voltage method and device-code helper are production bodies.
This checks error propagation and packet construction, not physical VCORE.
"""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


def method(source, signature):
    start = source.index(signature)
    brace = source.index("{", start)
    depth, end = 1, brace + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end] + "\n"


class RegulatorVoltageTests(unittest.TestCase):
    def test_real_write_error_blocks_success_for_both_regulators(self):
        base = (ROOT / "main/boards/drivers/TPS53647.cpp").read_text()
        six = (ROOT / "main/boards/drivers/TPS53667.cpp").read_text()
        bodies = "\n".join(method(base, signature) for signature in (
            "TPS53647::TPS53647()", "uint8_t TPS53647::volt_to_vid(",
            "esp_err_t TPS53647::write_word(", "bool TPS53647::set_vout(",
            "uint16_t TPS53647::get_device_code("))
        bodies += method(six, "TPS53667::TPS53667()")
        harness = r'''
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <vector>
#include "pmbus_commands.h"
using esp_err_t = int;
using i2c_cmd_handle_t = int;
constexpr int ESP_OK = 0, ESP_FAIL = -1, I2C_MASTER_NUM = 0, SMBUS_DEFAULT_TIMEOUT = 1000;
constexpr uint8_t WRITE_BIT = 0;
constexpr bool ACK_CHECK = true, NACK_VALUE = true;
static const char *TAG = "regulator-fixture";
#define ESP_LOGI(tag, ...) ((void)(tag))
#define ESP_LOGE(tag, ...) ((void)(tag))
#define ESP_LOGD(tag, ...) ((void)(tag))
static int busResult = ESP_OK, readsResult = ESP_OK, executions = 0, creations = 0, deletions = 0;
static uint16_t readValue = 0x01f0;
static std::vector<uint8_t> bytes;
i2c_cmd_handle_t i2c_cmd_link_create() { ++creations; bytes.clear(); return 1; }
void i2c_master_start(i2c_cmd_handle_t) {}
void i2c_master_write_byte(i2c_cmd_handle_t, uint8_t byte, bool) { bytes.push_back(byte); }
void i2c_master_stop(i2c_cmd_handle_t) {}
esp_err_t i2c_master_cmd_begin(int, i2c_cmd_handle_t, int) { ++executions; return busResult; }
void i2c_cmd_link_delete(i2c_cmd_handle_t) { ++deletions; }
class TPS53647 {
protected:
    uint8_t m_i2cAddr = 0;
    float m_hwMinVoltage = 0, m_initVOutMin = 0, m_initVOutMax = 0;
    uint8_t m_initOnOffConfig = 0, m_initOtWarnLimit = 0, m_initOtFaultLimit = 0;
    uint8_t volt_to_vid(float);
    esp_err_t write_word(uint8_t, uint16_t);
    esp_err_t read_word(uint8_t command, uint16_t *out) {
        assert(command == PMBUS_MFR_SPECIFIC_44); *out = readValue; return readsResult;
    }
public:
    TPS53647();
    bool set_vout(float);
    uint16_t get_device_code();
};
class TPS53667 : public TPS53647 { public: TPS53667(); };
'''
        harness += bodies + r'''
int main() {
    TPS53647 four;
    TPS53667 six;
    for (TPS53647 *regulator : {&four, static_cast<TPS53647 *>(&six)}) {
        for (float voltage : {-1.0f, 0.79f, 1.41f, std::numeric_limits<float>::quiet_NaN(),
                              std::numeric_limits<float>::infinity()}) {
            const int before = executions;
            assert(!regulator->set_vout(voltage) && executions == before);
        }
        const int before = executions;
        assert(regulator->set_vout(0) && executions == before); // Existing zero behavior is preserved.
        for (float voltage : {0.8f, 1.0f, 1.13f, 1.4f}) {
            busResult = ESP_OK;
            assert(regulator->set_vout(voltage));
            assert(bytes.size() == 4 && bytes[0] == 0xe2 && bytes[1] == PMBUS_VOUT_COMMAND && bytes[3] == 0);
            assert(bytes[2] != 0x97); // Retain factory-reset detection's reserved VID.
            busResult = ESP_FAIL;
            assert(!regulator->set_vout(voltage)); // No false success on I2C failure.
            assert(creations == deletions && creations == executions);
            busResult = ESP_OK;
            assert(regulator->set_vout(voltage)); // Retry succeeds without a false cached acknowledgement.
        }
        readsResult = ESP_OK; assert(regulator->get_device_code() == readValue);
        readsResult = ESP_FAIL; assert(regulator->get_device_code() == 0);
    }
    puts("Production TPS53647/667 voltage framing, finite/range validation, reserved VID and I2C failure propagation passed");
}
'''
        with tempfile.TemporaryDirectory(prefix="5tfw-regulator-") as temporary:
            source = Path(temporary) / "regulator.cpp"
            binary = Path(temporary) / "regulator"
            source.write_text(harness)
            subprocess.run([shutil.which("clang++") or shutil.which("g++"), "-std=c++17", "-Wall", "-Wextra", "-Werror",
                            "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
                            "-I", str(ROOT / "main/boards/drivers"), str(source), "-o", str(binary)], check=True)
            subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    unittest.main(verbosity=2)
