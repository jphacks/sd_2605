#include <cassert>
#include <cstdio>
#include "../../firmware/m5stack/cores3/AudioSyncPolicy.h"
int main() {
  assert(audioNeedsDownload(-1, 1, false));
  assert(!audioNeedsDownload(1, 1, true));
  assert(audioNeedsDownload(1, 2, true));
  assert(!audioNeedsDownload(2, 1, true));
  assert(audioNeedsDownload(2, 2, false));
  assert(audioNeedsDownload(2, 1, false));
  // Each slot is independent; a changed leave version does not refresh arrive.
  assert(!audioNeedsDownload(4, 4, true));
  assert(audioNeedsDownload(7, 8, true));
  puts("Audio sync: first fetch, unchanged, newer, rollback rejection, repair, slots passed");
}
