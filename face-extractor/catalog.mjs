// The single source of truth. Prompt, schema, editor, web preview and M5 code all use these IDs.
export const CATALOG = {
  bg:        ["sky", "pink", "yellow", "mint", "lavender", "peach", "white", "gray"],
  hairColor: ["black", "darkbrown", "brown", "blonde", "red", "gray", "white", "blue", "pink"],
  hair:      ["none", "straight-bangs", "side-bangs", "parted-bangs"],
  eyeColor:  ["black", "brown", "hazel", "green", "blue", "gray"],
  eyes:      ["round", "lashed", "sleepy", "sparkly"],
  brows:     ["none", "thin", "thick", "angled-up", "angled-down"],
  glasses:   ["none", "round", "square", "cat-eye"],
  beard:     ["none", "stubble", "mustache", "goatee", "full-beard"],
  mouth:     ["tongue", "teeth-smile", "closed-smile", "w"],
  extras:    ["ahoge", "clip", "ribbon", "freckles"], // multi-select; mole is placed by the user
};

// Hex values for the renderers (pastel-friendly). Edit freely, the AI never sees these.
export const PALETTE = {
  bg: { sky:"#96c8ff", pink:"#ffcde1", yellow:"#ffe896", mint:"#b8f0d0",
        lavender:"#d8c8ff", peach:"#ffd2b0", white:"#f4f4ff", gray:"#d0d0d8" },
  hairColor: { black:"#2a2024", darkbrown:"#4a3226", brown:"#7a5238", blonde:"#e8c870",
               red:"#b84a30", gray:"#9a9aa0", white:"#eeeeee", blue:"#4a6ad0", pink:"#e878a8" },
  eyeColor: { black:"#342634", brown:"#6a4430", hazel:"#8a6a30", green:"#3a8a5a",
              blue:"#3a78c8", gray:"#7a8090" },
};

export const DEFAULT_CHARACTER = {
  bg: "sky", hairColor: "black", hair: "none", eyeColor: "black", eyes: "round",
  brows: "thin", glasses: "none", beard: "none", mouth: "closed-smile", extras: [],
};