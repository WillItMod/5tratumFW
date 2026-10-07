#pragma once
#include "mining_control_state.h"
#include <ArduinoJson.h>

class Board;

namespace FiveTratumMining {
bool initializeControl(Board *board, bool canSlave);
bool controlSupported();
void setRuntimeReady(bool ready);
void notifyJobWorkerReady(bool ready);
void updateControl(); // Power-task owner, while holding its recursive hardware mutex.
bool writeControlReport(JsonDocument &report);
bool miningWritesAllowed();
bool applyFrequency(uint16_t frequency); // Qualified power-task owner, hardware mutex held.
uint64_t workGeneration();
OperationGate &operationGate();
using MiningOperation = OperationLease;
}
