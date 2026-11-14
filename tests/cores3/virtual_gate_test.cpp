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
 puts("Gate early, partial, ambiguous, conflicting flow and duplicate tests passed");
}

