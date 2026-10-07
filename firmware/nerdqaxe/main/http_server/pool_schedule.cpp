#include "pool_schedule.h"
namespace FiveTratumProfiles {
bool validPoolSchedule(JsonObjectConst s) {
    if (s.size()!=4 || !s["schemaVersion"].is<unsigned>() || s["schemaVersion"].as<unsigned>()!=1 || !s["enabled"].is<bool>() || !s["utcOffsetMinutes"].is<int>() || s["utcOffsetMinutes"].is<bool>() || !s["events"].is<JsonArrayConst>()) return false;
    int offset=s["utcOffsetMinutes"].as<int>(); if(offset < -720 || offset>840 || offset%15) return false;
    auto events=s["events"].as<JsonArrayConst>(); if(events.size()>MAX_POOL_EVENTS) return false;
    for (size_t i=0;i<events.size();++i) {
        JsonObjectConst event=events[i];
        if (event.size()!=4 || !event["enabled"].is<bool>() || event["dayMask"].is<bool>() || !event["dayMask"].is<unsigned>() || event["dayMask"].as<unsigned>()<1 || event["dayMask"].as<unsigned>()>127 || event["timeMinutes"].is<bool>() || !event["timeMinutes"].is<unsigned>() || event["timeMinutes"].as<unsigned>()>1439 || event["slot"].is<bool>() || !event["slot"].is<unsigned>() || event["slot"].as<unsigned>()>=SLOT_COUNT) return false;
        // Reject simultaneous overlapping points; otherwise array order silently changes the selected pool.
        for (size_t j=0;j<events.size();++j) {
            JsonObjectConst other=events[j];
            if (i==j || !event["enabled"].as<bool>() || !other["enabled"].as<bool>()) continue;
            if (event["timeMinutes"].as<unsigned>()==other["timeMinutes"].as<unsigned>() && (event["dayMask"].as<unsigned>() & other["dayMask"].as<unsigned>())) return false;
        }
    }
    return true;
}
int selectedScheduledSlot(JsonObjectConst s,int day,int minute) {
    if (!validPoolSchedule(s) || !s["enabled"].as<bool>() || day<0 || day>6 || minute<0 || minute>1439) return -1;
    const int now=day*1440+minute;int smallestAge=10081,slot=-1;
    for (JsonObjectConst event:s["events"].as<JsonArrayConst>()) {
        if (!event["enabled"].as<bool>()) continue;
        unsigned mask=event["dayMask"];for(int d=0;d<7;++d) if(mask & (1u<<d)) {
            int age=(now-(d*1440+event["timeMinutes"].as<int>())+10080)%10080;
            if(age<smallestAge) {smallestAge=age;slot=event["slot"].as<int>();}
        }
    }
    return slot;
}
bool scheduleReferences(JsonObjectConst s,unsigned slot) {
    if (!validPoolSchedule(s)) return false;
    // Disabled schedules retain their profile references too, so re-enabling cannot produce a dangling slot.
    for (JsonObjectConst event:s["events"].as<JsonArrayConst>()) if(event["slot"].as<unsigned>()==slot) return true;
    return false;
}
}
