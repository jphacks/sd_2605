#include <cassert>
#include <cstdio>
#include "../../firmware/m5stack/cores3/PirFusion.h"
using O = PirFusion::Outcome;
static PirFusion ready(uint32_t boot = 0) {
  PirFusion f; f.begin(boot); f.sample(boot+30000, false); return f;
}
int main() {
  auto f=ready();
  assert(f.camera(30100,30100)==O::PENDING);
  f.sample(31100,true); assert(f.poll(31100)==O::CONFIRMED); // +1000 inclusive
  f.sample(40000,true); assert(f.camera(40000,40000)==O::PENDING);
  assert(f.poll(41001)==O::REJECTED); // held HIGH cannot be reused, even after cooldown
  f.sample(42000,false); f.sample(42100,true);
  assert(f.camera(42100,42100)==O::CONFIRMED);
  f.sample(42200,false); f.sample(42300,true);
  assert(f.camera(42300,42300)==O::REJECTED); // cooldown
  f=ready(); f.sample(30100,true); f.sample(30200,false);
  assert(f.camera(32100,32100)==O::CONFIRMED); // -2000 inclusive
  f=ready(); f.sample(30100,true); f.sample(30200,false);
  assert(f.camera(32101,32101)==O::PENDING); assert(f.poll(33102)==O::REJECTED);
  f=ready(); assert(f.camera(31000,31000)==O::PENDING);
  f.sample(32001,true); assert(f.poll(32001)==O::REJECTED); // too late
  f=ready(); f.sample(30100,true); f.sample(35000,true);
  assert(f.camera(35000,35000)==O::CONFIRMED); // HIGH held longer than before window
  f=ready(); f.sample(30100,true); assert(f.poll(30100)==O::NONE); // PIR only
  f.begin(0); f.sample(1,true); assert(f.camera(100,100)==O::REJECTED);
  f.sample(30000,true); assert(f.camera(30000,30000)==O::PENDING);
  assert(f.poll(31001)==O::REJECTED); // warmup HIGH is ignored
  f.sample(32000,false); f.sample(32100,true); assert(f.camera(32100,32100)==O::CONFIRMED);
  f=ready(); assert(f.camera(30100,30100)==O::PENDING); f.cancel();
  f.sample(30200,true); assert(f.poll(30200)==O::NONE);
  f=ready(0xffff8000u); f.sample(0xfffffff0u,true);
  assert(f.camera(10,10)==O::CONFIRMED); // millis wrap
  puts("PIR fusion tests passed");
}
