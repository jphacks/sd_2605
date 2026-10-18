#include <initializer_list>
#include "../ProtocolParser.h"
#include <cassert>
#include <cstdio>
using namespace DoorProtocol;
int main() {
  Event e;
  assert(parse("EVT,1,1A2B3C4D,1,LEFT_TO_RIGHT", e));
  assert(e.boot == 0x1A2B3C4D && e.seq == 1 && e.action == Action::LeftToRight);
  assert(parse("EVT,1,00000000,4294967295,REPLAY", e));
  assert(parse("EVT,1,ffffffff,0,RIGHT_TO_LEFT", e));
  for (const char* bad : {"EVT,2,1A2B3C4D,1,REPLAY", "EVT,1,1A2B3C4,1,REPLAY",
       "EVT,1,1A2B3C4G,1,REPLAY", "EVT,1,1A2B3C4D,4294967296,REPLAY",
       "EVT,1,1A2B3C4D,-1,REPLAY", "EVT,1,1A2B3C4D,+1,REPLAY",
       "EVT,1,1A2B3C4D,,REPLAY", "EVT,1,1A2B3C4D,1,REPLAY,",
       "EVT,1,1A2B3C4D,1,REPLAY ", "EVT,1,1A2B3C4D,1,UNKNOWN", ""})
    assert(!parse(bad, e));
  Dedup d;
  for (uint32_t i = 0; i < 10000; ++i) {
    e = {42, i, Action::Replay};
    assert(!d.contains(e) && d.canAdd(e)); d.add(e); assert(d.contains(e));
  }
  e.seq = 0; assert(!d.canAdd(e));
  e.seq = 9998; assert(d.contains(e));
  e.boot = 43; assert(!d.contains(e)); d.add(e); assert(d.contains(e));
  for (uint32_t boot = 44; boot < 50; ++boot) { e.boot = boot; d.add(e); }
  e.boot = 50; assert(!d.canAdd(e));
  puts("protocol/dedup tests passed");
}
