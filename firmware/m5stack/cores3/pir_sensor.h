#pragma once
void initPir() {
  pinMode(PIR_PIN, INPUT);
  pir = digitalRead(PIR_PIN) == HIGH;
  pirStartedAt = millis();
  Serial.println("[PIR] GPIO8 initialized; warmup 30s, then wait LOW");
}
void updatePir(uint32_t now) {
  if (cooling && uint32_t(now - cooldownAt) >= COOLDOWN_MS) cooling = false;
  bool high = digitalRead(PIR_PIN) == HIGH;
  bool rising = high && !pir;
  if (high != pir) Serial.printf("[PIR] %s -> %s\n", pir ? "HIGH" : "LOW", high ? "HIGH" : "LOW");
  pir = high;
  if (!pirReady) {
    if (uint32_t(now - pirStartedAt) < WARMUP_MS) return;
    pirReady = true;
    Serial.println("[PIR] Warmup complete; waiting for LOW");
  }
  if (!high) armed = true;
  if (!rising) return;
  bool eligible = armed;
  armed = false; // Consume every edge, including edges during playback/cooldown.
  if (eligible && !cooling && audio == IDLE && !pending) {
    ++count;
    detectedAt = now;
    pending = true;
    Serial.println("[MOTION] Detected");
  }
}
