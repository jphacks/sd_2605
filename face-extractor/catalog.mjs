// The single source of truth. Prompt, schema, editor, web preview and M5 code all use these IDs.
export const CATALOG = {
  bg:        ["sky", "pink", "yellow", "mint", "lavender", "peach", "white", "gray"],
  hairColor: ["black", "darkbrown", "brown", "blonde", "red", "gray", "white", "blue", "pink"],
  hair:      ["none", "straight-bangs", "curly"],
  eyeColor:  ["black", "brown", "hazel", "green", "blue", "gray"],
  eyes:      ["male", "female", "halfmoon", "lashed"],
  glasses:   ["none", "round", "square", "cat-eye"],
  beard:     ["none", "mustache", "full-beard"],
  mouth:     ["tongue", "teeth-smile", "closed-smile", "w"],
  extras:    ["clip", "ribbon", "freckles"], // multi-select; mole is placed by the user
};

// Hex values for the renderers (pastel-friendly). Edit freely, the AI never sees these.
export const PALETTE = {
  bg: { sky:"#96c8ff", pink:"#ffcde1", yellow:"#ffe896", mint:"#b8f0d0",
        lavender:"#d8c8ff", peach:"#ffd2b0", white:"#f4f4ff", gray:"#d0d0d8" },
  hairColor: { black:"#2a2024", darkbrown:"#4a3226", brown:"#7a5238", blonde:"#e8c870",
               red:"#b84a30", gray:"#9a9aa0", white:"#eeeeee", blue:"#4a6ad0", pink:"#e878a8" },
  eyeColor: { black:"#1f1720", brown:"#38220f", hazel:"#4a3810", green:"#1a4a2f",
              blue:"#1d4275", gray:"#3e424c" },
};

export const DEFAULT_CHARACTER = {
  bg: "sky", hairColor: "black", hair: "none", eyeColor: "black", eyes: "male",
  glasses: "none", beard: "none", mouth: "closed-smile", extras: [],
};

// Ready-made characters that match the three faces.h presets (BOY / GIRL / FRIEND).
// They only use parts that exist in both renderers, so the preview and the M5 look the same.
export const PRESETS = {
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