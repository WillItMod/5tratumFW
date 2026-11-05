#pragma once
#include <ArduinoJson.h>
#include "autotune/core.h"
#include "autotune/clock_policy.h"
#include "autotune/events.h"
#include "autotune/recovery_guard.h"
#include "autotune/display_snapshot.h"
#include "autotune/write_gate.h"
#include "boards/drivers/BuckConverter.h"
#include <thread>
#include <chrono>
#include "tasks/asic_error_telemetry.h"
#include <algorithm>
#include <atomic>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <iostream>
#include <map>
#include <memory>
#include <pthread.h>
#include <string>
#include <vector>

using esp_err_t = int; using nvs_handle_t = int;
constexpr int ESP_OK=0,ESP_FAIL=-1,ESP_ERR_NVS_NOT_FOUND=-2,NVS_READONLY=0,NVS_READWRITE=1,HTTPD_401_UNAUTHORIZED=401;
static uint64_t hostNow=20000000,hostGeneration=1;
static bool storageFail=false,verifyFail=false,voltageFail=false,sensorFail=false,networkAllowed=true,otpAllowed=true,paused=false;
static bool sensorValid=true;
static bool restoreReadbackFail=false;
static bool offSensorFail=false,offGpioGood=true,resumeFail=false,fanRemainsOff=false;
static uint64_t hostPolicyRevision=0;static unsigned recoveryResumeCalls=0;
static bool miningGateOpen=true,counterValid=true;
static float voltageOffset=0,asicDegrees=50,vrmDegrees=55,watts=100,chipErrorWork=1;
static bool weakChip=false; static std::vector<std::string> writes;
static std::map<std::string,std::string> nvstore,staged;
static std::map<int,std::string> handles;
static int nextHandle=1;
static uint64_t storageDelayUs=0;
static bool revokeDuringBackup=false,fanReadFail=false;
static uint16_t fanReportedRpm=1800;
static thread_local bool hostInternalStack=true;
static bool hostReadFailure=false,hostBlobAllocationFail=false,hostWorkerExternal=false;
static std::atomic<unsigned> hostNvsCalls{0};
static unsigned hostSensorReads=0;
static uintptr_t esp_cpu_get_sp(){return hostInternalStack?0x3fc90000U:0x3d090000U;}
static bool esp_ptr_in_dram(const void *p){return reinterpret_cast<uintptr_t>(p)>=0x3fc80000U&&reinterpret_cast<uintptr_t>(p)<0x3fd00000U;}
#include "autotune/storage_context.h"
struct HostStackContext {bool old;explicit HostStackContext(bool internal):old(hostInternalStack){hostInternalStack=internal;}~HostStackContext(){hostInternalStack=old;}};
void *operator new[](size_t size,const std::nothrow_t &) noexcept {
 if(hostBlobAllocationFail)return nullptr;
 try{return ::operator new[](size);}catch(...){return nullptr;}
}
void operator delete[](void *p,const std::nothrow_t &) noexcept {::operator delete[](p);}
static std::atomic_bool configCommitted{false};
uint64_t esp_timer_get_time(){return hostNow;}
int nvs_open(const char *space,int mode,nvs_handle_t *handle){
    assert(hostInternalStack);++hostNvsCalls;if(hostReadFailure)return ESP_FAIL;
    if(storageFail && mode==NVS_READWRITE)return ESP_FAIL;
    bool exists=false;for(auto &item:nvstore)if(item.first.rfind(std::string(space)+"/",0)==0)exists=true;
    if(mode==NVS_READONLY&&!exists)return ESP_ERR_NVS_NOT_FOUND;
    *handle=nextHandle++;handles[*handle]=space;return ESP_OK;
}
void nvs_close(nvs_handle_t handle){assert(hostInternalStack);++hostNvsCalls;handles.erase(handle);}
int nvs_get_blob(nvs_handle_t handle,const char *key,void *out,size_t *length){
    assert(hostInternalStack);++hostNvsCalls;
    const auto full=handles[handle]+"/"+key;const auto found=nvstore.find(full);
    if(found==nvstore.end())return ESP_ERR_NVS_NOT_FOUND;
    if((verifyFail || (restoreReadbackFail&&full=="main/json_cfg")) && out)return ESP_FAIL;
    if(out){if(*length<found->second.size())return ESP_FAIL;memcpy(out,found->second.data(),found->second.size());}
    *length=found->second.size();return ESP_OK;
}
int nvs_set_blob(nvs_handle_t handle,const char *key,const void *data,size_t length){
    assert(hostInternalStack);++hostNvsCalls;
    if(storageFail)return ESP_FAIL;
    staged[handles[handle]+"/"+key]=std::string(static_cast<const char *>(data),length);
    return ESP_OK;
}
namespace FiveTratumAutotune {void externalHandoff(const char *);bool tuningFrequencyAllowed(uint16_t);bool originalFrequencyRestoreAllowed(uint16_t);}
int nvs_commit(nvs_handle_t){assert(hostInternalStack);++hostNvsCalls;if(storageFail)return ESP_FAIL;hostNow+=storageDelayUs;for(auto &entry:staged)nvstore[entry.first]=entry.second;staged.clear();if(revokeDuringBackup)FiveTratumAutotune::externalHandoff("Manual control during backup");return ESP_OK;}
using TaskHandle_t=void *;
static void (*workerFunction)(void *)=nullptr;
static unsigned workerNotifications=0;
static bool workerCreateFail=false;
static unsigned workerRequestedBytes=0,workerFreeBytes=7000;
static bool lowAfterConfigCommit=false;
unsigned uxTaskGetStackHighWaterMark(TaskHandle_t){return workerFreeBytes;}
constexpr int pdTRUE=1,pdPASS=1;constexpr uint32_t portMAX_DELAY=0xffffffff;
struct WorkerSleeping {};
int xTaskCreate(void (*function)(void *),const char *,unsigned bytes,void *,unsigned,TaskHandle_t *handle){workerRequestedBytes=bytes;if(workerCreateFail)return 0;workerFunction=function;*handle=reinterpret_cast<void *>(1);return pdPASS;}
void xTaskNotifyGive(TaskHandle_t){++workerNotifications;}
uint32_t ulTaskNotifyTake(int,uint32_t){if(!workerNotifications)throw WorkerSleeping{};workerNotifications=0;return 1;}
static std::atomic_bool wireEntered{false},wireRelease{false};
static std::atomic<unsigned> physicalWrites{0};
static bool pausePolarityWire=false,polarityFail=false;
static unsigned polarityWrites=0;
static bool EMC2302_set_fan_polarity(bool){
 if(pausePolarityWire){wireEntered=true;while(!wireRelease.load())std::this_thread::yield();}
 ++physicalWrites;++polarityWrites;return !polarityFail;
}
namespace FiveTratumAutotune {struct FirmwareUpdateGuard {FirmwareUpdateGuard();~FirmwareUpdateGuard();};}
class TPS53647 {
public:
 bool readTuningTelemetry(BuckTuningTelemetry &){return true;}
 bool set_vout(float){wireEntered=true;while(!wireRelease.load())std::this_thread::yield();++physicalWrites;return true;}
 bool setTuningVoltage(float,bool (*)());
};
enum packet_type_t { JOB_PACKET=0,CMD_PACKET=1 };
constexpr uint8_t TYPE_JOB=0x20;
static uint16_t crc16_false(const unsigned char *,unsigned){return 0;}
static uint8_t crc5(const unsigned char *,unsigned){return 0;}
static int SERIAL_send(const unsigned char *,unsigned size){wireEntered=true;while(!wireRelease.load())std::this_thread::yield();++physicalWrites;return size;}
class Asic {
public:
 bool (*m_recoveryGuard)(void *)=nullptr;
 void *m_recoveryContext=nullptr;
 std::atomic_bool m_transportFailed{false};
 void send(uint8_t,uint8_t *,uint8_t);
 bool recoveryDelay(uint32_t);
};
#define pdMS_TO_TICKS(ms) (ms)
static void vTaskDelay(uint32_t milliseconds){hostNow+=static_cast<uint64_t>(milliseconds)*1000;}
struct PThreadGuard { pthread_mutex_t &mutex; explicit PThreadGuard(pthread_mutex_t &m):mutex(m){pthread_mutex_lock(&mutex);} ~PThreadGuard(){pthread_mutex_unlock(&mutex);} };
namespace Config {
static JsonDocument s_doc;
static pthread_mutex_t s_mutex=PTHREAD_MUTEX_INITIALIZER;
static bool s_dirty=false;
static std::atomic<uint64_t> s_tuningRevision{0};
bool cfgApplyPatchPersisted(JsonObjectConst);
bool captureTuningSettings(JsonObject);bool validTuningSettings(JsonObjectConst);bool restoreTuningSettings(JsonObjectConst);
uint64_t tuningMutationRevision();void cfgSetU16(const char *,uint16_t);
bool write_document_to_nvs(const JsonDocument &doc){
    assert(hostInternalStack);++hostNvsCalls;
    if(storageFail)return false;
    std::string out;serializeJson(doc,out);nvstore["main/json_cfg"]=out;configCommitted=true;if(lowAfterConfigCommit)workerFreeBytes=512;return true;
}
uint16_t cfgGetU16(const char *key,uint16_t fallback=0){return s_doc[key].isNull()?fallback:s_doc[key].as<uint16_t>();}
bool isCanEnabled(){return false;}
uint16_t getFanManualSpeed(int ch){return cfgGetU16(ch?"fan1speed":"fanspeed",70);}
bool isFanPidUseMax(){return cfgGetU16("fan_pid_max",1)!=0;}
}
struct PidSettings {uint16_t targetTemp=55,p=600,i=10,d=1000;};
struct TuningSensors {float asicTempC=0,vrmTempC=0,powerW=0,vin=0,coreVoltageMv=0,outputCurrentA=0;int64_t capturedAtUs=0;bool measuredInputPower=false;};
class Board {
public:
 enum Error {NONE,VOUT_FAULT,TEMP_FAULT};
 int count=4,frequency=600,voltage=1150,standardFrequency=850;bool fanPolarity=false;PidSettings pid[2];
 const char *getDeviceModel(){return count==4?"NerdQAxe++":"NerdOCTAXE-γ";}
 const char *tuningAsicSensorLabel() const{return "Shared calibrated TMP1075";}
 const char *tuningVrmSensorLabel(){return "Hottest VRM sensor";}
 const char *getAsicModel(){return "BM1370";}
 int getAsicCount(){return count;}bool isCanSlave(){return false;}bool isInitialized(){return !paused;}bool isBuckInitialized(){return !paused;}bool isShutdown(){return false;}
 int getAbsMaxAsicFrequency(){return standardFrequency;}int getAbsMinAsicVoltageMillis(){return 1005;}int getAbsMaxAsicVoltageMillis(){return 1400;}
 int getAsicFrequency(){return frequency;}int getAsicVoltageMillis(){return voltage;}float getMaxPin(){return 300;}float getMinVin(){return 11;}float getMaxVin(){return 13;}
 struct Asics {int getSmallCoreCount(){return 2040;}} asics;
 Asics *getAsics(){return &asics;}
 int getNumFans(){return 2;}int getFanMode(int ch){return Config::cfgGetU16(ch?"fan1mode":"autofanspeed",2);}
 bool isInvertFanPolarityEnabled(){return fanPolarity;}void setFanPolarity(bool value){fanPolarity=value;}
 bool restoreTuningFanPolarity(bool,bool (*)());
 bool readTuningFan(int,uint16_t &rpm){rpm=fanReadFail?0:fanReportedRpm;return !fanReadFail;}
 bool refreshTuningTemperatures(){return true;}
 PidSettings *getPidSettings(int ch){return &pid[ch];}
 bool restoreOriginalTuningFrequency(uint16_t);
 void loadSettings(){frequency=std::min<int>(Config::cfgGetU16("asicfrequency",600),standardFrequency);voltage=Config::cfgGetU16("asicvoltage",1150);fanPolarity=Config::cfgGetU16("pwmfanpol",0);for(int i=0;i<2;++i){pid[i].targetTemp=Config::cfgGetU16(i?"fan1_pid_temp":"pid_temp",55);}}
 bool readTuningSensors(TuningSensors &);
 bool isRecoveryPowerOff(){return paused&&offGpioGood;}
 bool readRecoverySensors(TuningSensors &out){++hostSensorReads;out={};if(offSensorFail)return false;out.asicTempC=asicDegrees;out.vrmTempC=vrmDegrees;out.vin=12;out.capturedAtUs=hostNow-1000;return true;}
};
static Board hostBoard;
struct FanController {
 uint16_t getOverheatTemp(int ch){return Config::cfgGetU16(ch?"fan1_overheat":"overheat_temp",ch?105:75);}
 void loadSettings(){}
};
namespace FiveTratumMining {bool applyTuningFrequency(uint16_t);}
class PowerManagementTask {
public:
 uint16_t appliedFrequency=600,appliedVoltage=1150;FanController fan;
 Board *m_board=&hostBoard;bool m_shutdown=false;uint16_t &m_appliedAsicFrequency=appliedFrequency;
 bool refreshProtectionForMining(){return true;}
 uint16_t getAppliedTuningFrequency(){return appliedFrequency;}uint16_t getAppliedTuningVoltage(){return appliedVoltage;}
 bool isShutdown(){return false;}uint16_t getFanRPM(int){return 1800;}FanController &getFanController(){return fan;}
 void lock(){}void unlock(){}
 bool commandTuningFrequency(uint16_t);
 bool commandTuningVoltage(uint16_t value){writes.push_back("voltage:"+std::to_string(value));if(voltageFail)return false;appliedVoltage=value;return !paused;}
};
static PowerManagementTask POWER_MANAGEMENT_MODULE;
bool Board::readTuningSensors(TuningSensors &out){
 ++hostSensorReads;
 out={};if(sensorFail||!sensorValid)return false;
 out={asicDegrees,vrmDegrees,watts,12,static_cast<float>(POWER_MANAGEMENT_MODULE.appliedVoltage)+voltageOffset,60,static_cast<int64_t>(hostNow-1000),true};return true;
}
struct HostSystem {Board::Error error=Board::NONE;Board *getBoard(){return &hostBoard;}Board::Error getBoardError(){return error;}void setBoardError(Board::Error err,uint32_t){error=err;}};
static HostSystem SYSTEM_MODULE;
struct Stratum {bool isAnyConnected(){return true;}};
static Stratum manager;static Stratum *STRATUM_MANAGER=&manager;
namespace FiveTratumMining {
bool applyTuningFrequency(uint16_t value){if(!FiveTratumAutotune::tuningFrequencyAllowed(value))return false;writes.push_back("clock:"+std::to_string(value));POWER_MANAGEMENT_MODULE.appliedFrequency=value;++hostGeneration;return !paused;}
bool controlSupported(){return true;}bool miningWritesAllowed(){return !paused&&miningGateOpen;}uint64_t workGeneration(){return hostGeneration;}
bool manualOverride(bool value){++hostPolicyRevision;paused=value;return true;}
bool requestedPaused(){return paused;}
uint64_t policyRevision(){return hostPolicyRevision;}
bool resumeForRecovery(uint64_t expected,uint64_t &applied){if(expected!=hostPolicyRevision||!paused)return false;paused=false;applied=++hostPolicyRevision;return true;}
void updateControl(){if(FiveTratumAutotune::preserveRecoveryFaults()){++recoveryResumeCalls;if(resumeFail){paused=true;SYSTEM_MODULE.error=Board::VOUT_FAULT;}else{POWER_MANAGEMENT_MODULE.appliedFrequency=hostBoard.frequency;POWER_MANAGEMENT_MODULE.appliedVoltage=hostBoard.voltage;fanReadFail=fanRemainsOff;}}}
unsigned expectedStandaloneChain(const char *model,const char *asic){return strcmp(asic,"BM1370")?0:!strcmp(model,"NerdQAxe++")?4:!strcmp(model,"NerdOCTAXE-γ")?8:0;}
}
bool readFiveTratumPhysicalDeviceId(char *out){strcpy(out,"5tratum-0123456789abcdef");return true;}
struct HashMonitor {
 FiveTratumTelemetry::AsicErrorTelemetry getAsicErrorTelemetry(uint64_t now,bool){
  FiveTratumTelemetry::AsicErrorTelemetry out;out.asicCount=hostBoard.count;
  if(!sensorValid||!counterValid)return out;
  const auto stamp=static_cast<int64_t>(now/5000000*5000000);
  for(int i=0;i<out.asicCount;++i){out.chips[i]=FiveTratumTelemetry::chipErrorTelemetry({weakChip&&i==0?0.859f:1000.0f,stamp,true},{chipErrorWork,stamp,true},now,true);}
  out.errorPercentage=100.0*chipErrorWork/(1000+chipErrorWork);out.errorPercentageAvailable=true;return out;
 }
 bool copyTuningErrorTelemetry(FiveTratumTelemetry::TuningErrorTelemetry &out,int expected,uint64_t now,bool active){
  out={};if(hostBoard.count!=expected||(expected!=4&&expected!=8))return false;
  const auto all=getAsicErrorTelemetry(now,active);out.asicCount=all.asicCount;out.errorPercentage=all.errorPercentage;out.errorPercentageAvailable=all.errorPercentageAvailable;
  for(int i=0;i<all.asicCount;++i)out.chips[i]=all.chips[i];
  return true;
 }
};
static HashMonitor HASHRATE_MONITOR;
struct httpd_req_t {unsigned content_len=0;std::string body,response,status;};
static int http_server=0;
struct ConGuard {ConGuard(int,httpd_req_t *){}};
template<class T>struct LockGuard {T &object;explicit LockGuard(T &value):object(value){object.lock();}~LockGuard(){object.unlock();}};
int is_network_allowed(httpd_req_t *){return networkAllowed?ESP_OK:ESP_FAIL;}
int set_cors_headers(httpd_req_t *){return ESP_OK;}int validateOTP(httpd_req_t *){return otpAllowed?ESP_OK:ESP_FAIL;}
void httpd_resp_set_status(httpd_req_t *req,const char *status){req->status=status;}
void httpd_resp_set_type(httpd_req_t *,const char *){}void httpd_resp_set_hdr(httpd_req_t *,const char *,const char *){}
int httpd_resp_send_err(httpd_req_t *,int,const char *){return ESP_FAIL;}void httpd_resp_send_500(httpd_req_t *){}
int getJsonData(httpd_req_t *req,JsonDocument &doc){return deserializeJson(doc,req->body)?ESP_FAIL:ESP_OK;}
int sendJsonResponse(httpd_req_t *req,JsonDocument &doc){req->response.clear();serializeJson(doc,req->response);return ESP_OK;}
