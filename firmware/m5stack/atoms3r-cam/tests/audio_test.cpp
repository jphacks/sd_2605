#include "../../../../tests/cores3/api_stubs/HostApi.h"
struct SpeakerMock {
 struct Pins { int pin_bck=8, pin_ws=6, pin_data_out=5, i2s_port=0; };
 bool playing=false;
 uint8_t volume=0;
 std::vector<uint8_t> played;
 void* owner=nullptr;
 void (*callback)(void*,const void*,uint8_t)=nullptr;
 Pins config() { return {}; }
 bool begin() { return true; }
 bool isEnabled() { return true; }
 bool isPlaying() { return playing; }
 void setVolume(uint8_t v) { volume=v; }
 void setBufferReleaseCallback(void* p,void(*f)(void*,const void*,uint8_t)) { owner=p;callback=f; }
 bool tone(int,int,int) { return true; }
 bool playWav(const uint8_t* p,size_t n,int,int) { played.assign(p,p+n);playing=true;return true; }
 void finish() { playing=false;callback(owner,nullptr,0); }
};
struct { struct { void end() {} } Mic; SpeakerMock Speaker; } M5;
#include "../AudioManager.h"
#include "../LocalAudioTest.h"
int main() {
 Storage s;
 assert(createTestCache(s));
 AudioManager a(s); a.begin();
 assert(!a.replay());
 assert(a.play(1)); const auto first=M5.Speaker.played; const auto volume=M5.Speaker.volume;
 assert(!a.play(0));
 M5.Speaker.finish();a.update();
 // Cache replacement and a failed play must preserve exact previous bytes for replay.
 LittleFS.files.erase(s.path(1,s.records[1].bank));
 assert(!a.play(1));s.settings.volume=1;
 assert(a.replay());assert(M5.Speaker.played==first && M5.Speaker.volume==volume);
 M5.Speaker.finish();a.update();assert(a.play(0));assert(M5.Speaker.played!=first);
 M5.Speaker.finish();
 puts("local WAV/replay tests passed");
}
