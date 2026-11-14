// Same IDs as face-extractor/catalog.mjs. The Rails catalog must stay in step with this list.

export const CATALOG = {
  bg: ["sky", "pink", "yellow", "mint", "lavender", "peach", "white", "gray"],
  hairColor: ["black", "darkbrown", "brown", "blonde", "red", "gray", "white", "blue", "pink"],
  hair: ["none", "straight-bangs", "curly"],
  eyeColor: ["black", "brown", "hazel", "green", "blue", "gray"],
  eyes: ["male", "female", "halfmoon", "lashed"],
  glasses: ["none", "round", "square", "cat-eye"],
  beard: ["none", "mustache", "full-beard"],
  mouth: ["tongue", "teeth-smile", "closed-smile", "w"],
  extras: ["clip", "ribbon", "freckles"],
} as const;

export type Character = {
  bg: string;
  hairColor: string;
  hair: string;
  eyeColor: string;
  eyes: string;
  glasses: string;
  beard: string;
  mouth: string;
  extras: string[];
  uncertain?: string[];
};

export const PALETTE = {
  bg: {
    sky: "#96c8ff", pink: "#ffcde1", yellow: "#ffe896", mint: "#b8f0d0",
    lavender: "#d8c8ff", peach: "#ffd2b0", white: "#f4f4ff", gray: "#d0d0d8",
  },
  hairColor: {
    black: "#2a2024", darkbrown: "#4a3226", brown: "#7a5238", blonde: "#e8c870",
    red: "#b84a30", gray: "#9a9aa0", white: "#eeeeee", blue: "#4a6ad0", pink: "#e878a8",
  },
  eyeColor: {
    black: "#1f1720", brown: "#38220f", hazel: "#4a3810", green: "#1a4a2f",
    blue: "#1d4275", gray: "#3e424c",
  },
} as const;

export const PRESETS: Record<"boy" | "girl" | "friend", Character> = {
  boy: {
    bg: "sky", hairColor: "black", hair: "none", eyeColor: "black", eyes: "male",
    glasses: "round", beard: "none", mouth: "tongue", extras: [],
  },
  girl: {
    bg: "pink", hairColor: "black", hair: "none", eyeColor: "black", eyes: "female",
    glasses: "none", beard: "none", mouth: "w", extras: [],
  },
  friend: {
    bg: "yellow", hairColor: "black", hair: "none", eyeColor: "black", eyes: "halfmoon",
    glasses: "none", beard: "none", mouth: "closed-smile", extras: [],
  },
};

export const SLOT_LABELS: Record<Exclude<keyof typeof CATALOG, "extras">, string> = {
  bg: "背景",
  hairColor: "髪の色",
  hair: "前髪",
  eyeColor: "瞳の色",
  eyes: "目",
  glasses: "眼鏡",
  beard: "ひげ",
  mouth: "口",
};

export const OPTION_LABELS: Record<string, string> = {
  sky: "空色", pink: "ピンク", yellow: "黄色", mint: "ミント", lavender: "ラベンダー", peach: "ピーチ", white: "白", gray: "グレー",
  black: "黒", darkbrown: "ダークブラウン", brown: "茶", blonde: "金髪", red: "赤", blue: "青",
  none: "なし", "straight-bangs": "まっすぐ", curly: "カール",
  hazel: "ヘーゼル", green: "緑",
  male: "丸", female: "アイライン", halfmoon: "半月", lashed: "まつげ",
  round: "丸眼鏡", square: "四角", "cat-eye": "キャットアイ",
  mustache: "口ひげ", "full-beard": "あごひげ",
  tongue: "舌", "teeth-smile": "歯を見せる", "closed-smile": "閉じた笑顔", w: "猫口",
  clip: "ヘアピン", ribbon: "リボン", freckles: "そばかす",
};

export const optionLabel = (id: string) => OPTION_LABELS[id] ?? id;

export const cloneCharacter = (character: Character): Character => ({
  ...character,
  extras: [...character.extras],
  uncertain: [...(character.uncertain ?? [])],
});
