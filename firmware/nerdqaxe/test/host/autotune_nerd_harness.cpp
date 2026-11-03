static JsonDocument response(const char *body) {
    httpd_req_t request;request.body=body;request.content_len=request.body.size();
    const auto code=POST_five_tratum_autotune(&request);
    JsonDocument result;
    if(code==ESP_FAIL){result["ok"]=false;return result;}
    assert(deserializeJson(result,request.response)==DeserializationError::Ok);
    return result;
}
static JsonDocument snapshot() {
    JsonDocument report;assert(FiveTratumAutotune::report(report));
    std::string wire;serializeJson(report,wire);
    JsonDocument result;assert(deserializeJson(result,wire)==DeserializationError::Ok);return result;
}
static void start() {
    auto result=response("{\"action\":\"start\",\"waiverAccepted\":true,\"waiverVersion\":1}");
    if(!result["ok"].as<bool>()){std::string text;serializeJson(result,text);std::cerr<<text<<"\n";}
    assert(result["ok"].as<bool>());
}
static void step() {
    hostNow+=2000000;FiveTratumAutotune::tick();
    if(workerNotifications&&workerFunction){try{workerFunction(nullptr);}catch(const WorkerSleeping &){}}
}
static void boot() {
    using namespace FiveTratumAutotune;
    initialized=false;firmwareUpdates.store(0);permanentFreeze.store(false);optionsError=false;backupAvailable=false;backupRestored=false;restoring=false;restoreSaved=false;restoreCommitFailed=false;restoreWorker=nullptr;workerNotifications=0;pending=false;
    controller={};observed={};backup.clear();adapterReason=nullptr;handoff.store(nullptr);transitionHealthUntilUs=0;
    POWER_MANAGEMENT_MODULE.appliedFrequency=hostBoard.frequency;
    POWER_MANAGEMENT_MODULE.appliedVoltage=hostBoard.voltage;
    ++hostGeneration;step();
}
static void candidate(double frequency,double voltage) {
    using namespace FiveTratumAutotune;
    controller.owns=true;controller.active=true;controller.awaitingApply=true;controller.pending={frequency,voltage};
    target={frequency,voltage};pending=true;voltageMatches=0;transitionStarted=hostNow;
}
static uint64_t wireEpoch=0;
static bool wirePermission(){return Config::tuningMutationRevision()==wireEpoch;}
int main(int argc,char **argv) {
    assert(argc==2);const std::string scenario=argv[1];
    if(scenario=="oct"||scenario=="weak-chip-oct")hostBoard.count=8;
    step();
    assert(snapshot()["supported"].as<bool>());
    assert(!snapshot()["backup"]["available"].as<bool>());
    assert(!snapshot()["status"]["active"].as<bool>());
    auto initial=snapshot();
    for(const char *key:{"baseline","current","best"}) {
        bool present=false;
        for(JsonPairConst pair:initial["status"].as<JsonObjectConst>())if(!strcmp(pair.key().c_str(),key))present=true;
        assert(present);
    }
    assert(initial["status"]["baseline"].isNull()&&initial["status"]["best"].isNull());
    assert(initial["status"]["current"]["frequencyMHz"].as<int>()==600&&initial["status"]["current"]["coreVoltageMv"].as<int>()==1150);
    if(scenario=="restore-lock-order") {
        JsonDocument original;assert(FiveTratumAutotune::captureEffective(&hostBoard,original.to<JsonObject>()));
        std::thread restore;
        {
            FiveTratumAutotune::WriteGuard frame;
            restore=std::thread([&]{assert(Config::restoreTuningSettings(original.as<JsonObjectConst>()));});
            while(!configCommitted.load())std::this_thread::yield();
            const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(1);
            bool unlocked=false;
            while(std::chrono::steady_clock::now()<deadline) {
                if(pthread_mutex_trylock(&Config::s_mutex)==0){pthread_mutex_unlock(&Config::s_mutex);unlocked=true;break;}
                std::this_thread::yield();
            }
            assert(unlocked); // Restore waits on wire only after releasing config.
        }
        restore.join();
    } else if(scenario=="uart-revocation") {
        using namespace FiveTratumAutotune;
        Asic device;device.m_recoveryGuard=[](void *){return true;};uint8_t command[6]{};
        pending=true;mutationRevision=Config::tuningMutationRevision();
        std::atomic_bool revoked{false},attempting{false};
        std::thread writer([&]{device.send(0x51,command,6);});
        while(!wireEntered.load())std::this_thread::yield();
        std::thread revoker([&]{attempting=true;Config::cfgSetU16("asicfrequency",600);revoked=true;});
        while(!attempting.load())std::this_thread::yield();
        assert(!revoked.load());wireRelease=true;writer.join();revoker.join();
        assert(revoked.load()&&physicalWrites==1);
        device.send(0x51,command,6);assert(physicalWrites==1&&device.m_transportFailed.load());
    } else if(scenario=="polarity-revocation") {
        wireEpoch=Config::tuningMutationRevision();pausePolarityWire=true;
        std::atomic_bool revoked{false},attempting{false};
        std::thread writer([&]{assert(hostBoard.restoreTuningFanPolarity(false,wirePermission));});
        while(!wireEntered.load())std::this_thread::yield();
        std::thread revoker([&]{attempting=true;Config::cfgSetU16("pwmfanpol",1);revoked=true;});
        while(!attempting.load())std::this_thread::yield();
        assert(!revoked.load());wireRelease=true;writer.join();revoker.join();
        assert(revoked.load()&&physicalWrites==1);
        assert(!hostBoard.restoreTuningFanPolarity(false,wirePermission)&&physicalWrites==1);
    } else if(scenario=="wire-revocation") {
        TPS53647 device;wireEpoch=Config::tuningMutationRevision();
        std::atomic_bool revoked{false},attempting{false};
        std::thread writer([&]{assert(device.setTuningVoltage(1.15f,wirePermission));});
        while(!wireEntered.load())std::this_thread::yield();
        std::thread revoker([&]{attempting=true;Config::cfgSetU16("asicfrequency",600);revoked=true;});
        while(!attempting.load())std::this_thread::yield();
        assert(!revoked.load());wireRelease=true;writer.join();revoker.join();
        assert(revoked.load()&&physicalWrites==1);
        assert(!device.setTuningVoltage(1.15f,wirePermission)&&physicalWrites==1);
    } else if(scenario=="waiver") {
        for(const char *body:{"{\"action\":\"start\"}","{\"action\":\"start\",\"waiverAccepted\":false,\"waiverVersion\":1}","{\"action\":\"start\",\"waiverAccepted\":true,\"waiverVersion\":2}"})
            assert(!response(body)["ok"].as<bool>());
        assert(nvstore.empty()&&writes.empty());
        otpAllowed=false;assert(!response("{\"action\":\"stop\"}")["ok"].as<bool>());
        otpAllowed=true;networkAllowed=false;assert(!response("{\"action\":\"start\",\"waiverAccepted\":true,\"waiverVersion\":1}")["ok"].as<bool>());
    } else if(scenario=="backup-write-failure" || scenario=="backup-readback-failure") {
        storageFail=scenario=="backup-write-failure";verifyFail=scenario=="backup-readback-failure";
        assert(!response("{\"action\":\"start\",\"waiverAccepted\":true,\"waiverVersion\":1}")["ok"].as<bool>());
        assert(writes.empty()&&!snapshot()["status"]["ownsSettings"].as<bool>());
    } else if(scenario=="pending-manual"||scenario=="pending-manual-cache") {
        Config::cfgSetU16("asicfrequency",650);if(scenario=="pending-manual")hostBoard.loadSettings();step();
        assert(!response("{\"action\":\"start\",\"waiverAccepted\":true,\"waiverVersion\":1}")["ok"].as<bool>());
        assert(nvstore.empty()&&writes.empty());
    } else if(scenario=="invalid-config") {
        auto good=snapshot();JsonDocument document;document["action"]="configure";document["config"].set(good["config"]);
        for(const char *key:{"maxAsicTempC","maxVrmTempC","maxVoltageMv","maxFrequencyMHz","maxPowerW"}) {
            auto prior=document["config"][key].as<double>();document["config"][key]=1000000;
            std::string body;serializeJson(document,body);assert(!response(body.c_str())["ok"].as<bool>());document["config"][key]=prior;
        }
        assert(nvstore.empty());
    } else if(scenario=="backup-slow"||scenario=="backup-revocation"||scenario=="fan-read-failure") {
        if(scenario=="backup-slow")storageDelayUs=16000000;
        if(scenario=="backup-revocation")revokeDuringBackup=true;
        if(scenario=="fan-read-failure")fanReadFail=true;
        assert(!response("{\"action\":\"start\",\"waiverAccepted\":true,\"waiverVersion\":1}")["ok"].as<bool>());
        assert(writes.empty()&&!snapshot()["status"]["active"].as<bool>());
    } else if(scenario=="backup-invalid-initial") {
        Config::cfgSetU16("autofanspeed",1);step();
        assert(!response("{\"action\":\"start\",\"waiverAccepted\":true,\"waiverVersion\":1}")["ok"].as<bool>());
        assert(nvstore.find("fwtune/baseline")==nvstore.end());
        Config::cfgSetU16("autofanspeed",2);step();start();
        assert(snapshot()["backup"]["available"].as<bool>());
    } else if(scenario=="weak-chip-qa"||scenario=="weak-chip-oct") {
        weakChip=true;step();assert(!snapshot()["status"]["telemetry"]["valid"].as<bool>());
        assert(!response("{\"action\":\"start\",\"waiverAccepted\":true,\"waiverVersion\":1}")["ok"].as<bool>());
        assert(writes.empty()&&nvstore.empty());
    } else if(scenario=="start-readback") {
        voltageOffset=50;step();assert(!response("{\"action\":\"start\",\"waiverAccepted\":true,\"waiverVersion\":1}")["ok"].as<bool>());
        assert(writes.empty()&&nvstore.empty());
    } else if(scenario=="sensor-failure") {
        sensorFail=true;step();assert(!snapshot()["status"]["telemetry"]["valid"].as<bool>());
        assert(!response("{\"action\":\"start\",\"waiverAccepted\":true,\"waiverVersion\":1}")["ok"].as<bool>());
        assert(writes.empty()&&nvstore.empty());
    } else {
        start();
        assert(snapshot()["backup"]["available"].as<bool>());
        assert(snapshot()["status"]["baseline"]["frequencyMHz"].as<int>()==600&&snapshot()["status"]["baseline"]["coreVoltageMv"].as<int>()==1150);
        if(scenario=="json-points") {
            FiveTratumAutotune::controller.best={590,1145};FiveTratumAutotune::controller.bestAvailable=true;
            auto wire=snapshot();
            assert(wire["status"]["best"]["frequencyMHz"].as<int>()==590&&wire["status"]["best"]["coreVoltageMv"].as<int>()==1145);
            std::cout<<"Actual production report real-ArduinoJson serialized null/current/baseline/best points passed\n";
            return 0;
        }
        const std::string original=nvstore["fwtune/baseline"];
        JsonDocument saved;assert(deserializeJson(saved,original)==DeserializationError::Ok);
        assert(saved["settings"].size()==20&&saved["frequencyMHz"].as<int>()==600&&saved["coreVoltageMv"].as<int>()==1150);
        assert(saved["settings"]["wifiPass"].isNull());
        if(scenario=="backup-cached-deleted"||scenario=="backup-cached-changed") {
            FiveTratumAutotune::externalHandoff("Manual control");step();
            if(scenario=="backup-cached-deleted")nvstore.erase("fwtune/baseline");
            else {
                saved["frequencyMHz"]=590;saved["settings"]["asicfrequency"]=590;
                std::string changed;serializeJson(saved,changed);nvstore["fwtune/baseline"]=changed;
            }
            assert(!response("{\"action\":\"start\",\"waiverAccepted\":true,\"waiverVersion\":1}")["ok"].as<bool>());
            assert(writes.empty());
            if(scenario=="backup-cached-deleted")assert(nvstore.find("fwtune/baseline")==nvstore.end());
        } else if(scenario=="backup-identity"||scenario=="backup-settings") {
            if(scenario=="backup-identity")saved["deviceId"]="different-miner";
            else saved["settings"]["fanspeed"]=101;
            std::string corrupt;serializeJson(saved,corrupt);nvstore["fwtune/baseline"]=corrupt;boot();
            assert(!snapshot()["backup"]["available"].as<bool>());
            assert(!response("{\"action\":\"start\",\"waiverAccepted\":true,\"waiverVersion\":1}")["ok"].as<bool>());
            assert(nvstore["fwtune/baseline"]==corrupt&&writes.empty());
        } else if(scenario=="ota-freeze") {
            {
                FiveTratumAutotune::FirmwareUpdateGuard upload;
                step();assert(FiveTratumAutotune::ownsSettings()&&!FiveTratumAutotune::transitionAllowed());
                assert(!response("{\"action\":\"start\",\"waiverAccepted\":true,\"waiverVersion\":1}")["ok"].as<bool>());
            }
            assert(!FiveTratumAutotune::ownsSettings()&&FiveTratumAutotune::transitionAllowed());
            assert(writes.empty());
        } else if(scenario=="manual-handoff"||scenario=="ota-handoff") {
            if(scenario=="manual-handoff")Config::cfgSetU16("asicfrequency",600); // Same-value requests still revoke.
            else FiveTratumAutotune::externalHandoff("Firmware update took ownership");
            step();assert(!snapshot()["status"]["ownsSettings"].as<bool>()&&writes.empty());
            assert(nvstore["fwtune/baseline"]==original);
        } else if(scenario=="configure-owned-bounds") {
            auto good=snapshot();JsonDocument document;document["action"]="configure";document["config"].set(good["config"]);
            document["config"]["maxFrequencyMHz"]=590;
            std::string body;serializeJson(document,body);assert(!response(body.c_str())["ok"].as<bool>());
            assert(nvstore.find("fwtune/options")==nvstore.end());
            assert(snapshot()["config"]["maxFrequencyMHz"].as<double>()==600);
        } else if(scenario=="configure-pending") {
            candidate(590,1145);
            auto good=snapshot();JsonDocument document;document["action"]="configure";document["config"].set(good["config"]);
            document["config"]["maxAsicTempC"]=60;
            std::string body;serializeJson(document,body);assert(!response(body.c_str())["ok"].as<bool>());
            assert(snapshot()["config"]["maxAsicTempC"].as<double>()==65);
        } else if(scenario=="pending-headroom") {
            candidate(610,1155);asicDegrees=64;step();
            assert(paused&&SYSTEM_MODULE.error==Board::NONE&&writes.empty());
            assert(snapshot()["status"]["state"]=="suspended");
        } else if(scenario=="readback-timeout") {
            candidate(610,1155);step();voltageOffset=50;
            for(int i=0;i<20;++i)step();
            assert(paused&&SYSTEM_MODULE.error==Board::VOUT_FAULT);
            assert(writes.size()==1&&writes[0]=="voltage:1155");
        } else if(scenario=="pll-long-healthy"||scenario=="pll-long-failure") {
            candidate(610,1150);miningGateOpen=false;counterValid=false; // Retired counters and closed gate are expected.
            Asic device;device.m_recoveryGuard=[](void *){return FiveTratumAutotune::transitionHealthy();};
            wireRelease=true;uint8_t command[6]{};const uint64_t beginning=hostNow;
            bool blocked=false;
            for(int i=0;i<200;++i){
                if(scenario=="pll-long-failure"&&hostNow-beginning>=10000000)sensorFail=true;
                if(!device.recoveryDelay(100)){blocked=true;break;}
                device.send(0x51,command,6);
            }
            assert(scenario=="pll-long-failure"?blocked:!blocked);
            assert(scenario=="pll-long-failure"?physicalWrites<110:physicalWrites==200);
            assert(writes.empty()&&nvstore["fwtune/baseline"]==original);
        } else if(scenario=="pll-strict-sensor"||scenario=="pll-strict-fan"||scenario=="pll-strict-headroom") {
            candidate(610,1150);
            assert(FiveTratumAutotune::transitionHealthy());hostNow+=800001;
            if(scenario=="pll-strict-sensor")sensorFail=true;
            if(scenario=="pll-strict-fan")fanReadFail=true;
            if(scenario=="pll-strict-headroom")asicDegrees=64;
            assert(!FiveTratumAutotune::transitionHealthy());
            assert(writes.empty()&&nvstore["fwtune/baseline"]==original);
        } else if(scenario=="voltage-failure") {
            candidate(590,1145);voltageFail=true;step();step();
            assert(paused&&SYSTEM_MODULE.error==Board::VOUT_FAULT&&!snapshot()["status"]["active"].as<bool>());
            assert(nvstore["fwtune/baseline"]==original);
        } else if(scenario=="restored-status-age") {
            assert(response("{\"action\":\"stop\"}")["ok"].as<bool>());
            for(int i=0;i<8;++i)step();
            assert(snapshot()["backup"]["restored"].as<bool>());
            hostNow+=16000000;assert(!snapshot()["backup"]["restored"].as<bool>());
            step();assert(snapshot()["backup"]["restored"].as<bool>());
            Config::cfgSetU16("asicfrequency",600);assert(!snapshot()["backup"]["restored"].as<bool>());
        } else if(scenario.rfind("restore",0)==0) {
            POWER_MANAGEMENT_MODULE.appliedFrequency=scenario=="restore-down"?650:550;
            POWER_MANAGEMENT_MODULE.appliedVoltage=scenario=="restore-down"?1200:1100;
            Config::s_doc["wifiPassword"]="test-secret-never-backup";
            Config::cfgSetU16("pid_temp",67);hostBoard.loadSettings();
            if(scenario=="restore-reboot")boot();
            if(scenario=="restore-fault")SYSTEM_MODULE.error=Board::TEMP_FAULT;
            if(scenario=="restore-storage-failure")storageFail=true;
            if(scenario=="restore-worker-failure")workerCreateFail=true;
            if(scenario=="restore-hot")asicDegrees=72;
            if(scenario=="restore-disabled-protection") {
                Config::cfgSetU16("overheat_temp",0);Config::cfgSetU16("fan1_overheat",0);boot();
                assert(!response("{\"action\":\"start\",\"waiverAccepted\":true,\"waiverVersion\":1}")["ok"].as<bool>());
                asicDegrees=30;vrmDegrees=30;
            }
            if(scenario=="restore-polarity-failure")polarityFail=true;
            const auto result=response("{\"action\":\"stop\"}");
            if(workerCreateFail) {
                assert(!result["ok"].as<bool>());step();assert(writes.empty()&&Config::s_doc["pid_temp"].as<int>()==67);
                assert(snapshot()["status"]["ownsSettings"].as<bool>());
                workerCreateFail=false;assert(response("{\"action\":\"stop\"}")["ok"].as<bool>());
            } else assert(result["ok"].as<bool>()); // Accepted request; GET confirms completion.
            if(storageFail) {
                step();assert(writes.empty()&&Config::s_doc["pid_temp"].as<int>()==67);
                assert(snapshot()["status"]["ownsSettings"].as<bool>());
                storageFail=false;step();assert(writes.empty()&&Config::s_doc["pid_temp"].as<int>()==67);
                assert(response("{\"action\":\"stop\"}")["ok"].as<bool>());
            }
            if(scenario=="restore-fault"||scenario=="restore-hot") {
                step();step();assert(writes.empty()&&polarityWrites==0&&Config::s_doc["pid_temp"].as<int>()==67);
                if(scenario=="restore-hot")asicDegrees=50;
            }
            if(scenario=="restore-polarity-failure") {
                step();step();assert(writes.empty()&&polarityWrites==1&&!snapshot()["backup"]["restored"].as<bool>());
                polarityFail=false;assert(response("{\"action\":\"stop\"}")["ok"].as<bool>());
            }
            for(int i=0;i<8;++i)step();
            if(scenario=="restore-fault")assert(writes.empty()&&!snapshot()["backup"]["restored"].as<bool>()&&SYSTEM_MODULE.error==Board::TEMP_FAULT);
            else {
                assert(snapshot()["backup"]["restored"].as<bool>());
                assert(POWER_MANAGEMENT_MODULE.appliedFrequency==600&&POWER_MANAGEMENT_MODULE.appliedVoltage==1150);
                if(!writes.empty()){
                    assert(writes.front()==(scenario=="restore-down"?"clock:600":"voltage:1150"));
                    assert(writes.back()==(scenario=="restore-down"?"voltage:1150":"clock:600"));
                }
                assert(Config::s_doc["pid_temp"].as<int>()==55);
                assert(Config::s_doc["wifiPassword"]=="test-secret-never-backup");
                assert(Config::s_doc["fan1mode"].as<int>()==2&&Config::s_doc["overheat_temp"].as<int>()==75);
                boot();assert(snapshot()["backup"]["restored"].as<bool>());
            }
            assert(nvstore["fwtune/baseline"]==original);
        } else {
            assert(scenario=="qa"||scenario=="oct");
            candidate(590,1145);
            for(int i=0;i<6;++i)step();
            assert(writes.size()==2&&writes[0]=="clock:590"&&writes[1]=="voltage:1145");
            assert(POWER_MANAGEMENT_MODULE.appliedFrequency==590&&POWER_MANAGEMENT_MODULE.appliedVoltage==1145);
            assert(Config::s_doc["asicfrequency"].isNull()&&Config::s_doc["asicvoltage"].isNull());
            assert(nvstore["fwtune/baseline"]==original);
        }
    }
    std::cout<<"Actual Nerd tuning runtime/HTTP/config transaction passed: "<<scenario<<"\n";
}
