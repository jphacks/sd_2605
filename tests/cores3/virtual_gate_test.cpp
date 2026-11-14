#include <cassert>
#include <cstdio>
#include <initializer_list>
#include "../../firmware/m5stack/cores3/MotionDirectionTracker.h"
int main() {
 for (int sign : {-1,1}) {
  MotionDirectionTracker t; unsigned events=0; uint32_t now=0;
  for(int i=0;i<13;++i,now+=50) {
   float x=sign>0?20+i*10:140-i*10;
   if(t.update(now,{true,true,true,false,x,true,true,float(sign*10)})) {
    ++events; assert(t.event().early); assert(t.event().direction==(sign>0?MotionDirection::LEFT_TO_RIGHT:MotionDirection::RIGHT_TO_LEFT));
    printf("EARLY=%u normal earliest=%u saving>=%u ms\n",now,1000u,1000u-now);
   }
  }
  for(int i=0;i<60;++i,now+=50) if(t.update(now,{})) ++events;
  assert(events==1);
 }
 for(int mode=0;mode<4;++mode) {
  MotionDirectionTracker t;
  for(unsigned i=0;i<12;++i) {
   float x=mode==0?20+i*3:20+i*10;
   bool fired=t.update(i*50,{true,true,true,mode==1,x,true,true,mode==2?-10.f:10.f});
   if(mode<3) assert(!fired);
  }
 }
 // Repeated passes must recover despite isolated camera noise during cooldown.
 MotionDirectionTracker repeated;
 uint32_t tick = 0;
 unsigned repeatedEvents = 0;
 for (int pass = 0; pass < 3; ++pass) {
   for (int i = 0; i < 13; ++i, tick += 50) {
     float x = pass % 2 ? 140-i*10 : 20+i*10;
     float dx = pass % 2 ? -10.f : 10.f;
     if (repeated.update(tick, {true,true,true,false,x,true,true,dx})) {
       ++repeatedEvents;
       assert(repeated.event().direction == (pass % 2 ? MotionDirection::RIGHT_TO_LEFT : MotionDirection::LEFT_TO_RIGHT));
     }
   }
   assert(repeatedEvents == unsigned(pass + 1));
   // Persistent movement must never rearm, even after the cooldown expires.
   for (int i = 0; i < 60; ++i, tick += 50)
     assert(!repeated.update(tick, {true,true,true,false,140,false}));
   assert(repeated.state() == MotionTrackState::COOLDOWN);
   // A single noisy frame every 200ms previously prevented recovery forever.
   for (int i = 0; i < 60; ++i, tick += 50)
     assert(!repeated.update(tick, {true,i % 4 == 0,false,false,-1,false}));
   assert(repeated.state() == MotionTrackState::IDLE);
 }
 puts("Gate early, partial, ambiguous, conflicting flow, duplicate and noisy repeated-pass tests passed");
}

