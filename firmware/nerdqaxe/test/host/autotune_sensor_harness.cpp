#include "autotune/write_gate.h"
#include "drivers/BuckConverter.h"
#include "tuning_telemetry.h"
#include "tuning_fan.h"
#include "pmbus_commands.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>
#include <math.h>
#include <cstdint>
#include <map>
#include <pthread.h>
#include <iostream>
#include <vector>
using esp_err_t=int;constexpr int ESP_OK=0,ESP_FAIL=-1,ESP_ERR_INVALID_ARG=-3;
static std::map<uint8_t,uint8_t> fanRegisters;
static std::map<uint8_t,unsigned> fanReadCounts;
static int fanFailed=-1;
static esp_err_t i2c_master_register_read(uint8_t,uint8_t reg,uint8_t *out,size_t count){
 assert(count==1);++fanReadCounts[reg];if(fanFailed==reg)return ESP_FAIL;*out=fanRegisters[reg];
 // Model the actual read-to-clear fault registers, not ordinary RAM.
 if(reg==0x25 || reg==0x26 || reg==0x27)fanRegisters[reg]=0;
 return ESP_OK;
}
#define ESP_LOGI(...) do {} while(0)
#define ESP_LOGE(...) do {} while(0)
static int muxFailedPin=-1;
static std::map<int,int> gpioLevels;
static constexpr int BM1368_RST_PIN=10,TPS53647_EN_PIN=11,LDO_EN_PIN=13;
static int gpio_get_level(int pin){return gpioLevels[pin];}
static esp_err_t gpio_set_level(int pin,int){return pin==muxFailedPin?ESP_FAIL:ESP_OK;}
#define ESP_RETURN_ON_ERROR(expression,...) do {const int result=(expression);if(result!=ESP_OK)return result;} while(0)
class Tmp451Mux {public:int m_mux_a0=2,m_mux_a1=12;bool m_mux_active_high=true;esp_err_t select_channel(int);};
/* FAN_METHODS */
static uint64_t nowUs=20000000;static int failedCommand=-1,writeCount=0;static bool allowWrite=true;
static std::map<int,uint16_t> words;
static uint8_t mode=0x17;
static bool allowed(){return allowWrite;}
static uint64_t esp_timer_get_time(){return nowUs;}
static int readWord(uint8_t command,uint16_t *output){if(failedCommand==command)return ESP_FAIL;*output=words[command];return ESP_OK;}
static int readByte(uint8_t command,uint8_t *output){if(failedCommand==command)return ESP_FAIL;*output=command==PMBUS_VOUT_MODE?mode:static_cast<uint8_t>(words[command]);return ESP_OK;}
static int writeWord(uint8_t command,uint16_t){assert(command==PMBUS_VOUT_COMMAND);++writeCount;return ESP_OK;}
static void initializeWords(){failedCommand=-1;mode=0x17;words={{PMBUS_STATUS_WORD,0},{PMBUS_READ_TEMPERATURE_1,55},{PMBUS_READ_VIN,12},{PMBUS_READ_IOUT,60},{PMBUS_READ_PIN,100},{PMBUS_MFR_SPECIFIC_04,589},{PMBUS_READ_VOUT,1178}};}
class TPS53647 {
public:
 bool m_initialized=true;
 float slinear11_to_float(uint16_t);
 int read_word(uint8_t command,uint16_t *output){return readWord(command,output);}
 int read_byte(uint8_t command,uint8_t *output){return readByte(command,output);}
 bool set_vout(float volts){assert(volts>0);++writeCount;return true;}
 bool readTuningTelemetry(BuckTuningTelemetry &);
 bool readRecoveryTelemetry(BuckTuningTelemetry &);
 bool setTuningVoltage(float,bool (*)());
};
/* BASE_METHODS */
namespace Rev7TPS546 {
static bool is_initialized=true;
static int smb_read_word(uint8_t command,uint16_t *output){return readWord(command,output);}
static int smb_read_byte(uint8_t command,uint8_t *output){return readByte(command,output);}
static int smb_write_word(uint8_t command,uint16_t output){return writeWord(command,output);}
class TPS546 {
public:
 bool m_initialized=true;uint16_t m_voltageDomains=2;
 struct {float vout_min=1,vout_max=3;} m_config;
 bool readTuningTelemetry(BuckTuningTelemetry &);
 bool readRecoveryTelemetry(BuckTuningTelemetry &);
 bool setTuningVoltage(float,bool (*)());
};
/* REV_METHODS */
}
static bool buckWorks=true;static float externalVrm=48;
struct FakeBuck {
 bool external=true;
 bool readTuningTelemetry(BuckTuningTelemetry &out){out={};if(!buckWorks)return false;out={12,1.15f,60,100,55,true};return true;}
 bool readRecoveryTelemetry(BuckTuningTelemetry &out){out={};if(!buckWorks)return false;out.vin=12;out.temperatureC=55;return true;}
 bool uses_external_vr_temperature(){return external;}
 bool setTuningVoltage(float,bool (*guard)()){return guard&&guard();}
};
static float TMP1075_read_temperature(int){return externalVrm;}
class Board {
protected:
 int m_asicCount=8;float samples[8]{};float *m_chipTemps=samples;
 pthread_mutex_t m_chipTemperatureMutex=PTHREAD_MUTEX_INITIALIZER;int64_t m_chipTemperatureAtUs[64]{};
public:
 void setChipTemp(int,float);float getChipTemp(int);bool readTuningChipTemperatures(float &,int64_t &);
};
class NerdQaxePlus:public Board {
public:
 bool initialized=true,buckReady=true,shutdown=false;std::vector<float> shared{40,42};FakeBuck buck;FakeBuck *m_tps=&buck;
 virtual float getTemperature(int index){return shared.at(index);}
 virtual float tuningVrmOffset() const{return 0;}
 virtual bool tuningAsicTemperature(float &,int64_t &);
 bool readTuningSensors(TuningSensors &);
 void readTuningFans(TuningFanSample output[2]);
 bool readRecoverySensors(TuningSensors &);bool isRecoveryPowerOff();
 virtual bool refreshTuningTemperatures(){return true;}
 bool setTuningVoltage(float,bool (*)());
 int getNumTempSensors(){return shared.size();}bool isInitialized(){return initialized;}bool isBuckInitialized(){return buckReady;}bool isShutdown(){return shutdown;}
 bool validateVoltage(float volts){return std::isfinite(volts)&&volts>=1.005f&&volts<=1.4f;}
};
class NerdQaxePlus2:public NerdQaxePlus {public:float getTemperature(int index) override;};
class NerdOctaxeGamma:public NerdQaxePlus2 {
public:
 bool m_hasTMux[2]={false,false};bool sixPhase=false;
 unsigned refreshCount=0;bool refreshFail=false;
 void requestChipTemps(){++refreshCount;for(int chip=0;chip<8;++chip){nowUs+=250000;setChipTemp(chip,refreshFail&&chip==3?0:50);}}
 bool refreshTuningTemperatures() override;
 bool tuningAsicTemperature(float &,int64_t &) override;
 float tuningVrmOffset() const override{return sixPhase?8.0f:0;}
};
/* BOARD_METHODS */
int main(){
 Tmp451Mux mux;assert(mux.select_channel(0)==ESP_OK);assert(mux.select_channel(4)==ESP_ERR_INVALID_ARG);
 for(int pin:{2,12}){muxFailedPin=pin;assert(mux.select_channel(3)==ESP_FAIL);}muxFailedPin=-1;
 for(int channel=0;channel<2;++channel){
   const uint8_t base=channel?EMC2302_FAN1:EMC2302_FAN2;
   fanRegisters.clear();fanRegisters[base+EMC2302_OFS_FAN_CONFIG1]=0x08;
   fanRegisters[base+EMC2302_OFS_TACH_READING_MSB]=1487>>5;fanRegisters[base+EMC2302_OFS_TACH_READING_LSB]=(1487&31)<<3;
   uint16_t rpm=0;assert(EMC2302_read_tuning_fan(channel,&rpm)&&rpm==2644);
   for(int command:{base+EMC2302_OFS_FAN_CONFIG1,EMC2302_STALL_STATUS,EMC2302_SPIN_STATUS,EMC2302_DRIVE_STATUS,base+EMC2302_OFS_TACH_READING_MSB,base+EMC2302_OFS_TACH_READING_LSB}){
     fanFailed=command;rpm=999;assert(!EMC2302_read_tuning_fan(channel,&rpm)&&rpm==0);
   }
   fanFailed=-1;
   for(uint8_t status:{EMC2302_STALL_STATUS,EMC2302_SPIN_STATUS,EMC2302_DRIVE_STATUS})for(uint8_t bit:{1,2}){
     fanRegisters[status]=bit;const bool ownFault=bit==(channel?1:2);
     assert(EMC2302_read_tuning_fan(channel,&rpm)==!ownFault);
     assert(ownFault?rpm==0:rpm==2644);fanRegisters[status]=0;
   }
   fanRegisters[base+EMC2302_OFS_FAN_CONFIG1]=0x28;assert(!EMC2302_read_tuning_fan(channel,&rpm));fanRegisters[base+EMC2302_OFS_FAN_CONFIG1]=0x08;
   for(unsigned raw:{0U,8191U}){
     fanRegisters[base+EMC2302_OFS_TACH_READING_MSB]=raw>>5;fanRegisters[base+EMC2302_OFS_TACH_READING_LSB]=(raw&31)<<3;
     assert(EMC2302_get_fan_speed(channel,&rpm)==ESP_OK&&rpm==0);assert(!EMC2302_read_tuning_fan(channel,&rpm));
   }
 }
 // Each logical fan sees its own physical status bits from one shared snapshot.
 const auto initFans=[](unsigned first=1487,unsigned second=1487) {
   fanFailed=-1;fanRegisters.clear();fanReadCounts.clear();
   for(int channel=0;channel<2;++channel) {
     const uint8_t base=channel?EMC2302_FAN1:EMC2302_FAN2;
     const unsigned raw=channel?second:first;
     fanRegisters[base+EMC2302_OFS_FAN_CONFIG1]=0x08;
     fanRegisters[base+EMC2302_OFS_TACH_READING_MSB]=raw>>5;
     fanRegisters[base+EMC2302_OFS_TACH_READING_LSB]=(raw&31)<<3;
   }
 };
 const auto statusReadOnce=[]() {
   for(uint8_t reg:{EMC2302_STALL_STATUS,EMC2302_SPIN_STATUS,EMC2302_DRIVE_STATUS})
     assert(fanReadCounts[reg]==1);
 };
 TuningFanSample fans[2];
 initFans();EMC2302_read_tuning_fans(fans);statusReadOnce();
 assert(fans[0].valid&&fans[1].valid&&fans[0].rpm==2644&&fans[1].rpm==2644);
 for(uint8_t reg:{EMC2302_STALL_STATUS,EMC2302_SPIN_STATUS,EMC2302_DRIVE_STATUS}) {
   for(uint8_t bits:{1,2,3}) {
     initFans();fanRegisters[reg]=bits;EMC2302_read_tuning_fans(fans);statusReadOnce();
     // Faults in FAN1 are not lost when logical fan0/FAN2 is examined first.
     assert(fans[0].valid==!(bits&2));assert(fans[1].valid==!(bits&1));
     assert((bits&2)?fans[0].rpm==0:fans[0].rpm==2644);
     assert((bits&1)?fans[1].rpm==0:fans[1].rpm==2644);
     const char *reason=reg==EMC2302_STALL_STATUS?"stalled":reg==EMC2302_SPIN_STATUS?"spin-up":"drive";
     if(bits&2)assert(std::strstr(fans[0].reason,reason));
     if(bits&1)assert(std::strstr(fans[1].reason,reason));
     assert(fanRegisters[reg]==0); // Read-to-clear fake actually consumed it.
   }
   initFans();fanFailed=reg;EMC2302_read_tuning_fans(fans);statusReadOnce();
   assert(!fans[0].valid&&!fans[1].valid&&fans[0].rpm==0&&fans[1].rpm==0);
   assert(std::strstr(fans[0].reason,"status read failed"));
 }
 // Single connected logical fan0 remains valid despite an unused FAN1 fault.
 initFans(1487,8191);fanRegisters[EMC2302_STALL_STATUS]=1;
 EMC2302_read_tuning_fans(fans);statusReadOnce();assert(fans[0].valid&&!fans[1].valid);
 for(int channel=0;channel<2;++channel) {
   const uint8_t base=channel?EMC2302_FAN1:EMC2302_FAN2;
   initFans();fanFailed=base+EMC2302_OFS_FAN_CONFIG1;
   EMC2302_read_tuning_fans(fans);statusReadOnce();
   assert(!fans[channel].valid&&fans[1-channel].valid);
   assert(std::strstr(fans[channel].reason,"configuration read failed"));
   initFans();fanRegisters[base+EMC2302_OFS_FAN_CONFIG1]=0x28;
   EMC2302_read_tuning_fans(fans);assert(!fans[channel].valid&&fans[1-channel].valid);
   assert(std::strstr(fans[channel].reason,"configuration is unsupported"));
   for(uint8_t offset:{EMC2302_OFS_TACH_READING_MSB,EMC2302_OFS_TACH_READING_LSB}) {
     initFans();fanFailed=base+offset;EMC2302_read_tuning_fans(fans);
     assert(!fans[channel].valid&&fans[1-channel].valid);
     assert(std::strstr(fans[channel].reason,"tachometer read failed"));
   }
   for(unsigned raw:{0U,8191U,7865U,245U}) {
     initFans(channel?1487:raw,channel?raw:1487);EMC2302_read_tuning_fans(fans);
     assert(!fans[channel].valid&&fans[1-channel].valid&&fans[channel].rpm==0);
   }
   for(unsigned raw:{7864U,246U}) {
     initFans(channel?1487:raw,channel?raw:1487);EMC2302_read_tuning_fans(fans);
     assert(fans[channel].valid&&fans[1-channel].valid);
   }
 }
 initFans();NerdQaxePlus fanBoard;fanBoard.readTuningFans(fans);statusReadOnce();
 assert(fans[0].valid&&fans[1].valid); // The production board uses the batch path.
 initFans();uint16_t invalidRpm=123;assert(!EMC2302_read_tuning_fan(-1,&invalidRpm));
 assert(!EMC2302_read_tuning_fan(2,&invalidRpm));assert(!EMC2302_read_tuning_fan(0,nullptr));
 EMC2302_read_tuning_fans(nullptr);assert(fanReadCounts.empty());
 initializeWords();TPS53647 four;Rev7TPS546::TPS546 rev;
 BuckTuningTelemetry sample;
 assert(four.readTuningTelemetry(sample)&&sample.measuredInputPower&&sample.pin==100&&std::fabs(sample.vout-1.15039)<0.001);
 assert(rev.readTuningTelemetry(sample)&&!sample.measuredInputPower&&std::fabs(sample.vout-1.15039)<0.001);
 for(int command:{PMBUS_STATUS_WORD,PMBUS_READ_TEMPERATURE_1,PMBUS_READ_VIN,PMBUS_MFR_SPECIFIC_04,PMBUS_READ_IOUT,PMBUS_READ_PIN}){
   failedCommand=command;sample.vin=999;assert(!four.readTuningTelemetry(sample)&&sample.vin==0);assert(!four.setTuningVoltage(1.15f,allowed));
 }
 for(int command:{PMBUS_STATUS_WORD,PMBUS_VOUT_MODE,PMBUS_READ_TEMPERATURE_1,PMBUS_READ_VIN,PMBUS_READ_VOUT,PMBUS_READ_IOUT}){
   failedCommand=command;sample.vin=999;assert(!rev.readTuningTelemetry(sample)&&sample.vin==0);assert(!rev.setTuningVoltage(1.15f,allowed));
 }
 initializeWords();words[PMBUS_STATUS_WORD]=0x80;assert(!four.readTuningTelemetry(sample)&&!rev.readTuningTelemetry(sample));
 initializeWords();four.m_initialized=false;Rev7TPS546::is_initialized=false;
 words[PMBUS_STATUS_WORD]=0x0840;words[PMBUS_MFR_SPECIFIC_04]=0;words[PMBUS_READ_VOUT]=0;words[PMBUS_READ_IOUT]=0;words[PMBUS_READ_PIN]=0;
 assert(four.readRecoveryTelemetry(sample)&&rev.readRecoveryTelemetry(sample)&&sample.vin==12&&sample.vout==0&&sample.iout==0);
 assert(!four.readTuningTelemetry(sample)&&!rev.readTuningTelemetry(sample));
 for(int command:{PMBUS_STATUS_WORD,PMBUS_READ_TEMPERATURE_1,PMBUS_READ_VIN,PMBUS_STATUS_VOUT,PMBUS_STATUS_IOUT,PMBUS_STATUS_INPUT,PMBUS_STATUS_TEMPERATURE,PMBUS_STATUS_CML}) {
   failedCommand=command;assert(!four.readRecoveryTelemetry(sample)&&!rev.readRecoveryTelemetry(sample));
 }
 failedCommand=-1;for(int command:{PMBUS_STATUS_VOUT,PMBUS_STATUS_IOUT,PMBUS_STATUS_INPUT,PMBUS_STATUS_TEMPERATURE,PMBUS_STATUS_CML}) {
   words[command]=1;assert(!four.readRecoveryTelemetry(sample)&&!rev.readRecoveryTelemetry(sample));words[command]=0;
 }
 words[PMBUS_STATUS_WORD]=0x0844;assert(!four.readRecoveryTelemetry(sample)&&!rev.readRecoveryTelemetry(sample));
 four.m_initialized=true;Rev7TPS546::is_initialized=true;
 initializeWords();words[PMBUS_READ_TEMPERATURE_1]=128;assert(!four.readTuningTelemetry(sample)&&!rev.readTuningTelemetry(sample));
 initializeWords();mode=0x7f;assert(!rev.readTuningTelemetry(sample)&&!rev.setTuningVoltage(1.15f,allowed));
 initializeWords();allowWrite=false;assert(!four.setTuningVoltage(1.15f,allowed)&&!rev.setTuningVoltage(1.15f,allowed)&&writeCount==0);
 allowWrite=true;assert(four.setTuningVoltage(1.15f,allowed)&&rev.setTuningVoltage(1.15f,allowed)&&writeCount==2);
 NerdQaxePlus2 q;TuningSensors sensors;q.initialized=false;assert(q.isRecoveryPowerOff()&&q.readRecoverySensors(sensors)&&sensors.asicTempC==52&&sensors.vin==12&&sensors.powerW==0);
 gpioLevels[TPS53647_EN_PIN]=1;assert(!q.isRecoveryPowerOff());gpioLevels[TPS53647_EN_PIN]=0;q.initialized=true;assert(q.readTuningSensors(sensors)&&sensors.asicTempC==52&&sensors.vrmTempC==55);
 q.shared[0]=0;assert(!q.readTuningSensors(sensors));q.shared[0]=40;
 externalVrm=0;assert(!q.readTuningSensors(sensors));externalVrm=48;
 buckWorks=false;assert(!q.readTuningSensors(sensors));buckWorks=true;
 q.buck.external=false;assert(q.readTuningSensors(sensors));q.shutdown=true;assert(!q.readTuningSensors(sensors));
 NerdOctaxeGamma oct;oct.sixPhase=true;externalVrm=60;assert(oct.readTuningSensors(sensors)&&sensors.asicTempC==52&&sensors.vrmTempC==68);
 externalVrm=0;assert(!oct.readTuningSensors(sensors));externalVrm=48;
 oct.m_hasTMux[0]=true;assert(!oct.readTuningSensors(sensors));oct.m_hasTMux[1]=true;
 for(int chip=0;chip<8;++chip)oct.setChipTemp(chip,45+chip);
 assert(oct.readTuningSensors(sensors)&&sensors.asicTempC==52);
 nowUs+=5000001;assert(!oct.readTuningSensors(sensors));
 for(int chip=0;chip<8;++chip)oct.setChipTemp(chip,50);
 oct.setChipTemp(5,0);assert(!oct.readTuningSensors(sensors));
 assert(oct.refreshTuningTemperatures()&&oct.refreshCount==1);
 for(int pass=0;pass<25;++pass){nowUs+=800000;assert(oct.refreshTuningTemperatures()&&oct.readTuningSensors(sensors));}
 assert(oct.refreshCount>3);oct.refreshFail=true;nowUs+=5000001;assert(!oct.refreshTuningTemperatures());
 oct.m_hasTMux[0]=oct.m_hasTMux[1]=false;const auto refreshes=oct.refreshCount;assert(oct.refreshTuningTemperatures()&&oct.refreshCount==refreshes);
 std::cout<<"Actual strict TPS53647/67, rev7 TPS546, EMC2302 batched per-channel read-to-clear status/tach and shared/mux board sensors: failed reads/mode/status/saturation/zero, cross-channel isolation, revocation, calibrated offsets, partial/stale/missing per-chip inputs passed\n";
}
