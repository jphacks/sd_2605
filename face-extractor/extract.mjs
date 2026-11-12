import fs from "node:fs";
import path from "node:path";
import { GoogleGenAI, Type } from "@google/genai";
import { CATALOG, DEFAULT_CHARACTER } from "./catalog.mjs";

const MODEL = "gemini-3.8-flash"; // if this errors, copy the current free Flash model name from AI Studio
const file = process.argv[2] || "me.jpg";

if (!process.env.GEMINI_API_KEY) {
  console.error("Set GEMINI_API_KEY first (see below).");
  process.exit(1);
}
const ai = new GoogleGenAI({ apiKey: process.env.GEMINI_API_KEY });

// Build the response schema from the catalog: every field is an enum, so the model can't invent values.
const enumField = (values) => ({ type: Type.STRING, enum: values });
const schema = {
  type: Type.OBJECT,
  properties: {
    ...Object.fromEntries(
      Object.entries(CATALOG).filter(([k]) => k !== "extras").map(([k, v]) => [k, enumField(v)])
    ),
    extras: { type: Type.ARRAY, items: enumField(CATALOG.extras) },
    uncertain: {
      type: Type.ARRAY,
      description: "Names of fields you are not confident about",
      items: enumField(Object.keys(CATALOG)),
    },
  },
  required: [...Object.keys(CATALOG), "uncertain"],
  propertyOrdering: [...Object.keys(CATALOG), "uncertain"],
};

const prompt = `You convert a photo of a person into a cartoon character made of fixed parts.
Look at the person's face and, for each field, choose the option that best matches.
Rules:
- Choose only from the allowed values.
- bg: pick a pastel that suits the person's skin tone or clothing. It is a style choice, not a skin match.
- hair: pick "none" unless the person clearly has bangs/fringe covering the forehead.
- eyes: choose by how the person's eyes look in the photo (shape, lashes, makeup).
  - "male": plain round eyes.
  - "female": eyes with eyeliner (a winged upper lid line).
  - "halfmoon": half-moon, relaxed or smiling eyes.
  - "lashed": round eyes with lashes but no eyeliner.
  Do not infer gender, age or ethnicity.
- eyeColor: the iris colour. If it is hard to see, pick "black" or "brown" and add "eyeColor" to uncertain.
- beard: "none" if clean-shaven or no clear facial hair.
- mouth: based on the smile in the photo (teeth visible -> teeth-smile).
- extras: only include items that are clearly visible (e.g. freckles).
- Do not guess gender, age or ethnicity. Do not output anything except the JSON.`;

const mimeTypes = { ".jpg": "image/jpeg", ".jpeg": "image/jpeg", ".png": "image/png", ".webp": "image/webp" };
const mimeType = mimeTypes[path.extname(file).toLowerCase()];
if (!mimeType) { console.error("Use a .jpg, .png or .webp file"); process.exit(1); }
const data = fs.readFileSync(file).toString("base64");

// Check every value against the catalog; fall back to the default for anything invalid.
function validate(raw) {
  const out = {};
  const bad = [];
  for (const [key, allowed] of Object.entries(CATALOG)) {
    if (key === "extras") {
      out.extras = Array.isArray(raw.extras) ? raw.extras.filter((e) => allowed.includes(e)) : [];
    } else if (allowed.includes(raw[key])) {
      out[key] = raw[key];
    } else {
      out[key] = DEFAULT_CHARACTER[key];
      bad.push(key);
    }
  }
  out.uncertain = [...new Set([...(raw.uncertain || []), ...bad])];
  return out;
}

try {
  const res = await ai.models.generateContent({
    model: MODEL,
    contents: [{ inlineData: { mimeType, data } }, { text: prompt }],
    config: {
      responseMimeType: "application/json",
      responseSchema: schema,
      temperature: 0.2,
    },
  });
  const character = validate(JSON.parse(res.text));
  console.log(JSON.stringify(character, null, 2));
} catch (err) {
  console.error("API call failed, using default character:", err.message);
  console.log(JSON.stringify({ ...DEFAULT_CHARACTER, uncertain: Object.keys(CATALOG) }, null, 2));
}