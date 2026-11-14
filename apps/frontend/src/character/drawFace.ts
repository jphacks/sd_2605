// Canvas port of face-extractor/preview.html. Coordinates match that preview and faces.h.
import { PALETTE, type Character } from "./catalog";

type Pen = CanvasRenderingContext2D;

const rgb565 = (r: number, g: number, b: number) =>
  (((r & 0xf8) << 8) | ((g & 0xfc) << 3) | (b >> 3)) & 0xffff;

const TFT_BLACK = rgb565(0, 0, 0);
const TFT_WHITE = rgb565(255, 255, 255);
const FACE_EYE_FRAME = rgb565(20, 14, 18);
const FACE_MOUTH = TFT_BLACK;
const FACE_MOUTH_OPEN = rgb565(60, 24, 36);
const FACE_TONGUE = rgb565(246, 132, 146);
const FACE_GLASS = rgb565(58, 50, 62);
const OUTLINE = rgb565(92, 48, 50);
const FACE_PI = Math.PI;

function hexTo565(hex: string) {
  const n = parseInt(hex.slice(1), 16);
  return rgb565((n >> 16) & 0xff, (n >> 8) & 0xff, n & 0xff);
}

function unpack565(c: number): [number, number, number] {
  const r = (c >> 11) & 0x1f;
  const g = (c >> 5) & 0x3f;
  const b = c & 0x1f;
  return [(r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2)];
}

function css(c: number) {
  const [r, g, b] = unpack565(c);
  return `rgb(${r},${g},${b})`;
}

const to565Map = (obj: object) =>
  Object.fromEntries(Object.entries(obj).map(([key, value]) => [key, hexTo565(String(value))]));

const PAL = {
  bg: to565Map(PALETTE.bg),
  hairColor: to565Map(PALETTE.hairColor),
  eyeColor: to565Map(PALETTE.eyeColor),
};

const bg565 = (c: Character) => PAL.bg[c.bg] ?? PAL.bg.sky;
const hair565 = (c: Character) => PAL.hairColor[c.hairColor] ?? PAL.hairColor.black;
const eye565 = (c: Character) => PAL.eyeColor[c.eyeColor] ?? PAL.eyeColor.black;

function hairStripe565(c: Character) {
  const [r, g, b] = unpack565(hair565(c));
  const lum = (r * 299 + g * 587 + b * 114) / 1000;
  if (lum < 80) return rgb565(Math.min(255, r + 55), Math.min(255, g + 40), Math.min(255, b + 60));
  return rgb565(Math.trunc(r * 0.62), Math.trunc(g * 0.62), Math.trunc(b * 0.72));
}

const BLUSH_BY_BG: Record<string, number> = {
  sky: hexTo565("#fa8aa8"),
  pink: hexTo565("#f482a5"),
  yellow: hexTo565("#f8ac78"),
};
const BLUSH_DEFAULT = hexTo565("#f58aa8");
const blush565 = (c: Character) => BLUSH_BY_BG[c.bg] ?? BLUSH_DEFAULT;

const FACE_CX = 160, FACE_CY = 120, FACE_R = 200;
const EYE_Y = 96, EYE_DX = 74, EYE_R = 38;
const EYE_X_L = FACE_CX - EYE_DX;
const EYE_X_R = FACE_CX + EYE_DX;
const GLASS_R = EYE_R + 9;
const BLUSH_Y = 160, BLUSH_DX = 116, BLUSH_RX = 30, BLUSH_RY = 14;
const MOUTH_Y = 170;
const int = Math.trunc;

function fillCircle(g: Pen, x: number, y: number, r: number, color: number) {
  g.fillStyle = css(color);
  g.beginPath();
  g.arc(x, y, r, 0, FACE_PI * 2);
  g.fill();
}

function fillEllipse(g: Pen, x: number, y: number, rx: number, ry: number, color: number) {
  g.fillStyle = css(color);
  g.beginPath();
  g.ellipse(x, y, rx, ry, 0, 0, FACE_PI * 2);
  g.fill();
}

function fillTriangle(g: Pen, x0: number, y0: number, x1: number, y1: number, x2: number, y2: number, color: number) {
  g.fillStyle = css(color);
  g.beginPath();
  g.moveTo(x0, y0);
  g.lineTo(x1, y1);
  g.lineTo(x2, y2);
  g.closePath();
  g.fill();
}

function drawLine(g: Pen, x0: number, y0: number, x1: number, y1: number, color: number) {
  g.strokeStyle = css(color);
  g.lineWidth = 1;
  g.beginPath();
  g.moveTo(x0 + 0.5, y0 + 0.5);
  g.lineTo(x1 + 0.5, y1 + 0.5);
  g.stroke();
}

function fillRect(g: Pen, x: number, y: number, w: number, h: number, color: number) {
  g.fillStyle = css(color);
  g.fillRect(x, y, w, h);
}

function drawEllipse(g: Pen, x0: number, y0: number, rx: number, ry: number, color: number) {
  if (rx <= 0 || ry <= 0) return;
  g.strokeStyle = css(color);
  g.lineWidth = 1.5;
  g.beginPath();
  g.ellipse(x0, y0, rx, ry, 0, 0, FACE_PI * 2);
  g.stroke();
}

function drawFaceStroke(g: Pen, x0: number, y0: number, x1: number, y1: number, r: number, color: number) {
  const dx = x1 - x0, dy = y1 - y0;
  const n = Math.max(Math.abs(dx), Math.abs(dy));
  if (n === 0) {
    fillCircle(g, x0, y0, r, color);
    return;
  }
  for (let i = 0; i <= n; ++i) fillCircle(g, x0 + int((dx * i) / n), y0 + int((dy * i) / n), r, color);
}

function paintBg(id: string) {
  const color = PAL.bg[id] ?? PAL.bg.sky;
  return (g: Pen) => {
    fillRect(g, 0, 0, 320, 240, color);
    fillCircle(g, FACE_CX, FACE_CY, FACE_R, color);
  };
}

function drawBlush(g: Pen, c: Character) {
  fillEllipse(g, FACE_CX - BLUSH_DX, BLUSH_Y, BLUSH_RX, BLUSH_RY, blush565(c));
  fillEllipse(g, FACE_CX + BLUSH_DX, BLUSH_Y, BLUSH_RX, BLUSH_RY, blush565(c));
}

const EYE_FRAME = 5;

function drawEyeBase(g: Pen, x: number, y: number, rx: number, ry: number, c: Character) {
  fillEllipse(g, x, y, rx, ry, FACE_EYE_FRAME);
  fillEllipse(g, x, y, Math.max(1, rx - EYE_FRAME), Math.max(1, ry - EYE_FRAME), eye565(c));
}

function drawEyeBaseSolid(g: Pen, x: number, y: number, r: number, c: Character) {
  fillCircle(g, x, y, r, FACE_EYE_FRAME);
  fillCircle(g, x, y, r - 2, eye565(c));
}

function drawEyeCrescent(g: Pen, x: number, y: number, r: number, c: Character, leftSide: boolean) {
  const outer = leftSide ? -1 : 1;
  const dWhite = r - 36;
  const dCover = dWhite - 5;
  fillCircle(g, x + outer * dWhite, y, 32, TFT_WHITE);
  fillCircle(g, x + outer * dCover, y, 32, eye565(c));
}

function drawEyeHighlights(g: Pen, x: number, y: number, r: number, c: Character, leftSide: boolean, lashed: boolean) {
  drawEyeCrescent(g, x, y, r, c, leftSide);
  const inner = leftSide ? 1 : -1;
  const outer = -inner;
  fillCircle(g, x + inner * int(r / 5), y - int(r / 3), int(r / 3), TFT_WHITE);
  const bx = int(r / 3);
  const by = int(r / 3);
  const small = Math.max(3, int(r / 6));
  const big = small + 2;
  fillCircle(g, x + outer * bx, y + by, lashed ? big : small, TFT_WHITE);
  fillCircle(g, x + inner * bx, y + by, lashed ? small - 1 : small, TFT_WHITE);
}

function drawEyeRound(g: Pen, x: number, y: number, r: number, c: Character) {
  drawEyeBaseSolid(g, x, y, r, c);
  drawEyeHighlights(g, x, y, r, c, x < FACE_CX, false);
}

function drawEyeliner(g: Pen, x: number, y: number, r: number, leftSide: boolean) {
  const outer = leftSide ? -1 : 1;
  const start = leftSide ? 195 : 345;
  const end = leftSide ? 340 : 200;
  const steps = 28;
  for (let i = 0; i <= steps; ++i) {
    const t = i / steps;
    const a = ((start + (end - start) * t) * FACE_PI) / 180;
    const px = x + int((r - 4) * Math.cos(a));
    const py = y + int((r - 4) * Math.sin(a));
    fillCircle(g, px, py, 2 + int(2 * (1 - t)), TFT_BLACK);
  }
  const bx = x + outer * (r - 2);
  fillTriangle(g, bx, y - 6, bx - outer * 3, y - 11, x + outer * (r + 11), y - 15, TFT_BLACK);
}

function drawEyeFemale(g: Pen, x: number, y: number, r: number, c: Character) {
  const leftSide = x < FACE_CX;
  drawEyeBaseSolid(g, x, y, r, c);
  drawEyeHighlights(g, x, y, r, c, leftSide, false);
  drawEyeliner(g, x, y, r, leftSide);
}

function drawFaceLashes(g: Pen, x: number, y: number, r: number, leftSide: boolean) {
  const angles = [202, 226, 250];
  for (let i = 0; i < 3; ++i) {
    const a = leftSide ? angles[i] : 180 - angles[i];
    const rad = (a * FACE_PI) / 180;
    const bx = Math.cos(rad), by = Math.sin(rad);
    const px = int(-by * 5), py = int(bx * 5);
    const rx = x + int((r - 2) * bx), ry = y + int((r - 2) * by);
    const tx = x + int((r + 10) * bx), ty = y + int((r + 10) * by);
    fillTriangle(g, rx + px, ry + py, rx - px, ry - py, tx, ty, TFT_BLACK);
  }
}

function drawGirlEye(g: Pen, x: number, y: number, r: number, c: Character, leftSide: boolean) {
  drawEyeBaseSolid(g, x, y, r, c);
  drawEyeHighlights(g, x, y, r, c, leftSide, true);
  drawFaceLashes(g, x, y, r, leftSide);
}

function drawHalfMoonEye(g: Pen, x: number, y: number, r: number, c: Character, leftSide: boolean) {
  drawEyeBase(g, x, y, r, r, c);
  fillRect(g, x - r - 2, y - r - 4, 2 * r + 4, r + 4, bg565(c));
  drawFaceStroke(g, x - r + 1, y, x + r - 1, y, 2, FACE_EYE_FRAME);
  const inner = leftSide ? 1 : -1;
  fillCircle(g, x + inner * int(r / 3), y + int(r / 6), 3, TFT_WHITE);
  fillCircle(g, x + inner * int(r / 8), y + int(r / 2), 2, TFT_WHITE);
}

function drawGirlMouth(g: Pen, cx: number, y: number) {
  const top = y - 9, bot = y + 9, mid = y - 6;
  drawFaceStroke(g, cx - 24, top, cx - 12, bot, 3, FACE_MOUTH);
  drawFaceStroke(g, cx - 12, bot, cx, mid, 3, FACE_MOUTH);
  drawFaceStroke(g, cx, mid, cx + 12, bot, 3, FACE_MOUTH);
  drawFaceStroke(g, cx + 12, bot, cx + 24, top, 3, FACE_MOUTH);
}

function drawFaceSmile(g: Pen, cx: number, y: number, w: number, depth: number) {
  let oX = 0, oY = 0;
  for (let i = 0; i <= 12; ++i) {
    const t = i / 12;
    const qX = cx - w + int(2 * w * t + 0.5);
    const qY = y + int(depth * (1 - (2 * t - 1) * (2 * t - 1)) + 0.5);
    if (i > 0) drawFaceStroke(g, oX, oY, qX, qY, 3, FACE_MOUTH);
    oX = qX;
    oY = qY;
  }
}

function drawTeethSmile(g: Pen) {
  fillEllipse(g, FACE_CX, 171, 34, 16, FACE_MOUTH_OPEN);
  fillEllipse(g, FACE_CX, 161, 26, 4, TFT_WHITE);
  fillEllipse(g, FACE_CX + 4, 179, 15, 7, FACE_TONGUE);
}

function drawMustache(g: Pen, c: Character) {
  const hair = hair565(c);
  fillEllipse(g, FACE_CX - 14, 150, 18, 7, hair);
  fillEllipse(g, FACE_CX + 14, 150, 18, 7, hair);
}

function drawFullBeard(g: Pen, c: Character) {
  const hair = hair565(c);
  drawFaceStroke(g, FACE_CX - 50, 150, FACE_CX - 48, 194, 10, hair);
  drawFaceStroke(g, FACE_CX + 50, 150, FACE_CX + 48, 194, 10, hair);
  fillEllipse(g, FACE_CX, 212, 54, 26, hair);
  drawMustache(g, c);
}

function drawGlassesRound(g: Pen) {
  for (let d = 0; d < 3; ++d) {
    drawEllipse(g, EYE_X_L, EYE_Y, GLASS_R - d, GLASS_R - d, FACE_GLASS);
    drawEllipse(g, EYE_X_R, EYE_Y, GLASS_R - d, GLASS_R - d, FACE_GLASS);
  }
  for (let d = -1; d < 2; ++d) drawLine(g, EYE_X_L + GLASS_R - 2, EYE_Y + d, EYE_X_R - GLASS_R + 2, EYE_Y + d, FACE_GLASS);
  drawLine(g, EYE_X_L - GLASS_R + 2, EYE_Y, 8, EYE_Y - 6, FACE_GLASS);
  drawLine(g, EYE_X_R + GLASS_R - 2, EYE_Y, 312, EYE_Y - 6, FACE_GLASS);
}

function drawRectOutline(g: Pen, x: number, y: number, w: number, h: number, t: number, color: number) {
  fillRect(g, x, y, w, t, color);
  fillRect(g, x, y + h - t, w, t, color);
  fillRect(g, x, y, t, h, color);
  fillRect(g, x + w - t, y, t, h, color);
}

function drawGlassesSquare(g: Pen) {
  const half = EYE_R + 8, t = 3;
  for (const x of [EYE_X_L, EYE_X_R]) drawRectOutline(g, x - half, EYE_Y - half, half * 2, half * 2, t, FACE_GLASS);
  for (let d = -1; d < 2; ++d) drawLine(g, EYE_X_L + half, EYE_Y + d, EYE_X_R - half, EYE_Y + d, FACE_GLASS);
  drawLine(g, EYE_X_L - half, EYE_Y - 6, 8, EYE_Y - 10, FACE_GLASS);
  drawLine(g, EYE_X_R + half, EYE_Y - 6, 312, EYE_Y - 10, FACE_GLASS);
}

function drawGlassesCatEye(g: Pen) {
  const innerDx = EYE_R + 8;
  const botY = EYE_Y + EYE_R + 4;
  for (const side of [-1, 1]) {
    const x = FACE_CX + side * EYE_DX;
    const outerTopX = x + side * (EYE_R + 18);
    const innerX = x - side * innerDx;
    const outerBotX = x + side * (EYE_R + 10);
    drawFaceStroke(g, outerTopX, 48, innerX, 56, 2, FACE_GLASS);
    drawFaceStroke(g, innerX, 56, innerX, botY, 2, FACE_GLASS);
    drawFaceStroke(g, innerX, botY, outerBotX, botY - 4, 2, FACE_GLASS);
    drawFaceStroke(g, outerBotX, botY - 4, outerTopX, 48, 2, FACE_GLASS);
  }
  for (let d = -1; d < 2; ++d) drawLine(g, EYE_X_L + innerDx, EYE_Y + d, EYE_X_R - innerDx, EYE_Y + d, FACE_GLASS);
  drawLine(g, EYE_X_L - (EYE_R + 12), EYE_Y - 4, 8, EYE_Y - 10, FACE_GLASS);
  drawLine(g, EYE_X_R + (EYE_R + 12), EYE_Y - 4, 312, EYE_Y - 10, FACE_GLASS);
}

function drawCurlyBangs(g: Pen, c: Character) {
  const hair = hair565(c);
  fillRect(g, 0, 0, 320, 24, hair);
  for (let x = 20; x <= 300; x += 40) fillCircle(g, x, 24, 21, hair);
}

function drawStraightBangs(g: Pen, c: Character) {
  const base = hair565(c);
  const stripe = hairStripe565(c);
  fillRect(g, 0, 0, 320, 70, base);
  const tops = [64, 128, 192, 256];
  const bottoms = [48, 120, 200, 272];
  for (let i = 0; i < tops.length; ++i) {
    const xTop = tops[i];
    const xBot = bottoms[i];
    const xMid = (xTop + xBot) / 2;
    const xAt = (t: number) => (1 - t) * (1 - t) * xTop + 2 * (1 - t) * t * xMid + t * t * xBot;
    const yAt = (t: number) => (1 - t) * (1 - t) * -6 + 2 * (1 - t) * t * 27 + t * t * 64;
    for (let s = 0; s <= 28; ++s) {
      const t = s / 28;
      fillCircle(g, int(xAt(t)), int(yAt(t)), 20, OUTLINE);
    }
    for (let s = 0; s <= 28; ++s) {
      const t = s / 28;
      fillCircle(g, int(xAt(t)), int(yAt(t)), 16, stripe);
    }
  }
  for (let x = 0; x < 320; ++x) {
    const dx = (x - 160) / 160;
    const yLine = int(46 + 14 * dx * dx);
    fillRect(g, x, yLine, 1, 100 - yLine, bg565(c));
  }
  for (let x = 0; x <= 320; x += 2) {
    const dx = (x - 160) / 160;
    fillCircle(g, x, int(46 + 14 * dx * dx), 2, OUTLINE);
  }
}

const RIBBON_RED = rgb565(232, 28, 48);
const CLIP_PIN = rgb565(150, 220, 205);
const CLIP_PETAL = rgb565(255, 150, 185);
const CLIP_CENTER = rgb565(255, 208, 88);
const FRECKLE_COLOR = rgb565(132, 78, 56);

function drawClip(g: Pen) {
  drawFaceStroke(g, 50, 44, 74, 28, 5, OUTLINE);
  drawFaceStroke(g, 52, 42, 72, 29, 3, CLIP_PIN);
  drawFaceStroke(g, 56, 41, 70, 32, 1, OUTLINE);
  const fx = 84, fy = 22;
  for (let i = 0; i < 5; ++i) {
    const a = (-90 + i * 72) * (Math.PI / 180);
    const px = fx + Math.round(9 * Math.cos(a));
    const py = fy + Math.round(9 * Math.sin(a));
    fillCircle(g, px, py, 7, OUTLINE);
    fillCircle(g, px, py, 5, CLIP_PETAL);
  }
  fillCircle(g, fx, fy, 7, OUTLINE);
  fillCircle(g, fx, fy, 4, CLIP_CENTER);
}

function drawRibbon(g: Pen) {
  fillEllipse(g, 222, 27, 14, 19, OUTLINE);
  fillTriangle(g, 262, 30, 222, 8, 222, 46, OUTLINE);
  fillEllipse(g, 222, 27, 11, 16, RIBBON_RED);
  fillTriangle(g, 262, 30, 222, 11, 222, 43, RIBBON_RED);
  fillEllipse(g, 298, 34, 14, 19, OUTLINE);
  fillTriangle(g, 262, 30, 298, 15, 298, 53, OUTLINE);
  fillEllipse(g, 298, 34, 11, 16, RIBBON_RED);
  fillTriangle(g, 262, 30, 298, 18, 298, 50, RIBBON_RED);
  fillCircle(g, 240, 32, 4, OUTLINE);
  fillCircle(g, 240, 32, 2, RIBBON_RED);
  fillCircle(g, 284, 34, 4, OUTLINE);
  fillCircle(g, 284, 34, 2, RIBBON_RED);
  fillCircle(g, 262, 30, 12, OUTLINE);
  fillCircle(g, 262, 30, 9, RIBBON_RED);
}

function drawFreckles(g: Pen) {
  const dots: Array<[number, number]> = [
    [-118, -4], [-106, -9], [-94, -3], [-112, -15], [-98, -14],
    [118, -4], [106, -9], [94, -3], [112, -15], [98, -14],
  ];
  for (const [dx, dy] of dots) fillCircle(g, FACE_CX + dx, BLUSH_Y + dy, 2, FRECKLE_COLOR);
}

type Brick = (g: Pen, c: Character) => void;

const PARTS: Record<string, Record<string, Brick>> = {
  bg: Object.fromEntries(Object.keys(PAL.bg).map((id) => [id, (g: Pen) => paintBg(id)(g)])),
  eyes: {
    male: (g, c) => {
      drawEyeRound(g, EYE_X_L, EYE_Y, EYE_R, c);
      drawEyeRound(g, EYE_X_R, EYE_Y, EYE_R, c);
    },
    female: (g, c) => {
      drawEyeFemale(g, EYE_X_L, EYE_Y, EYE_R, c);
      drawEyeFemale(g, EYE_X_R, EYE_Y, EYE_R, c);
    },
    halfmoon: (g, c) => {
      drawHalfMoonEye(g, EYE_X_L, EYE_Y, EYE_R, c, true);
      drawHalfMoonEye(g, EYE_X_R, EYE_Y, EYE_R, c, false);
    },
    lashed: (g, c) => {
      drawGirlEye(g, EYE_X_L, EYE_Y, EYE_R, c, true);
      drawGirlEye(g, EYE_X_R, EYE_Y, EYE_R, c, false);
    },
  },
  mouth: {
    tongue: (g) => {
      fillTriangle(g, FACE_CX - 24, MOUTH_Y - 13, FACE_CX + 24, MOUTH_Y - 13, FACE_CX + 15, MOUTH_Y + 8, FACE_MOUTH);
      fillTriangle(g, FACE_CX - 24, MOUTH_Y - 13, FACE_CX + 15, MOUTH_Y + 8, FACE_CX - 15, MOUTH_Y + 8, FACE_MOUTH);
      fillEllipse(g, FACE_CX, MOUTH_Y + 8, 15, 7, FACE_MOUTH);
      fillEllipse(g, FACE_CX, MOUTH_Y + 7, 17, 10, FACE_TONGUE);
    },
    "teeth-smile": (g) => { drawTeethSmile(g); },
    "closed-smile": (g) => { drawFaceSmile(g, FACE_CX, MOUTH_Y - 4, 12, 9); },
    w: (g) => { drawGirlMouth(g, FACE_CX, MOUTH_Y); },
  },
  beard: {
    none: () => {},
    mustache: (g, c) => { drawMustache(g, c); },
    "full-beard": (g, c) => { drawFullBeard(g, c); },
  },
  glasses: {
    none: () => {},
    round: (g) => { drawGlassesRound(g); },
    square: (g) => { drawGlassesSquare(g); },
    "cat-eye": (g) => { drawGlassesCatEye(g); },
  },
  hair: {
    none: () => {},
    "straight-bangs": (g, c) => { drawStraightBangs(g, c); },
    curly: (g, c) => { drawCurlyBangs(g, c); },
  },
  extras: {
    clip: (g) => { drawClip(g); },
    ribbon: (g) => { drawRibbon(g); },
    freckles: (g) => { drawFreckles(g); },
  },
};

function drawPart(g: Pen, c: Character, slot: string, id: string) {
  PARTS[slot]?.[id]?.(g, c);
}

export function renderFace(canvas: HTMLCanvasElement, character: Character) {
  const g = canvas.getContext("2d");
  if (!g) return;
  g.clearRect(0, 0, canvas.width, canvas.height);
  drawPart(g, character, "bg", character.bg);
  drawPart(g, character, "hair", character.hair);
  drawBlush(g, character);
  drawPart(g, character, "eyes", character.eyes);
  drawPart(g, character, "mouth", character.mouth);
  drawPart(g, character, "beard", character.beard);
  drawPart(g, character, "glasses", character.glasses);
  for (const id of character.extras ?? []) drawPart(g, character, "extras", id);
}
