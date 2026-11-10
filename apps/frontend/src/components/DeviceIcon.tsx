import { Bird, Bot, Cat, Dog, Flower2, Heart, House, Leaf, Moon, Smile, Star, Sun } from "lucide-react";
import type { LucideIcon } from "lucide-react";
import type { DeviceIcon as DeviceIconValue } from "../devices";

export const DEFAULT_ICON_ID = "robot";
export const PRESET_ICONS: { id: string; label: string; Icon: LucideIcon; bg: string; fg: string }[] = [
  { id: "robot", label: "ロボット", Icon: Bot, bg: "#e6eee3", fg: "#5d805f" },
  { id: "house", label: "おうち", Icon: House, bg: "#f5e7d9", fg: "#a57b53" },
  { id: "heart", label: "ハート", Icon: Heart, bg: "#f8e4e0", fg: "#b9675b" },
  { id: "star", label: "ほし", Icon: Star, bg: "#f6efd3", fg: "#a88743" },
  { id: "sun", label: "たいよう", Icon: Sun, bg: "#fbecd2", fg: "#b9833a" },
  { id: "moon", label: "つき", Icon: Moon, bg: "#e7e9f3", fg: "#6f77a6" },
  { id: "flower", label: "はな", Icon: Flower2, bg: "#f6e4ee", fg: "#a76486" },
  { id: "leaf", label: "はっぱ", Icon: Leaf, bg: "#e1efdc", fg: "#5c8a58" },
  { id: "cat", label: "ねこ", Icon: Cat, bg: "#f3e8dc", fg: "#9a7656" },
  { id: "dog", label: "いぬ", Icon: Dog, bg: "#efe6da", fg: "#8f7253" },
  { id: "bird", label: "とり", Icon: Bird, bg: "#e3eef4", fg: "#5f8aa3" },
  { id: "smile", label: "えがお", Icon: Smile, bg: "#f7ecd8", fg: "#a8803c" },
];
const presetOf = (id?: string) => PRESET_ICONS.find(p => p.id === id) ?? PRESET_ICONS[0];

/** 家のアイコン。写真なら画像、プリセットなら色付きの丸。未設定なら標準（ロボット） */
export default function DeviceIcon({ icon, size = 36, label }: { icon?: DeviceIconValue; size?: number; label?: string }) {
  const style = { width: size, height: size, borderRadius: Math.round(size * 0.33) };
  if (icon?.kind === "photo") return <img className="device-icon" style={style} src={icon.dataUrl} alt={label ?? ""} aria-hidden={label ? undefined : true} />;
  const { Icon, bg, fg, label: presetLabel } = presetOf(icon?.kind === "preset" ? icon.id : undefined);
  return <span className="device-icon" style={{ ...style, background: bg, color: fg }} role={label ? "img" : undefined} aria-label={label ? `${label}（${presetLabel}）` : undefined} aria-hidden={label ? undefined : true}><Icon size={Math.round(size * 0.52)} /></span>;
}
