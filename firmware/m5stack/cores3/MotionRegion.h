#pragma once
#include <algorithm>
#include <cstdlib>
#include <cmath>
#include "MotionDirectionConfig.h"

struct MotionRegion {
  bool usable = false, ambiguous = false;
  float x = -1;
  unsigned pixels = 0, components = 0;
};

// Fixed workspace; no allocation and no large arrays on the camera task stack.
class MotionRegionExtractor {
  uint8_t mask[MotionDirectionConfig::samplePixels] = {};
  uint8_t filtered[MotionDirectionConfig::samplePixels] = {};
  uint16_t queue[MotionDirectionConfig::samplePixels] = {};
  struct Blob {
    unsigned pixels = 0, sumX = 0, sumY = 0;
    int minX = 40, maxX = 0, minY = 30, maxY = 0;
  };
 public:
  MotionRegion extract(const uint8_t* current, const uint8_t* previous, int lightShift) {
    using namespace MotionDirectionConfig;
    for (int i = 0; i < samplePixels; ++i)
      mask[i] = std::abs(int(current[i]) - previous[i] - lightShift) >= pixelDelta;
    for (int y = 0; y < sampleHeight; ++y) {
      for (int x = 0; x < sampleWidth; ++x) {
        unsigned neighbors = 0;
        for (int yy = std::max(0, y - 1); yy <= std::min(sampleHeight - 1, y + 1); ++yy)
          for (int xx = std::max(0, x - 1); xx <= std::min(sampleWidth - 1, x + 1); ++xx)
            neighbors += mask[yy * sampleWidth + xx];
        filtered[y * sampleWidth + x] = mask[y * sampleWidth + x] && neighbors >= minNeighbors;
      }
    }
    MotionRegion result;
    unsigned sumX = 0;
    Blob largest, second;
    for (int i = 0; i < samplePixels; ++i) {
      if (!filtered[i]) continue;
      Blob blob;
      unsigned head = 0, tail = 0;
      queue[tail++] = i; filtered[i] = 0;
      while (head < tail) {
        unsigned pos = queue[head++];
        int x = pos % sampleWidth, y = pos / sampleWidth;
        ++blob.pixels; blob.sumX += x; blob.sumY += y;
        blob.minX = std::min(blob.minX, x); blob.maxX = std::max(blob.maxX, x);
        blob.minY = std::min(blob.minY, y); blob.maxY = std::max(blob.maxY, y);
        for (int yy = std::max(0, y - 1); yy <= std::min(sampleHeight - 1, y + 1); ++yy) {
          for (int xx = std::max(0, x - 1); xx <= std::min(sampleWidth - 1, x + 1); ++xx) {
            unsigned next = yy * sampleWidth + xx;
            if (filtered[next]) { filtered[next] = 0; queue[tail++] = next; }
          }
        }
      }
      if (blob.pixels < minBlobPixels) continue;
      ++result.components; result.pixels += blob.pixels; sumX += blob.sumX;
      if (blob.pixels > largest.pixels) { second = largest; largest = blob; }
      else if (blob.pixels > second.pixels) second = blob;
    }
    result.usable = result.pixels >= minRegionPixels;
    if (result.usable) result.x = (float(sumX) / result.pixels + .5f) * (imageWidth / sampleWidth);
    // One moving object often produces two edge blobs. Permit nearby aligned edges,
    // but reject comparable blobs far apart or at different vertical positions.
    if (largest.pixels && second.pixels >= minRegionPixels && second.pixels * 3 >= largest.pixels) {
      float dx = std::abs(float(largest.sumX) / largest.pixels - float(second.sumX) / second.pixels);
      float dy = std::abs(float(largest.sumY) / largest.pixels - float(second.sumY) / second.pixels);
      bool verticalOverlap = largest.minY <= second.maxY && second.minY <= largest.maxY;
      result.ambiguous = dx > sampleWidth * .5f || (!verticalOverlap && dy > sampleHeight * .25f);
    }
    if (result.pixels > unsigned(samplePixels * .8f)) result.ambiguous = true;
    return result;
  }
};
