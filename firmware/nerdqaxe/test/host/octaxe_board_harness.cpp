#include "octaxe_board_support.h"
#include "nerdoctaxegamma.h"
#include "stratum/native_pool_stream.h"

#include <cassert>
#include <cstring>
#include <iostream>
#include <limits>

class Probe final : public NerdOctaxeGamma {
public:
    const char *model() const { return m_deviceModel; }
    const char *agent() const { return m_miningAgent; }
    int count() const { return m_asicCount; }
    float maximumPower() const { return m_maxPin; }
    float minimumPower() const { return m_minPin; }
    int phases() const { return m_numPhases; }
    int frequency() const { return m_asicFrequency; }
    int voltage() const { return m_asicVoltageMillis; }
    int versionRolling() const { return m_vrFrequency; }
    const char *regulator() const { return m_tps->kind(); }
    void shutdown(bool value) { m_shutdown = value; }
    void noAsic() { m_asics = nullptr; }
    void seed(float value) { for (int i = 0; i < count(); ++i) setChipTemp(i, value); }
};

static void allZero(const Probe &board) {
    for (int i = 0; i < board.count(); ++i) assert(board.getChipTemp(i) == 0.0f);
}

int main(int argc, char **argv) {
    assert(argc == 2);
    const std::string scenario = argv[1];
    mockMuxInit = {{ESP_FAIL, ESP_FAIL}};
    if (scenario == "six-phase") mockStrap = 1;
    if (scenario == "mux" || scenario == "invalid-mux" || scenario == "shutdown" ||
        scenario == "reinit-no-mux" || scenario == "reinit-failure") mockMuxInit = {{ESP_OK, ESP_OK}};
    if (scenario == "partial-mux") mockMuxInit = {{ESP_FAIL, ESP_OK}};
    if (scenario == "parent-failure") mockParentInit = false;
    {
        Probe board;
        assert(board.count() == 8);
        assert(!strcmp(board.model(), "NerdOCTAXE-\xCE\xB3"));
        assert(!strcmp(board.agent(), "NerdOCTAXE-Gamma"));
        assert(mockOperations == std::vector<std::string>({"reset-3", "input-3", "pull-down-3", "read-3"}));
        assert(mockNowUs == 1000); // existing strap settle, no new hardware wait
        assert(mockUartTemperatureRequests == 0);
        if (scenario == "six-phase") {
            assert(board.phases() == 6 && !strcmp(board.regulator(), "TPS53667"));
            assert(board.maximumPower() == 300 && board.minimumPower() == 30);
            assert(board.getVRTemp() == 62.5f);
            assert(mockTpsConstructed == 2 && mockTpsDestroyed == 1);
        } else {
            assert(board.phases() == 4 && !strcmp(board.regulator(), "TPS53647"));
            assert(board.maximumPower() == 200 && board.minimumPower() == 100);
            assert(board.getVRTemp() == 54.5f);
        }
        // Actual production Board::loadSettings, external config reads only.
        board.loadSettings();
        assert(board.frequency() == 700 && board.voltage() == 1180 && board.versionRolling() == 25011);
        char agent0[FiveTratumNativePool::AgentBufferBytes], agent1[FiveTratumNativePool::AgentBufferBytes];
        const char *id = "5tfw:0102030405060708090a0b0c0d0e0f10";
        assert(FiveTratumNativePool::buildAgent(agent0, sizeof(agent0), board.agent(), "BM1370", "5tratumFW-oct-test", id, 0));
        assert(FiveTratumNativePool::buildAgent(agent1, sizeof(agent1), board.agent(), "BM1370", "5tratumFW-oct-test", id, 1));
        assert(strstr(agent0, ":0/work-context-v2") && strstr(agent1, ":1/work-context-v2"));
        assert(strcmp(agent0, agent1) && strlen(agent0) <= 192 && strlen(agent1) <= 192);
        char rejected[193];
        assert(!FiveTratumNativePool::buildAgent(rejected, sizeof(rejected), board.model(), "BM1370", "fixture", id, 0));

        board.seed(75.0f);
        const bool initialized = board.initBoard();
        assert(initialized == mockParentInit);
        allZero(board);
        if (scenario == "parent-failure") {
            assert(mockMuxProbeCalls[0] == 0 && mockMuxProbeCalls[1] == 0);
            assert(mockUartTemperatureRequests == 0);
        } else if (scenario == "fallback" || scenario == "missing-asic") {
            if (scenario == "missing-asic") board.noAsic();
            board.requestChipTemps();
            assert(mockUartTemperatureRequests == 0);
            mockNowUs += 15000000;
            board.requestChipTemps();
            assert(mockUartTemperatureRequests == (scenario == "fallback" ? 1 : 0));
            assert(mockMuxReadCalls[0] == 0 && mockMuxReadCalls[1] == 0);
            board.requestChipTemps();
            assert(mockUartTemperatureRequests == (scenario == "fallback" ? 1 : 0));
        } else if (scenario == "mux" || scenario == "invalid-mux") {
            mockMuxValues = {{{{50, 51, 52, 53}}, {{54, 55, 56, 57}}}};
            board.requestChipTemps();
            for (int i = 0; i < 8; ++i) assert(board.getChipTemp(i) == 50 + i);
            assert(mockMuxReadCalls[0] == 4 && mockMuxReadCalls[1] == 4);
            assert(mockUartTemperatureRequests == 0);
            if (scenario == "invalid-mux") {
                mockMuxValues = {{{{NAN, INFINITY, -1, 0}}, {{160, 58, NAN, 59}}}};
                board.requestChipTemps();
                for (int i = 0; i < 4; ++i) assert(board.getChipTemp(i) == 0);
                assert(board.getChipTemp(4) == 160); // never hide a finite hot reading
                assert(board.getChipTemp(5) == 58 && board.getChipTemp(6) == 0 && board.getChipTemp(7) == 59);
            }
        } else if (scenario == "partial-mux") {
            mockMuxValues[1] = {{54, 55, 56, 57}};
            board.seed(75);
            board.requestChipTemps();
            for (int i = 0; i < 4; ++i) assert(board.getChipTemp(i) == 0);
            for (int i = 4; i < 8; ++i) assert(board.getChipTemp(i) == 50 + i);
            assert(mockMuxReadCalls[0] == 0 && mockMuxReadCalls[1] == 4 && mockUartTemperatureRequests == 0);
        } else if (scenario == "shutdown") {
            board.seed(75); board.shutdown(true); board.requestChipTemps(); allZero(board);
            assert(mockMuxReadCalls[0] == 0 && mockMuxReadCalls[1] == 0 && mockUartTemperatureRequests == 0);
        } else if (scenario == "reinit-no-mux" || scenario == "reinit-failure") {
            mockMuxValues = {{{{50, 51, 52, 53}}, {{54, 55, 56, 57}}}};
            board.requestChipTemps();
            assert(board.getChipTemp(0) == 50 && board.getChipTemp(7) == 57);
            mockMuxReadCalls = {{0, 0}};
            mockMuxProbeCalls = {{0, 0}};
            mockMuxInit = {{ESP_FAIL, ESP_FAIL}};
            mockParentInit = scenario != "reinit-failure";
            assert(board.initBoard() == mockParentInit);
            allZero(board);
            if (!mockParentInit) assert(mockMuxProbeCalls[0] == 0 && mockMuxProbeCalls[1] == 0);
            board.requestChipTemps(); // establish the base method's periodic baseline
            assert(mockUartTemperatureRequests == 0);
            mockNowUs += 15000000;
            board.requestChipTemps();
            assert(mockMuxReadCalls[0] == 0 && mockMuxReadCalls[1] == 0);
            assert(mockUartTemperatureRequests == 1);
            allZero(board);
        }
        std::cout << "{\"passed\":true,\"count\":8,\"agent0\":\"" << agent0 << "\",\"agent1\":\"" << agent1 << "\"}";
    }
    assert(mockTpsConstructed == mockTpsDestroyed);
}
