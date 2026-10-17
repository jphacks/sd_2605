#pragma once

// PORT B GPIO36 has no internal pull-up. Sample edges in the main loop.
inline void initPir() { pinMode(PIR_PIN, INPUT); }
inline bool readPir() { return digitalRead(PIR_PIN) == HIGH; }
