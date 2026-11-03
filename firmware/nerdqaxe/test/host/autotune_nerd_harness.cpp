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
    hostNow+=2000000;
    {HostStackContext power(false);FiveTratumAutotune::tick();}
    if(workerNotifications&&workerFunction){HostStackContext worker(!hostWorkerExternal);try{workerFunction(nullptr);}catch(const WorkerSleeping &){}}
}
static void boot() {
    using namespace FiveTratumAutotune;
    initialized=false;firmwareUpdates.store(0);permanentFreeze.store(false);optionsError=false;backupAvailable=false;backupRestored=false;restoring=false;restoreSaved=false;restoreCommitFailed=false;restoreWorker=nullptr;workerNotifications=0;pending=false;
    storagePrepared=false;powerInitialized=false;baselineLoadError=false;optionsStatus=BlobStatus::Unloaded;baselineStatus=BlobStatus::Unloaded;bootOptions.clear();restoreMinFreeBytes=UINT32_MAX;restoreStackLow=false;
    controller={};history={};lastOwnerTickUs=0;recoveryRequested=false;recoveryAutomatic=false;recoveryPhase="idle";recoveryReason="No recovery requested";recoveryStartupGuardFailure=nullptr;observed={};backup.clear();adapterReason=nullptr;handoff.store(nullptr);transitionHealthUntilUs=0;
    POWER_MANAGEMENT_MODULE.appliedFrequency=hostBoard.frequency;
    POWER_MANAGEMENT_MODULE.appliedVoltage=hostBoard.voltage;
    ++hostGeneration;
    {HostStackContext main(true);preloadStorage();}
    {HostStackContext power(false);powerReady();}
    step();
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
    assert(FiveTratumAutotune::preloadStorage());
    {HostStackContext power(false);FiveTratumAutotune::powerReady();}
    step();
    assert(snapshot()["supported"].as<bool>());
    assert(!snapshot()["backup"]["available"].as<bool>());
    assert(!snapshot()["status"]["active"].as<bool>());
    auto initial=snapshot();
    assert(initial["config"]["maxAsicTempC"]==60 && initial["config"]["maxVrmTempC"]==70);
    for(const char *key:{"baseline","current","best"}) {
        bool present=false;
        for(JsonPairConst pair:initial["status"].as<JsonObjectConst>())if(!strcmp(pair.key().c_str(),key))present=true;
        assert(present);
    }
    assert(initial["status"]["baseline"].isNull()&&initial["status"]["best"].isNull());
    assert(initial["status"]["current"]["frequencyMHz"].as<int>()==600&&initial["status"]["current"]["coreVoltageMv"].as<int>()==1150);
    if(scenario=="recovery-startup-margin") {
        using namespace FiveTratumAutotune;
        start();FiveTratumMining::manualOverride(true);FiveTratumMining::updateControl();
        assert(response("{\"action\":\"restore-and-resume\"}")["ok"]==true);recoveryAutomatic=true;
        asicDegrees=50;vrmDegrees=60;assert(recoverySensorsSafe(&hostBoard,true));
        // Actual off admission is still 5 C below the configured 60/70 caps.
        asicDegrees=56;vrmDegrees=66;assert(!recoverySensorsSafe(&hostBoard,true));
        {RecoveryStartupGuard startup;
            assert(transitionHealthy()); // Powered startup may warm within the user ceilings.
            asicDegrees=60;assert(!transitionHealthy());assert(strstr(recoveryStartupGuardFailure,"user thermal ceiling"));asicDegrees=56;
            vrmDegrees=70;assert(!transitionHealthy());vrmDegrees=66;
            offSensorFail=true;assert(!transitionHealthy());assert(strstr(recoveryStartupGuardFailure,"sensor or regulator-status read failed"));offSensorFail=false;
            SYSTEM_MODULE.error=Board::TEMP_FAULT;assert(!transitionHealthy());SYSTEM_MODULE.error=Board::NONE;
            ++hostPolicyRevision;assert(!transitionHealthy());
        }
        assert(recoveryResumeCalls==0&&!preserveRecoveryFaults());return 0;
    }
    if(scenario.rfind("auto-",0)==0) {
        using namespace FiveTratumAutotune;
        if(scenario=="auto-worker-failure") {workerCreateFail=true;assert(response("{\"action\":\"start\",\"waiverAccepted\":true,\"waiverVersion\":1}")["error"]=="storage-unavailable"&&!controller.active);return 0;}
        const char *fixed=R"({"action":"configure","config":{"priority":"balanced","continuous":true,"minFrequencyMHz":600,"maxFrequencyMHz":600,"minVoltageMv":1150,"maxVoltageMv":1150,"maxAsicTempC":60,"maxVrmTempC":70,"maxPowerW":200}})";
        assert(response(fixed)["ok"]==true);start();const auto original=nvstore["fwtune/baseline"];
        if(scenario=="auto-upper-caps") {
            candidate(590,1150);for(int i=0;i<2;++i)step();assert(!pending);
            auto d=snapshot();JsonDocument changed;changed["action"]="configure";changed["config"].set(d["config"]);changed["config"]["minFrequencyMHz"]=590;changed["config"]["maxFrequencyMHz"]=590;
            std::string body;serializeJson(changed,body);assert(response(body.c_str())["ok"]==true);
        }
        asicDegrees=62;
        if(scenario=="auto-hardware-fault")SYSTEM_MODULE.error=Board::TEMP_FAULT;
        step();
        if(scenario=="auto-hardware-fault") {for(int i=0;i<40;++i)step();assert(!recoveryRequested&&paused&&recoveryResumeCalls==0&&SYSTEM_MODULE.error==Board::TEMP_FAULT);return 0;}
        if(scenario=="auto-upper-caps") {for(int i=0;i<40;++i)step();assert(paused&&!recoveryRequested&&recoveryResumeCalls==0&&snapshot()["status"]["recovery"]["phase"]=="blocked");return 0;}
        assert(recoveryRequested&&recoveryAutomatic&&paused&&!controller.active);
        if(scenario=="auto-storage")storageFail=true;
        if(scenario=="auto-manual")Config::cfgSetU16("asicfrequency",650);
        if(scenario=="auto-no-retry")resumeFail=true;
        for(int i=0;i<4;++i)step();
        assert(recoveryResumeCalls==0); // user ceiling cooldown is mandatory.
        asicDegrees=50;
        for(int i=0;i<40;++i)step();
        auto result=snapshot();
        if(scenario=="auto-cooling") {
            assert(!paused&&recoveryResumeCalls==1&&!recoveryRequested&&!controller.active&&!controller.owns);
            assert(result["backup"]["restored"]==true&&result["status"]["recovery"]["phase"]=="complete");
            bool bad=false;for(auto event:result["status"]["events"].as<JsonArrayConst>())if(event["kind"]=="rejected"&&event["outcome"]=="failed")bad=true;assert(bad);
        } else {assert(paused&&!recoveryRequested&&result["backup"]["restored"]==false);}
        if(scenario=="auto-no-retry"){resumeFail=false;for(int i=0;i<40;++i)step();assert(paused&&recoveryResumeCalls==1);}
        assert(nvstore["fwtune/baseline"]==original);return 0;
    }
    if(scenario.rfind("recovery-",0)==0) {
        using namespace FiveTratumAutotune;
        start();const auto baselineRecord=nvstore["fwtune/baseline"];
        if(scenario=="recovery-running") {const unsigned writesBefore=hostNvsCalls;assert(response("{\"action\":\"restore-and-resume\"}")["error"]=="miner-not-paused"&&hostNvsCalls==writesBefore&&controller.active);return 0;}
        if(scenario=="recovery-floor") {
            auto beforeFloor=snapshot();auto config=beforeFloor["config"].as<JsonObjectConst>();JsonDocument body;body["action"]="configure";body["config"].set(config);body["config"]["minFrequencyMHz"]=600;body["config"]["minVoltageMv"]=1150;
            std::string json;serializeJson(body,json);assert(response(json.c_str())["ok"]==true);
            asicDegrees=62;step();assert(paused&&!controller.active);
            auto afterFloor=snapshot();auto failed=afterFloor["status"]["events"].as<JsonArrayConst>();bool found=false;
            for(auto event:failed)if(event["kind"]=="rejected"&&event["outcome"]=="failed") {found=true;assert(event["reason"]=="temperature-limit-backoff"&&event["conditions"]["asicTempC"]==62&&event["measurement"].isNull());}
            assert(found);asicDegrees=50;
        } else {
            Config::cfgSetU16("asicfrequency",650);Config::cfgSetU16("asicvoltage",1200);hostBoard.loadSettings();
            POWER_MANAGEMENT_MODULE.appliedFrequency=650;POWER_MANAGEMENT_MODULE.appliedVoltage=1200;paused=true;
        }
        fanReadFail=true;
        if(scenario=="recovery-fault")SYSTEM_MODULE.error=Board::TEMP_FAULT;
        if(scenario=="recovery-hot")asicDegrees=72;
        if(scenario=="recovery-sensor")offSensorFail=true;
        if(scenario=="recovery-gpio")offGpioGood=false;
        if(scenario=="recovery-after-stop"){assert(response("{\"action\":\"stop\"}")["ok"]==true);step();assert(restoring&&recoveryResumeCalls==0);}
        if(scenario!="recovery-floor")assert(response("{\"action\":\"restore-and-resume\"}")["ok"]==true);
        if(scenario=="recovery-duplicate"){const unsigned calls=hostNvsCalls;assert(response("{\"action\":\"restore-and-resume\"}")["error"]=="transition-in-progress"&&calls==hostNvsCalls);}
        assert(snapshot()["status"]["active"]==false&&snapshot()["status"]["recovery"]["requested"]==true);
        if(scenario=="recovery-storage")storageFail=true;
        if(scenario=="recovery-readback")restoreReadbackFail=true;
        if(scenario=="recovery-policy")++hostPolicyRevision;
        if(scenario=="recovery-manual")Config::cfgSetU16("asicfrequency",700);
        if(scenario=="recovery-ota")externalHandoff("Firmware update took ownership");
        if(scenario=="recovery-stop")assert(response("{\"action\":\"stop\"}")["ok"]==true);
        if(scenario=="recovery-fan")fanRemainsOff=true;
        if(scenario=="recovery-resume-failure")resumeFail=true;
        if(scenario=="recovery-hot"||scenario=="recovery-sensor") {
            for(int i=0;i<4;++i){step();}
            assert(recoveryRequested&&recoveryResumeCalls==0&&paused);
            asicDegrees=50;offSensorFail=false;
        }
        for(int i=0;i<35;++i)step();
        const bool succeeds=scenario=="recovery-cold"||scenario=="recovery-after-stop"||scenario=="recovery-duplicate"||scenario=="recovery-hot"||scenario=="recovery-sensor"||scenario=="recovery-floor";
        auto result=snapshot();
        if(succeeds) {
            assert(result["status"]["recovery"]["phase"]=="complete"&&!recoveryRequested&&!paused&&recoveryResumeCalls==1);
            assert(result["backup"]["restored"]==true&&!controller.active&&!controller.owns&&!restoring);
            assert(POWER_MANAGEMENT_MODULE.appliedFrequency==600&&POWER_MANAGEMENT_MODULE.appliedVoltage==1150);
            assert(Config::cfgGetU16("asicfrequency")==600&&Config::cfgGetU16("asicvoltage")==1150);
            if(scenario=="recovery-floor") {bool found=false;for(auto event:result["status"]["events"].as<JsonArrayConst>())if(event["kind"]=="rejected"&&event["outcome"]=="failed")found=true;assert(found);}
        } else {
            assert(!recoveryRequested&&result["backup"]["restored"]==false&&paused);
            assert(recoveryResumeCalls==((scenario=="recovery-fan"||scenario=="recovery-resume-failure")?1U:0U));
            if(scenario=="recovery-manual")assert(Config::cfgGetU16("asicfrequency")==700);
            if(scenario=="recovery-fault")assert(SYSTEM_MODULE.error==Board::TEMP_FAULT);
        }
        assert(nvstore["fwtune/baseline"]==baselineRecord&&!preserveRecoveryFaults());
        return 0;
    } else if(scenario.rfind("events-",0)==0) {
        using namespace FiveTratumAutotune;
        const char *configuration=R"({"action":"configure","config":{"priority":"efficiency","continuous":false,"minFrequencyMHz":590,"maxFrequencyMHz":600,"minVoltageMv":1145,"maxVoltageMv":1150,"maxAsicTempC":65,"maxVrmTempC":80,"maxPowerW":200}})";
        assert(response(configuration)["ok"]==true);start();
        auto first=snapshot();assert(first["status"]["eventCapacity"]==8&&first["status"]["events"].size()==1);
        auto began=first["status"]["events"][0];assert(began["kind"]=="started"&&began["from"].isNull()&&began["point"]["frequencyMHz"]==600&&began["measurement"].isNull());
        if(scenario=="events-failed")voltageFail=true;
        bool found=false,baseline=false,applied=false;
        uint32_t maxId=0;
        for(unsigned count=0;count<1300;++count) {
            if(controller.trial && !pending && controller.state==FWT_SETTLING) {
                watts=scenario=="events-accepted"?95:105;
                if(scenario=="events-errors")chipErrorWork=25;
            }
            step();auto d=snapshot();const auto events=d["status"]["events"].as<JsonArrayConst>();
            assert(events.size()<=8);uint32_t prior=0;
            for(JsonObjectConst event:events) {
                const uint32_t id=event["id"];assert(id>prior);prior=id;maxId=std::max(maxId,id);
                assert(event["atSeconds"].as<double>()>0&&event["point"]["frequencyMHz"].as<double>()>0&&event["point"]["coreVoltageMv"].as<double>()>0);
                if(event["kind"]=="baseline") {baseline=true;assert(event["measurement"]["hashRateGHs"].as<double>()>0&&event["measurement"]["powerW"]==100);}
                if(event["kind"]=="adjustment"&&event["outcome"]=="applied")applied=true;
                if((scenario=="events-accepted"&&event["kind"]=="accepted") || (scenario=="events-rejected"&&event["kind"]=="rejected") ||
                   (scenario=="events-errors"&&event["kind"]=="errors") || (scenario=="events-failed"&&event["outcome"]=="failed")) {
                    found=true;
                    if(event["kind"]=="accepted")assert(event["score"].as<double>()>event["requiredScore"].as<double>());
                    if(event["kind"]=="rejected")assert(event["score"].as<double>()<=event["requiredScore"].as<double>());
                    if(event["kind"]=="errors")assert(event["measurement"]["worstErrorPercentage"].as<double>()>1);
                }
            }
            if(found && (scenario!="events-accepted"||events.size()==8))break;
        }
        assert(found&&baseline&&(scenario=="events-failed"||applied));
        if(scenario=="events-accepted")assert(maxId>8&&fwt_history_count(&history)==8);
        if(scenario=="events-failed") {
            unsigned failures=0;auto d=snapshot();for(JsonObjectConst event:d["status"]["events"].as<JsonArrayConst>())if(event["outcome"]=="failed")++failures;
            assert(failures==1); // Requested adjustment is updated in place, not duplicated.
        }
        const unsigned nvs=hostNvsCalls,sensors=hostSensorReads;const auto samples=controller.samples;const auto sequence=controller.eventSequence;
        std::string wire;serializeJson(snapshot()["status"]["events"],wire);
        for(unsigned read=0;read<12;++read) {HostStackContext external(false);std::string repeated;serializeJson(snapshot()["status"]["events"],repeated);assert(wire==repeated);DisplaySnapshot view;assert(tryDisplaySnapshot(view)&&view.lastKind!=FWT_EVENT_NONE&&view.lastLabel);}
        assert(hostNvsCalls==nvs&&hostSensorReads==sensors&&controller.samples==samples&&controller.eventSequence==sequence);
        boot();auto rebooted=snapshot();assert(rebooted["status"]["events"].size()==0&&!rebooted["status"]["active"].as<bool>()); // RAM-only history, no automatic resume.
        return 0;
    } else if(scenario=="fault-projection") {
        using namespace FiveTratumAutotune;
        controller.state=FWT_FAULTED;controller.active=controller.owns=false;controller.reason="cannot-reduce-load-within-limits";
        JsonDocument passive;DisplaySnapshot display;
        assert(activity(passive)&&tryDisplaySnapshot(display));
        assert(passive["state"]=="faulted"&&passive["fault"]==false&&!display.fault&&snapshot()["status"]["telemetry"]["fault"]==false);
        SYSTEM_MODULE.error=Board::TEMP_FAULT;step();
        assert(activity(passive)&&tryDisplaySnapshot(display));
        assert(passive["fault"]==true&&display.fault&&!display.valid&&snapshot()["status"]["telemetry"]["fault"]==true);
        return 0;
    } else if(scenario=="passive-activity") {
        using namespace FiveTratumAutotune;
        const unsigned nvs=hostNvsCalls,sensors=hostSensorReads;
        const auto heartbeat=lastOwnerTickUs;
        auto read=[]() {httpd_req_t req;assert(GET_five_tratum_autotune_activity(&req)==ESP_OK);JsonDocument d;assert(deserializeJson(d,req.response)==DeserializationError::Ok);return d;};
        {HostStackContext external(false);auto d=read();assert(d["available"]==true&&d["supported"]==true&&d["active"]==false&&d["controllerAgeSeconds"]==0);}
        controller.active=controller.owns=true;controller.state=FWT_MEASURING;controller.config.priority=FWT_PERFORMANCE;controller.config.continuous=true;
        hostNow+=5000000;
        for(unsigned i=0;i<12;++i) {HostStackContext external(false);auto d=read();assert(d["active"]==true&&d["ownsSettings"]==true&&d["state"]=="measuring"&&d["priority"]=="performance"&&d["continuous"]==true&&d["controllerAgeSeconds"]==5);}
        assert(lastOwnerTickUs==heartbeat);
        hostNow+=11000000;auto stale=read();assert(stale["controllerAgeSeconds"].as<double>()==16); // Polling cannot renew controller freshness.
        auto reused=read();pthread_mutex_lock(&mutex);auto busy=read();assert(activity(reused));pthread_mutex_unlock(&mutex);
        assert(busy["schemaVersion"]==1&&busy["available"]==false&&busy.size()==2&&reused.size()==2&&reused["available"]==false);
        observed.fault=true;auto faulted=read();assert(faulted["fault"]==true&&faulted["state"]=="measuring");observed.fault=false;
        controller.state=FWT_FAULTED;assert(read()["fault"]==false);controller.state=FWT_MEASURING;
        initialized=false;auto cold=read();assert(cold["supported"]==false&&!initialized);initialized=true;
        lastOwnerTickUs=hostNow+1;assert(read()["controllerAgeSeconds"].isNull());lastOwnerTickUs=heartbeat;
        controller.active=false;controller.owns=false;restoring=true;auto restore=read();assert(restore["active"]==false&&restore["ownsSettings"]==true&&restore["state"]=="restoring");restoring=false;
        controller.active=controller.owns=true;externalHandoff("Manual control test");
        auto fullRevoked=snapshot();assert(fullRevoked["status"]["active"]==false&&fullRevoked["status"]["ownsSettings"]==false);
        auto revoked=read();assert(revoked["active"]==false&&revoked["ownsSettings"]==false&&revoked["reason"]=="Manual control test");handoff.store(nullptr);
        firmwareUpdates=1;assert(read()["active"]==false);firmwareUpdates=0;
        otpAllowed=false;assert(read()["available"]==true);otpAllowed=true; // Read only, never an OTP control request.
        networkAllowed=false;httpd_req_t denied;assert(GET_five_tratum_autotune_activity(&denied)==ESP_FAIL&&denied.response.empty());networkAllowed=true;
        assert(hostNvsCalls==nvs&&hostSensorReads==sensors&&writes.empty());
        return 0;
    } else if(scenario=="search-display") {
        using namespace FiveTratumAutotune;
        const unsigned nvs=hostNvsCalls,sensorReads=hostSensorReads;
        assert(initial["status"]["search"]["minimumSamples"]==8 && initial["status"]["search"]["roundExperimentLimit"]==24);
        assert(initial["status"]["search"]["phaseElapsedSeconds"].isNull() && initial["status"]["search"]["reference"].isNull());
        controller.state=FWT_BASELINE;controller.owns=controller.active=true;
        controller.config.priority=FWT_PERFORMANCE;controller.config.continuous=true;
        controller.observationStartedUs=hostNow-12000000;controller.samples=6;
        controller.roundExperiments=3;controller.rejected=2;controller.bestAvailable=true;
        controller.baselineHash=4000;controller.baselinePower=100;controller.bestHash=4100;controller.bestPower=99;
        DisplaySnapshot view;
        {HostStackContext external(false);assert(tryDisplaySnapshot(view));}
        assert(sizeof(view)<=256 && view.available && view.fresh(hostNow) && view.priority==FWT_PERFORMANCE && view.continuous);
        assert(view.current.frequencyMHz==600 && view.search.timed && view.search.phaseElapsedSeconds==12 && view.search.minimumSeconds==75 && view.search.samples==6);
        auto report=snapshot();auto search=report["status"]["search"];
        assert(search["phaseElapsedSeconds"].as<double>()==12 && search["minimumSeconds"]==75);
        assert(search["roundExperiments"]==3 && search["rejectedWindows"]==2 && search["reference"]["hashRateGHs"]==4000 && search["best"]["powerW"]==99);
        pthread_mutex_lock(&mutex);
        assert(!tryDisplaySnapshot(view) && !view.available && view.current.frequencyMHz==0 && !view.valid);
        pthread_mutex_unlock(&mutex);
        observed.fault=true;controller.active=false;controller.owns=false;controller.state=FWT_DISABLED;
        assert(tryDisplaySnapshot(view) && view.fault && !view.valid && view.current.frequencyMHz==0 && view.hashRateGHs==0); // Inactive faults remain visible, old readings do not.
        observed.fault=false;firmwareUpdates.store(1);
        assert(tryDisplaySnapshot(view) && view.updating && !view.valid && view.current.frequencyMHz==0);
        report=snapshot();assert(report["status"]["search"]["reference"].isNull() && report["status"]["search"]["best"].isNull());
        firmwareUpdates.store(0);hostNow+=16000000;
        assert(tryDisplaySnapshot(view) && !view.valid && !view.fresh(hostNow) && view.current.frequencyMHz==0 && view.powerW==0);
        report=snapshot();assert(!report["status"]["telemetry"]["valid"].as<bool>());
        controller.bestHash=NAN;controller.baselinePower=0;
        report=snapshot();assert(report["status"]["search"]["reference"].isNull() && report["status"]["search"]["best"].isNull());
        controller.limits.maxFrequencyMHz=0;
        assert(!tryDisplaySnapshot(view) && !view.available && view.current.coreVoltageMv==0);
        assert(hostNvsCalls==nvs && hostSensorReads==sensorReads && writes.empty());
        return 0;
    } else if(scenario=="storage-bootstrap-context") {
        using namespace FiveTratumAutotune;
        initialized=false;storagePrepared=false;powerInitialized=false;optionsStatus=BlobStatus::Unloaded;baselineStatus=BlobStatus::Unloaded;
        const unsigned calls=hostNvsCalls;
        {
            HostStackContext external(false);
            assert(!preloadStorage());powerReady();tick();auto waiting=snapshot();
            assert(!waiting["supported"].as<bool>()&&waiting["status"]["reason"].is<const char *>());
            assert(!waiting["diagnostics"]["storagePreloaded"].as<bool>()&&hostNvsCalls==calls);
        }
        assert(preloadStorage());const unsigned loaded=hostNvsCalls;
        {HostStackContext external(false);powerReady();tick();assert(snapshot()["supported"].as<bool>());}
        assert(hostNvsCalls==loaded&&!snapshot()["status"]["active"].as<bool>()&&!paused);
    } else if(scenario=="restore-lock-order") {
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
        if(scenario.rfind("boot-",0)==0) {
            JsonDocument config;config["action"]="configure";config["config"].set(snapshot()["config"]);config["config"]["priority"]="efficiency";config["config"]["maxAsicTempC"]=65;config["config"]["maxVrmTempC"]=80;
            std::string text;serializeJson(config,text);assert(response(text.c_str())["ok"].as<bool>());
            const std::string originalBackup=nvstore["fwtune/baseline"];
            if(scenario=="boot-missing-records"){nvstore.erase("fwtune/baseline");nvstore.erase("fwtune/options");}
            if(scenario=="boot-corrupt-options")nvstore["fwtune/options"]="{bad";
            if(scenario=="boot-corrupt-baseline")nvstore["fwtune/baseline"]="{bad";
            if(scenario=="boot-read-failure")hostReadFailure=true;
            if(scenario=="boot-memory-failure")hostBlobAllocationFail=true;
            boot();hostReadFailure=false;hostBlobAllocationFail=false;
            const unsigned loaded=hostNvsCalls;
            for(int i=0;i<5;++i){step();HostStackContext external(false);auto report=snapshot();assert(report["status"]["reason"].is<const char *>());}
            assert(hostNvsCalls==loaded&&!paused&&writes.empty());
            auto report=snapshot();assert(!report["status"]["active"].as<bool>()&&!report["status"]["ownsSettings"].as<bool>());
            const bool bad=scenario=="boot-corrupt-options"||scenario=="boot-corrupt-baseline"||scenario=="boot-read-failure"||scenario=="boot-memory-failure";
            if(scenario=="boot-present-records")assert(report["config"]["maxAsicTempC"]==65&&report["config"]["maxVrmTempC"]==80&&report["config"]["priority"]=="efficiency"&&report["backup"]["available"].as<bool>()&&nvstore["fwtune/baseline"]==originalBackup);
            if(scenario=="boot-missing-records")assert(report["diagnostics"]["optionsStorage"]=="missing"&&report["diagnostics"]["baselineStorage"]=="missing"&&!report["backup"]["available"].as<bool>());
            if(scenario=="boot-corrupt-options")assert(report["diagnostics"]["optionsStorage"]=="invalid"&&report["backup"]["available"].as<bool>());
            if(scenario=="boot-corrupt-baseline")assert(report["diagnostics"]["baselineStorage"]=="invalid"&&!report["backup"]["available"].as<bool>());
            if(scenario=="boot-read-failure"||scenario=="boot-memory-failure")assert(report["diagnostics"]["optionsStorage"]=="read-failure"&&report["diagnostics"]["baselineStorage"]=="read-failure");
            if(bad){assert(!response("{\"action\":\"start\",\"waiverAccepted\":true,\"waiverVersion\":1}")["ok"].as<bool>());assert(hostNvsCalls==loaded);}
            if(scenario=="boot-corrupt-options"){
                assert(response("{\"action\":\"stop\"}")["ok"].as<bool>());for(int i=0;i<8;++i)step();assert(snapshot()["backup"]["restored"].as<bool>());
            }
            return 0;
        }
        if(scenario=="wrong-context-actions") {
            JsonDocument retained;retained["sentinel"]=7;JsonDocument original;assert(FiveTratumAutotune::captureEffective(&hostBoard,original.to<JsonObject>()));
            const unsigned calls=hostNvsCalls;const auto revision=Config::tuningMutationRevision();
            {
                HostStackContext external(false);
                assert(!response("{\"action\":\"start\",\"waiverAccepted\":true,\"waiverVersion\":1}")["ok"].as<bool>());
                assert(!response("{\"action\":\"stop\"}")["ok"].as<bool>());
                assert(!response("{\"action\":\"configure\",\"config\":{}}")["ok"].as<bool>());
                assert(FiveTratumAutotune::readBlobState("baseline",retained)==FiveTratumAutotune::BlobStatus::ReadFailure&&retained["sentinel"]==7);
                assert(!FiveTratumAutotune::writeVerified("baseline",retained));
                assert(!Config::restoreTuningSettings(original.as<JsonObjectConst>()));
                assert(snapshot()["status"]["ownsSettings"].as<bool>());
            }
            assert(hostNvsCalls==calls&&Config::tuningMutationRevision()==revision&&writes.empty());return 0;
        }
        if(scenario=="worker-low-before"||scenario=="worker-low-after-commit"||scenario=="worker-wrong-context") {
            Config::cfgSetU16("pid_temp",67);hostBoard.loadSettings();step();
            if(scenario=="worker-low-before")workerFreeBytes=512;
            if(scenario=="worker-low-after-commit")lowAfterConfigCommit=true;
            if(scenario=="worker-wrong-context")hostWorkerExternal=true;
            assert(response("{\"action\":\"stop\"}")["ok"].as<bool>());assert(workerRequestedBytes==8192);
            step();step();step();
            assert(snapshot()["status"]["ownsSettings"].as<bool>()&&snapshot()["status"]["state"]=="restoring"&&!snapshot()["backup"]["restored"].as<bool>()&&writes.empty());
            assert(Config::s_doc["pid_temp"].as<int>()==(scenario=="worker-low-after-commit"?55:67));
            if(scenario!="worker-wrong-context")assert(snapshot()["diagnostics"]["restoreWorkerStackLow"].as<bool>()&&snapshot()["diagnostics"]["restoreWorkerMinFreeBytes"]==512);
            if(scenario=="worker-low-after-commit")assert(FiveTratumAutotune::revisionCurrent());
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
            document["config"]["maxAsicTempC"]=55;
            std::string body;serializeJson(document,body);assert(!response(body.c_str())["ok"].as<bool>());
            assert(snapshot()["config"]["maxAsicTempC"].as<double>()==60);
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
            if(scenario=="restore-worker-failure"){FiveTratumAutotune::restoreWorker=nullptr;workerCreateFail=true;}
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
