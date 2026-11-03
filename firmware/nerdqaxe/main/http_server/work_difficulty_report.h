#pragma once
#include <ArduinoJson.h>
#include "tasks/work_difficulty.h"
namespace FiveTratumWork {
inline bool report(JsonDocument &doc,const Snapshot &snapshot) {
    doc.clear();doc["schemaVersion"]=1;doc["normalization"]="bitcoin-diff1";
    auto pools=doc["pools"].to<JsonArray>();
    for(unsigned index=0;index<2;++index) {
        const auto &source=snapshot.pools[index];const auto &work=source.work;
        const bool valid=source.connected&&source.selected&&work.available&&work.receivedAtUs&&snapshot.nowUs>=work.receivedAtUs&&
            std::isfinite(work.shareDifficulty)&&work.shareDifficulty>0&&std::isfinite(work.networkDifficulty)&&work.networkDifficulty>0;
        auto row=pools.add<JsonObject>();row["index"]=index;row["selected"]=source.selected;row["connected"]=source.connected;
        row["available"]=valid;row["source"]="active-stratum-job";
        if(valid) {row["shareDifficulty"]=work.shareDifficulty;row["networkDifficulty"]=work.networkDifficulty;row["jobAgeSeconds"]=(snapshot.nowUs-work.receivedAtUs)/1000000.0;}
        else {row["shareDifficulty"]=nullptr;row["networkDifficulty"]=nullptr;row["jobAgeSeconds"]=nullptr;}
    }
    return !doc.overflowed();
}
}
