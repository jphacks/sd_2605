#include "api_stubs/HostApi.h"
#include "../../firmware/m5stack/cores3/WavValidation.h"
#include "../../firmware/m5stack/cores3/ApiClient.h"
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
 VoiceReply::eventId="reply-test"; VoiceReply::slot=1;
 VoiceReply::occurredAt=time(nullptr);
 std::vector<uint8_t> recording(VoiceReply::bytes);
 VoiceReply::wav=recording.data(); VoiceReply::state.store(VoiceReply::READY);
 LittleFS.open("/voice-reply.bin",FILE_WRITE);
 replies[url("events")]={503,""}; api.start();
 assert(VoiceReply::state.load()==VoiceReply::READY);
 assert(calls[url("events/reply-test/voice")]==0);
 replies[url("events")]={201,"{}"};
 replies[url("events/reply-test/voice")]={503,""}; api.start();
 assert(VoiceReply::state.load()==VoiceReply::READY);
 assert(LittleFS.exists("/voice-reply.bin"));
 replies[url("events")]={200,"{}"};
 replies[url("events/reply-test/voice")]={201,"{}"}; api.start();
 assert(VoiceReply::state.load()==VoiceReply::IDLE);
 assert(!LittleFS.exists("/voice-reply.bin"));
 assert(calls[url("events/reply-test/voice")]==2);
 JsonDocument submitted;
 assert(!deserializeJson(submitted,postedBodies[url("events")]));
 assert(submitted["event_id"].as<std::string>()=="reply-test");
 assert(submitted["event_type"].as<std::string>()=="left_home");
 assert(contentTypes[url("events/reply-test/voice")]=="audio/wav");
 static_assert(VoiceReply::durationMs==10000 && VoiceReply::bytes==320044);
 VoiceReply::pendingBytes=96044; // Pending replies from the old firmware remain sendable.
 VoiceReply::state.store(VoiceReply::READY); api.start();
 assert(VoiceReply::state.load()==VoiceReply::IDLE);
 VoiceReply::pendingBytes=32044;
 VoiceReply::state.store(VoiceReply::READY); api.start();
 assert(postedSizes[url("events/reply-test/voice")]==32044);
 int uploads=calls[url("events/reply-test/voice")];
 VoiceReply::pendingBytes=0;
 VoiceReply::state.store(VoiceReply::READY); api.start();
 assert(VoiceReply::state.load()==VoiceReply::IDLE);
 assert(calls[url("events/reply-test/voice")]==uploads);
 assert(!deserializeJson(submitted,postedBodies[url("events")]));
 assert(submitted["voice"].isNull()); // No voice attachment or skipped-recording mail.
 puts("ApiClient: existing sync/event regressions and voice event failure, upload retry, acknowledgement cleanup passed");
 return 0;
}
