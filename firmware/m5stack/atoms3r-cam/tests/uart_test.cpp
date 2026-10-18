#include <HardwareSerial.h>
#include <atomic>
#include "../ProtocolParser.h"
// Expose only private members in this host test; production public API is unchanged.
#define class struct
#include "../UartEventReceiver.h"
#undef class
#include <cassert>
std::string HardwareSerial::output;
HardwareSerial Serial{0};
void send(UartEventReceiver& r, const std::string& line) { for(char c:line) r.receive(c); }
int main() {
 UartEventReceiver r; assert(r.begin());
 send(r,"EVT,1,1A2B3C4D,1,LEFT_TO_RIGHT\n");
 assert(HardwareSerial::output=="ACK,1,1A2B3C4D,1\n");
 send(r,"EVT,1,1A2B3C4D,1,LEFT_TO_RIGHT\n");
 assert(r.acceptedCount==1 && r.duplicateCount==1);
 DoorProtocol::Action a; assert(r.next(a) && a==DoorProtocol::Action::LeftToRight); assert(!r.next(a));
 const auto old=HardwareSerial::output;
 send(r,"EVT,2,1A2B3C4D,2,REPLAY\n");
 send(r,std::string(100,'X')+"\n");
 std::string binary="EVT,1,1A2B3C4D,2,REPLAY"; binary.push_back(0); binary+="junk\n";send(r,binary);
 assert(HardwareSerial::output==old && !r.next(a));
 for(unsigned n=2;n<18;++n) send(r,"EVT,1,1A2B3C4D,"+std::to_string(n)+",REPLAY\n");
 const auto full=HardwareSerial::output;
 send(r,"EVT,1,1A2B3C4D,18,REPLAY\n"); assert(HardwareSerial::output==full);
 assert(r.next(a));
 send(r,"EVT,1,1A2B3C4D,18,REPLAY\r\n"); assert(r.acceptedCount==18);
 delete r.queue;
 puts("UART framing/ACK/queue tests passed");
}
