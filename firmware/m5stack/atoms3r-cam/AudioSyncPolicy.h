#pragma once
#include <stdint.h>
// Never roll back a valid cached WAV; repair a missing/corrupt file even at the same version.
inline bool audioNeedsDownload(int64_t saved, int64_t remote, bool validCachedWav) {
  return !validCachedWav || remote > saved;
}
