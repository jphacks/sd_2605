// ---------------------------------------------------------------- face drawing ----
// Self-contained character faces. Drawn with primitives only (no images, no SD, no
// sensors, no audio), so this is the only code needed to draw a character in any
// M5Unified / LovyanGFX sketch.
//
// A character is a colour set plus a style (see FaceCharacter / BOY / GIRL / FRIEND below).
// This file only draws; it does not handle buttons. Pick the character in your own code:
//
//   drawFace(display, mood, blink, character);
//     display    any lgfx::LGFXBase&  (M5.Display, or an M5Canvas for flicker-free drawing)
//     mood       one of the FaceMood values below
//     blink      0 = eyes wide open ... 100 = eyes fully shut
//     character  a FaceCharacter (defaults to BOY)
//
// The head is larger than the panel and centred on it, so the panel itself becomes the face.
// drawFace() fills the whole screen with the body colour first, so you can call it directly.
//
// `#include <M5Unified.h>` brings in lgfx::LGFXBase and the TFT_* colours; drop that line if
// your sketch already includes it (or LovyanGFX directly).

#include <M5Unified.h>

// ------------------------------------------------------------------------ colours ----
constexpr uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return uint16_t(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}
constexpr uint16_t FACE_EYE = rgb565(52, 38, 44);       // eye colour
constexpr uint16_t FACE_MOUTH = TFT_BLACK;              // solid black mouth
constexpr uint16_t FACE_TONGUE = rgb565(246, 132, 146); // tongue
constexpr uint16_t FACE_GLASS = rgb565(58, 50, 62);     // spectacle frame
constexpr float FACE_PI = 3.14159265f;

// --------------------------------------------------------------------- characters ----
// How a character is drawn.
//   STYLE_BOY    round eyes, glasses, laughs.
//   STYLE_GIRL   lashed eyes, "w" cat mouth, winks.
//   STYLE_FRIEND heavy-lidded eyes, "u" smile, squints "> <".
enum FaceStyle { STYLE_BOY, STYLE_GIRL, STYLE_FRIEND };

// A character is its colours plus how it looks.
struct FaceCharacter {
  uint16_t fill;   // body colour (fills the whole panel)
  uint16_t blush;  // blush colour
  FaceStyle style;
};

// "boy": blue body, pink blush, glasses.
constexpr FaceCharacter BOY = {
  rgb565(150, 200, 255), rgb565(250, 138, 168), STYLE_BOY
};
// "girl": pink body, deeper pink blush, lashed eyes and a "w" cat mouth.
constexpr FaceCharacter GIRL = {
  rgb565(255, 205, 225), rgb565(244, 130, 165), STYLE_GIRL
};
// "friend": yellow body, warm blush, heavy-lidded eyes and a "u" smile.
constexpr FaceCharacter FRIEND = {
  rgb565(255, 232, 150), rgb565(248, 172, 120), STYLE_FRIEND
};

// ----------------------------------------------------------------------- geometry ----
// Sized for a 320 x 240 panel (M5Stack Core, rotation 1).
constexpr int FACE_CX = 160, FACE_CY = 120, FACE_R = 200;  // covers the full panel
constexpr int EYE_Y = 96, EYE_DX = 74, EYE_R = 42;
constexpr int GLASS_R = EYE_R + 9;                         // spectacle lens around each eye
constexpr int BLUSH_Y = 160, BLUSH_DX = 116, BLUSH_RX = 30, BLUSH_RY = 14;
constexpr int MOUTH_Y = 170;

// What the character is doing. MOOD_FUN is each character's signature random expression
// (the girl winks, the friend squints).
enum FaceMood { MOOD_IDLE, MOOD_WAKE, MOOD_SING, MOOD_SLEEP, MOOD_LAUGH, MOOD_FUN };

// ------------------------------------------------------------------------ drawing ----
// A round-capped stroke of radius r, drawn as a run of circles between two points.
void drawFaceStroke(lgfx::LGFXBase& g, int x0, int y0, int x1, int y1, int r, uint16_t color) {
  int dx = x1 - x0, dy = y1 - y0;
  int n = abs(dx) > abs(dy) ? abs(dx) : abs(dy);
  if (n == 0) { g.fillCircle(x0, y0, r, color); return; }
  for (int i = 0; i <= n; ++i) g.fillCircle(x0 + dx * i / n, y0 + dy * i / n, r, color);
}

// ----------------------------------------------------------------------- boy eyes ----
// One eye. style: 0 = open, 1 = wide (surprised), 2 = happy ^^, 3 = sleepy (closed),
// 4 = laughing (squinting half eye). blink: 0 = wide open, 100 = fully shut. The eye is
// squashed vertically as the lids close, which reads as a human blink. fill is the body
// colour, used to cut shapes.
void drawFaceEye(lgfx::LGFXBase& g, int x, int y, int style, int r, int blink, uint16_t fill) {
  if (style == 2) {  // happy ^^, drawn twice to make the caret 2 px thick
    for (int d = 0; d < 2; ++d) {
      g.drawLine(x - r, y + 5 + d, x, y - 4 + d, FACE_EYE);
      g.drawLine(x, y - 4 + d, x + r, y + 5 + d, FACE_EYE);
    }
    return;
  }
  int ry = r - (r - 2) * blink / 100;  // 0 -> r (open), 100 -> 2 (shut)
  if (style == 1) {  // wide, with a small pupil so it still reads as an eye
    g.fillEllipse(x, y, r + 4, ry + 4, TFT_WHITE);
    g.fillEllipse(x, y, r - 1, ry > 2 ? ry - 1 : 2, FACE_EYE);
    if (blink < 45) g.fillCircle(x - 3, y - ry / 3, 2, TFT_WHITE);
  } else if (style == 3) {  // sleepy, kept shut
    ry = 2;
    g.fillEllipse(x, y, r, ry, FACE_EYE);
  } else if (style == 4) {  // laughing: top half cut away, single highlight
    g.fillEllipse(x, y, r, r, FACE_EYE);
    g.fillRect(x - r - 1, y - r - 1, 2 * r + 2, r + 1, fill);
    g.fillCircle(x - r / 3, y + r / 4, r / 5 > 3 ? r / 5 : 3, TFT_WHITE);
    ry = 1;
  } else {  // open, with a single highlight
    g.fillEllipse(x, y, r, ry, FACE_EYE);
    if (blink < 45) g.fillEllipse(x - r / 3, y - ry / 3, r / 3, ry / 3 > 1 ? ry / 3 : 1, TFT_WHITE);
  }
}

// ---------------------------------------------------------------------- girl eyes ----
// Three lash spikes on the outer top corner of an eye (left eye: on the left, right eye
// on the right).
void drawFaceLashes(lgfx::LGFXBase& g, int x, int y, int r, bool leftSide) {
  const int angles[3] = { 202, 226, 250 };
  for (int i = 0; i < 3; ++i) {
    int a = leftSide ? angles[i] : 180 - angles[i];
    float rad = a * FACE_PI / 180.0f;
    float bx = cosf(rad), by = sinf(rad);
    int px = int(-by * 5), py = int(bx * 5);
    int rx = x + int((r - 5) * bx), ry = y + int((r - 5) * by);
    int tx = x + int((r + 10) * bx), ty = y + int((r + 10) * by);
    g.fillTriangle(rx + px, ry + py, rx - px, ry - py, tx, ty, FACE_EYE);
  }
}

// Lashes for a shut eye. Instead of flaring upward like the open eye, they sit on the outer
// end of the lid and follow its curve, drooping out and down.
void drawFaceLashesClosed(lgfx::LGFXBase& g, int x, int y, int r, bool leftSide) {
  const int angles[3] = { 204, 220, 236 };
  for (int i = 0; i < 3; ++i) {
    int a = leftSide ? angles[i] : 180 - angles[i];
    float rad = a * FACE_PI / 180.0f;
    float ca = cosf(rad), sa = sinf(rad);
    int bx = x + int(r * ca);
    int by = y + int(r * 0.7f * sa);
    float s = leftSide ? 1.0f : -1.0f;  // outward along the lid
    int ex = bx + int(s * sa * 15.0f);
    int ey = by + int(s * (-0.7f) * ca * 15.0f);
    drawFaceStroke(g, bx, by, ex, ey, 2, FACE_EYE);
  }
}

// An open girl eye: big round black eye with two highlights and outer lashes. Blinks like
// the boy, by squashing vertically.
void drawGirlEye(lgfx::LGFXBase& g, int x, int y, int r, int blink, bool leftSide) {
  int ry = r - (r - 3) * blink / 100;
  g.fillEllipse(x, y, r, ry, FACE_EYE);
  if (blink < 60) {
    int hr = (ry * 2 / 5) > 3 ? ry * 2 / 5 : 3;
    g.fillEllipse(x + r / 4, y - ry / 5, r * 2 / 5, hr, TFT_WHITE);   // big highlight
    g.fillCircle(x - r / 3, y + ry / 4, r / 6 > 2 ? r / 6 : 2, TFT_WHITE);  // small highlight
  }
  if (blink >= 60) drawFaceLashesClosed(g, x, y, r, leftSide);
  else drawFaceLashes(g, x, y, r, leftSide);
}

// A closed girl eye: an arch with the outer lashes (used when winking or sleepy).
void drawGirlEyeClosed(lgfx::LGFXBase& g, int x, int y, int r, bool leftSide) {
  int oX = 0, oY = 0;
  for (int a = 196; a <= 344; a += 6) {
    float rad = a * FACE_PI / 180.0f;
    int qX = x + int(r * cosf(rad));
    int qY = y + int(r * 0.7f * sinf(rad));
    if (a > 196) drawFaceStroke(g, oX, oY, qX, qY, 2, FACE_EYE);
    oX = qX; oY = qY;
  }
  drawFaceLashesClosed(g, x, y, r, leftSide);
}

// -------------------------------------------------------------------- friend eyes ----
// A friend eye: a heavy-lidded eye. The body colour scoops the upper part away so it reads
// as half shut, with two small highlights and a thick lid line above. Blinks like the
// others, by squashing vertically.
void drawFriendEye(lgfx::LGFXBase& g, int x, int y, int r, int blink, uint16_t fill) {
  int ry = r - (r - 2) * blink / 100;
  g.fillEllipse(x, y, r, ry, FACE_EYE);
  if (blink < 60) {
    g.fillEllipse(x + r / 6, y - ry * 3 / 4, r * 3 / 4, ry / 2, fill);  // the lid scoops the top
    g.fillCircle(x - r / 3, y + ry / 6, 3, TFT_WHITE);                  // small highlight
    g.fillCircle(x - r / 8, y + ry / 2, 2, TFT_WHITE);                  // small highlight
    // Thick upper lid line.
    int oX = 0, oY = 0;
    for (int i = 0; i <= 10; ++i) {
      float t = i / 10.0f;
      int qX = x - r - 2 + int((2 * r + 4) * t);
      int qY = y - r - 5 - int(9 * (1.0f - (2 * t - 1) * (2 * t - 1)) + 0.5f);
      if (i > 0) drawFaceStroke(g, oX, oY, qX, qY, 3, FACE_EYE);
      oX = qX; oY = qY;
    }
  }
}

// The friend's random expression: a happy squint, "> <".
void drawFriendSquint(lgfx::LGFXBase& g, int x, int y, bool leftSide) {
  int d = leftSide ? 1 : -1;  // ">" on the left, "<" on the right
  drawFaceStroke(g, x - d * 30, y - 20, x + d * 20, y, 4, FACE_EYE);
  drawFaceStroke(g, x + d * 20, y, x - d * 30, y + 20, 4, FACE_EYE);
}

// -------------------------------------------------------------------- friend mouth ----
// A "u" smile.
void drawFaceSmile(lgfx::LGFXBase& g, int cx, int y, int w, int depth) {
  int oX = 0, oY = 0;
  for (int i = 0; i <= 12; ++i) {
    float t = i / 12.0f;
    int qX = cx - w + int(2 * w * t + 0.5f);
    int qY = y + int(depth * (1.0f - (2 * t - 1) * (2 * t - 1)) + 0.5f);
    if (i > 0) drawFaceStroke(g, oX, oY, qX, qY, 3, FACE_MOUTH);
    oX = qX; oY = qY;
  }
}

// --------------------------------------------------------------------- girl mouth ----
// The "w" cat mouth: flat top ends, two rounded bottoms and a middle peak.
void drawGirlMouth(lgfx::LGFXBase& g, int cx, int y) {
  int top = y - 9, bot = y + 9, mid = y - 6;
  drawFaceStroke(g, cx - 24, top, cx - 12, bot, 3, FACE_MOUTH);
  drawFaceStroke(g, cx - 12, bot, cx, mid, 3, FACE_MOUTH);
  drawFaceStroke(g, cx, mid, cx + 12, bot, 3, FACE_MOUTH);
  drawFaceStroke(g, cx + 12, bot, cx + 24, top, 3, FACE_MOUTH);
}

// A small music note, used while the greeting sound plays.
void drawFaceNote(lgfx::LGFXBase& g, int x, int y, uint16_t color) {
  g.fillCircle(x, y, 4, color);
  g.drawLine(x + 4, y, x + 4, y - 16, color);
  g.drawLine(x + 4, y - 16, x + 12, y - 11, color);
}

// The open mouths shared by every character: surprised, singing, laughing and sleepy.
void drawFaceOpenMouth(lgfx::LGFXBase& g, int mood, uint16_t fill) {
  if (mood == MOOD_LAUGH) {       // big open laugh
    g.fillEllipse(FACE_CX, MOUTH_Y, 24, 28, FACE_MOUTH);
    g.fillEllipse(FACE_CX, MOUTH_Y + 15, 13, 9, FACE_TONGUE);
  } else if (mood == MOOD_SING) { // open while the sound plays, no tongue
    g.fillEllipse(FACE_CX, MOUTH_Y, 22, 26, FACE_MOUTH);
  } else if (mood == MOOD_WAKE) { // surprised: small round mouth
    g.fillCircle(FACE_CX, MOUTH_Y, 12, FACE_MOUTH);
  } else {                        // sleepy: smaller smile
    g.fillEllipse(FACE_CX, MOUTH_Y, 13, 9, FACE_MOUTH);
    g.fillEllipse(FACE_CX, MOUTH_Y - 9, 17, 9, fill);  // trim the top half
  }
}

// Draw one character filling the whole panel. No text is ever drawn.
void drawFace(lgfx::LGFXBase& g, int mood, int blink, const FaceCharacter& c = BOY) {
  // Fill the whole panel with the body colour: the screen itself becomes the face.
  g.fillScreen(c.fill);
  g.fillCircle(FACE_CX, FACE_CY, FACE_R, c.fill);

  if (c.style == STYLE_BOY) {
    const int eyeStyle = mood == MOOD_SING ? 2 : mood == MOOD_LAUGH ? 4 :
                         mood == MOOD_WAKE ? 1 : mood == MOOD_SLEEP ? 3 : 0;
    drawFaceEye(g, FACE_CX - EYE_DX, EYE_Y, eyeStyle, EYE_R, blink, c.fill);
    drawFaceEye(g, FACE_CX + EYE_DX, EYE_Y, eyeStyle, EYE_R, blink, c.fill);
    // Spectacles: a ring around each eye, a bridge over the nose and arms to the sides.
    for (int d = 0; d < 3; ++d) {
      g.drawEllipse(FACE_CX - EYE_DX, EYE_Y, GLASS_R - d, GLASS_R - d, FACE_GLASS);
      g.drawEllipse(FACE_CX + EYE_DX, EYE_Y, GLASS_R - d, GLASS_R - d, FACE_GLASS);
    }
    for (int d = -1; d < 2; ++d) {
      g.drawLine(FACE_CX - EYE_DX + GLASS_R - 2, EYE_Y + d,
                 FACE_CX + EYE_DX - GLASS_R + 2, EYE_Y + d, FACE_GLASS);
    }
    g.drawLine(FACE_CX - EYE_DX - GLASS_R + 2, EYE_Y, 8, EYE_Y - 6, FACE_GLASS);
    g.drawLine(FACE_CX + EYE_DX + GLASS_R - 2, EYE_Y, 312, EYE_Y - 6, FACE_GLASS);
  } else if (c.style == STYLE_GIRL) {
    // The girl winks with the left eye; being sleepy shuts both.
    const bool wink = (mood == MOOD_FUN) || (mood == MOOD_SLEEP);
    if (wink) drawGirlEyeClosed(g, FACE_CX - EYE_DX, EYE_Y, EYE_R, true);
    else drawGirlEye(g, FACE_CX - EYE_DX, EYE_Y, EYE_R, blink, true);
    if (mood == MOOD_SLEEP) drawGirlEyeClosed(g, FACE_CX + EYE_DX, EYE_Y, EYE_R, false);
    else drawGirlEye(g, FACE_CX + EYE_DX, EYE_Y, EYE_R, blink, false);
  } else {
    if (mood == MOOD_FUN) {  // random expression: squint "> <"
      drawFriendSquint(g, FACE_CX - EYE_DX, EYE_Y, true);
      drawFriendSquint(g, FACE_CX + EYE_DX, EYE_Y, false);
    } else {
      drawFriendEye(g, FACE_CX - EYE_DX, EYE_Y, EYE_R, blink, c.fill);
      drawFriendEye(g, FACE_CX + EYE_DX, EYE_Y, EYE_R, blink, c.fill);
    }
  }

  g.fillEllipse(FACE_CX - BLUSH_DX, BLUSH_Y, BLUSH_RX, BLUSH_RY, c.blush);
  g.fillEllipse(FACE_CX + BLUSH_DX, BLUSH_Y, BLUSH_RX, BLUSH_RY, c.blush);

  // The mouth is solid black.
  if (c.style == STYLE_BOY) {
    if (mood == MOOD_IDLE || mood == MOOD_FUN) {
      // Normal time: flat top, sloping sides, tongue at the bottom.
      g.fillTriangle(FACE_CX - 24, MOUTH_Y - 13, FACE_CX + 24, MOUTH_Y - 13,
                     FACE_CX + 15, MOUTH_Y + 8, FACE_MOUTH);
      g.fillTriangle(FACE_CX - 24, MOUTH_Y - 13, FACE_CX + 15, MOUTH_Y + 8,
                     FACE_CX - 15, MOUTH_Y + 8, FACE_MOUTH);
      g.fillEllipse(FACE_CX, MOUTH_Y + 8, 15, 7, FACE_MOUTH);    // round the bottom corners
      g.fillEllipse(FACE_CX, MOUTH_Y + 7, 17, 10, FACE_TONGUE);  // tongue, poking out below
    } else {
      drawFaceOpenMouth(g, mood, c.fill);
    }
  } else if (c.style == STYLE_GIRL) {
    if (mood == MOOD_IDLE || mood == MOOD_FUN) drawGirlMouth(g, FACE_CX, MOUTH_Y);
    else drawFaceOpenMouth(g, mood, c.fill);
  } else {  // the friend keeps the same "u" smile, both normally and when squinting
    if (mood == MOOD_IDLE || mood == MOOD_FUN) drawFaceSmile(g, FACE_CX, MOUTH_Y - 4, 12, 9);
    else drawFaceOpenMouth(g, mood, c.fill);
  }

  // Music notes while singing. (No text is ever written.)
  if (mood == MOOD_SING) {
    drawFaceNote(g, 258, 72, TFT_CYAN);
    drawFaceNote(g, 294, 46, TFT_CYAN);
  }
}
