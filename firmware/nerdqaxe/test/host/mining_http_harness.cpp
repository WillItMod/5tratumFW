#include "http_utils.h"
#include "handler_mining.h"
#include "tasks/mining_schedule.h"
#include "tasks/mining_control.h"
#include "nvs.h"
#include "sntp.h"
#include <cassert>
#include <cstring>
#include <iostream>
#include <string>

httpd_handle_t http_server=0; System SYSTEM_MODULE; SNTP sntp;
static std::string flash, pending; static int writes=0; static bool supported=true, fail=false;
int nvs_open(const char *,int mode,nvs_handle_t *h){*h=1;return mode==NVS_READONLY && flash.empty()?ESP_ERR_NVS_NOT_FOUND:ESP_OK;}
int nvs_get_blob(nvs_handle_t,const char *,void *out,size_t *len){if(flash.empty())return ESP_ERR_NVS_NOT_FOUND;if(out)memcpy(out,flash.data(),flash.size());*len=flash.size();return ESP_OK;}
int nvs_set_blob(nvs_handle_t,const char *,const void *data,size_t len){pending.assign(static_cast<const char *>(data),len);return ESP_OK;}
int nvs_commit(nvs_handle_t){if(fail)return -7;flash=pending;++writes;return ESP_OK;}
void nvs_close(nvs_handle_t){pending.clear();}
namespace FiveTratumMining {
bool controlSupported(){return supported;}
bool writeControlReport(JsonDocument &d){d["supported"]=supported;d["status"]["appliedPaused"]=true;d["status"]["transitionPending"]=!requestedPaused();return true;}
}
int main(){
    using namespace FiveTratumMining;
    assert(initializeSchedule());
    auto call=[](auto fn,const char *text="",bool auth=true,bool otp=true){httpd_req_t r;r.body=text;r.content_len=r.body.size();r.authorized=auth;r.otp=otp;fn(&r);return r;};
    for(auto fn:{PUT_mining_schedule,POST_mining_pause,POST_mining_resume,POST_mining_schedule_override}) {
        auto r=call(fn,"{}",false);assert(r.status=="401" && writes==0);
        r=call(fn,"{}",true,false);assert(r.status=="401" && writes==0);
    }
    auto r=call(GET_mining_schedule);JsonDocument report;assert(!deserializeJson(report,r.response));
    assert(report["schemaVersion"].is<unsigned>() && report["schemaVersion"]==1);
    assert(report["identity"]["deviceId"]=="5tfw:0102030405060708090a0b0c0d0e0f10");
    assert(report["hardware"]["asicCount"]==4 && report["firmware"]["product"]=="5tratumFW");
    assert(report["supported"]==true && report["status"]["appliedPaused"]==true && report["status"]["transitionPending"]==true);
    assert(report.size()==8 && report["schedule"].size()==3 && report["status"].size()==8);
    assert(report["limits"].size()==2 && report["limits"]["maxWindows"]==8 && report["limits"]["timezones"].size()==10);
    supported=false;r=call(POST_mining_pause);assert(r.status=="409 Conflict" && !requestedPaused());
    r=call(PUT_mining_schedule,R"({"enabled":false,"timezone":"UTC","windows":[]})");assert(r.status=="409 Conflict" && writes==0);supported=true;
    r=call(POST_mining_pause);assert(r.status=="200 OK" && requestedPaused());
    r=call(POST_mining_resume,"{}");assert(r.status=="200 OK" && !requestedPaused());
    r=call(POST_mining_resume,R"({"asicIndex":0})");assert(r.status=="400 Bad Request");
    r=call(POST_mining_pause,"[]");assert(r.status=="400 Bad Request" && !requestedPaused());
    r=call(POST_mining_schedule_override,R"({"mode":"resume"})");assert(r.status=="400 Bad Request");
    r=call(POST_mining_schedule_override,R"({"mode":"schedule"})");assert(r.status=="200 OK");
    const char *schedule=R"({"enabled":true,"timezone":"Europe/London","windows":[{"days":127,"start":"22:00","end":"06:00"}]})";
    fail=true;r=call(PUT_mining_schedule,schedule);assert(r.status=="507 Insufficient Storage" && writes==0 && !requestedPaused());fail=false;
    r=call(PUT_mining_schedule,schedule);assert(r.status=="200 OK" && writes==1 && requestedPaused()); // No trusted NTP.
    assert(!deserializeJson(report,r.response));
    assert(report["schemaVersion"]==1 && report["supported"]==true && report["identity"]["deviceId"]=="5tfw:0102030405060708090a0b0c0d0e0f10");
    assert(report["firmware"]["product"]=="5tratumFW" && report["hardware"]["asicCount"]==4);
    assert(report["schedule"]["enabled"]==true && report["schedule"]["timezone"]=="Europe/London" && report["schedule"]["windows"].size()==1);
    assert(report["schedule"]["windows"][0]["days"]==127 && report["schedule"]["windows"][0]["start"]=="22:00" && report["schedule"]["windows"][0]["end"]=="06:00");
    assert(report["status"]["clockValid"]==false && report["status"]["localTime"].isNull() && report["status"]["requestedPaused"]==true && report["status"]["appliedPaused"]==true);
    assert(report["limits"]["maxWindows"]==8 && report["limits"]["timezones"].size()==10 && report["message"].isNull());
    std::cout<<"POWER_SNAPSHOT "<<r.response<<"\n";
    r=call(GET_mining_schedule);assert(!deserializeJson(report,r.response));assert(report["schedule"]["timezone"]=="Europe/London" && report["limits"]["maxWindows"]==8);
    r=call(PUT_mining_schedule,R"({"enabled":true,"timezone":"UTC","windows":[]})");assert(r.status=="400 Bad Request" && writes==1);
    httpd_req_t huge;huge.content_len=2049;huge.body="{}";PUT_mining_schedule(&huge);assert(huge.status=="400 Bad Request" && writes==1);
    std::cout<<"Actual power HTTP handlers passed identity, OTP/auth, bounds, unsupported targets, persistence failure and requested/applied distinction checks\n";
}
