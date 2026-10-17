// Reuses the CoreS3 API regression scenarios and host mocks against the Atom headers.
#include "../cores3/api_stubs/HostApi.h"
#include "../../firmware/m5stack/atoms3r-cam/WavValidation.h"
#include <time.h>
time_t atomApiClock = 0;
time_t atomApiTime(time_t*) { return atomApiClock; }
#define time atomApiTime
#include "../../firmware/m5stack/atoms3r-cam/ApiClient.h"
#undef time
std::string url(const char* tail) { return std::string(Config::baseUrl)+"/api/devices/entrance-1/"+tail; }
std::string wav() {
 std::string b(48,'\0');
 auto put=[&](int at,uint32_t n){for(int i=0;i<4;++i)b[at+i]=char(n>>(i*8));};
 b.replace(0,4,"RIFF");put(4,40);b.replace(8,8,"WAVEfmt ");put(16,16);
 b[20]=1;b[22]=1;put(24,16000);put(28,32000);b[32]=2;b[34]=16;
 b.replace(36,4,"data");put(40,4);return b;
}
std::string heartbeat(int arrive,int leave) {
 return std::string(R"({"device_id":"entrance-1","leave_greeting_enabled":true,"arrive_greeting_enabled":true,"volume":70,"arrive_audio":{"version":)")+
 std::to_string(arrive)+R"(,"byte_size":48,"content_type":"audio/wav","path":"/api/devices/entrance-1/audio/arrive"},"leave_audio":{"version":)"+
 std::to_string(leave)+R"(,"byte_size":48,"content_type":"audio/wav","path":"/api/devices/entrance-1/audio/leave"}})";
}
int main() {
 Storage storage; EventManager events; ApiClient api(storage,events);
 replies[url("heartbeat")]={200,heartbeat(1,1)};
 replies[url("audio/arrive")]={200,wav(),48};
 replies[url("audio/leave")]={200,wav(),48};
 api.start(); // HTTPS must wait for a usable clock, preserving the pending sync request.
 assert(calls.empty());
 atomApiClock=1792215000;
 api.start(); // fake scheduler runs one actual ApiClient task iteration
 assert(api.code()==200);
 assert(storage.records[0].version==1 && storage.records[1].version==1);
 assert(calls[url("audio/arrive")]==1 && calls[url("audio/leave")]==1);
 api.start();
 assert(calls[url("audio/arrive")]==1 && calls[url("audio/leave")]==1);
 replies[url("heartbeat")].body=heartbeat(1,2);
 replies[url("audio/leave")].status=503;
 api.start(); assert(storage.records[1].version==1);
 assert(calls[url("audio/arrive")]==1);
 auto old=LittleFS.files[storage.path(1,storage.records[1].bank)];
 replies[url("audio/leave")]={200,wav(),48,20};
 api.start(); assert(storage.records[1].version==1);
 assert(std::string(old->begin(),old->end())==wav());
 replies[url("audio/leave")]={200,std::string(48,'x'),48};
 api.start(); assert(storage.records[1].version==1);
 replies[url("audio/leave")]={200,wav(),99};
 api.start(); assert(storage.records[1].version==1);
 replies[url("audio/leave")]={200,wav(),48};
 storage.failCommit=true;
 api.start(); assert(storage.records[1].version==1);
 assert(std::string(old->begin(),old->end())==wav());
 storage.failCommit=false;
 api.start(); assert(storage.records[1].version==2);
 auto committed=storage.records[1];
 Storage restored=storage;
 ApiClient restarted(restored,events); restarted.start();
 assert(restored.records[1].version==2 && restored.records[1].bank==committed.bank);
 File f=LittleFS.open(storage.path(1,committed.bank),FILE_READ); assert(validWav(f));
 // An older response must not replace a valid newer local WAV.
 int leaveCalls=calls[url("audio/leave")];
 replies[url("heartbeat")].body=heartbeat(1,1);
 api.start();
 assert(storage.records[1].version==2 && calls[url("audio/leave")]==leaveCalls);
 // A corrupt cache must be repaired even when its version matches the API.
 auto active=storage.path(1,storage.records[1].bank);
 LittleFS.files[active]=std::make_shared<std::vector<uint8_t>>(48,'x');
 replies[url("heartbeat")].body=heartbeat(1,2);
 api.start();
 assert(storage.records[1].version==2 && calls[url("audio/leave")]==leaveCalls+1);
 File repaired=LittleFS.open(storage.path(1,storage.records[1].bank),FILE_READ);
 assert(validWav(repaired));
 replies[url("heartbeat")].body=heartbeat(1,2);
 auto& bad=replies[url("heartbeat")].body;
 bad.replace(bad.find("audio/wav"),9,"audio/mp3");
 api.start(); assert(api.code()==-100 && storage.records[1].version==2);
 replies[url("heartbeat")].body="<html>not JSON</html>";
 api.start(); assert(api.code()==-100 && storage.records[1].version==2);
 replies[url("heartbeat")]={404,""};
 api.start(); assert(api.code()==404 && storage.records[1].version==2);
 replies[url("heartbeat")]={200,heartbeat(1,2)};
 events.pending="{event contract payload}";
 replies[url("events")]={503,""}; api.start(); assert(!events.pending.empty());
 replies[url("events")]={201,"{}"}; api.start(); assert(events.pending.empty());
 puts("ApiClient: JSON parsing, first sync, same-version skip, per-slot update, HTTP error, truncated/invalid WAV, length mismatch, failed commit, restart, events retry, NTP gate, rollback rejection, corrupt cache repair passed");
 return 0;
}
