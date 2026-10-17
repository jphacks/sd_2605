#pragma once
#include <cstdint>
namespace MotionDirectionConfig {
constexpr int sampleWidth = 40, sampleHeight = 30, imageWidth = 160;
constexpr int samplePixels = sampleWidth * sampleHeight;
constexpr uint8_t pixelDelta = 20, minNeighbors = 3;
constexpr unsigned minBlobPixels = 4, minRegionPixels = 12;
constexpr unsigned startFrames = 2, minTrackFrames = 4, endFrames = 3;
constexpr uint32_t captureIntervalMs = 33, previewIntervalMs = 50, endQuietMs = 400;
constexpr unsigned differenceLagFrames = 2;
constexpr int flowSearchRadius = 6, flowMinVotes = 3;
constexpr float flowVoteAgreement = .75f, flowPathAgreement = .65f;
constexpr uint32_t rearmQuietMs = 1000;
constexpr float backgroundClearPercent = 5.0f;
constexpr uint32_t cooldownMs = 2000, detectedHoldMs = 300, maxTrackMs = 6000, maxFrameGapMs = 500;
constexpr float minDisplacement = 32.0f, smoothingAlpha = .5f;
constexpr float maxCentroidStep = 64.0f, maxReverseTravel = 8.0f, jitterPixels = 2.0f;
constexpr float leftZoneEnd = imageWidth / 3.0f, rightZoneStart = imageWidth * 2.0f / 3.0f;
// If enabled, preview and processing are flipped together, keeping screen coordinates consistent.
constexpr bool mirrorImage = false;
// Installation mapping only: never changes the reported screen direction.
constexpr bool invertArrivalDeparture = false;
}
