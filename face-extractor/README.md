# face-extractor

Photo → character parts → cartoon face for the M5Stack.

## How it works
1. `catalog.mjs` lists the categories and their allowed options
   (eyes, hair, glasses, beard, mouth, background...).
2. A photo is sent to the AI (Gemini). For each category it picks one option.
3. It returns a small dictionary, e.g. `{"glasses":"round","eyes":"male","beard":"none"}`.
4. We use that dictionary to set the parts and draw the character, like Lego.

The AI can only pick from the list, so the result can always be drawn.

## Files
- `catalog.mjs`: the list of parts, colour palette and 3 presets (boy / girl / friend). The shared contract.
- `extract.mjs`: command-line test. Prints the dictionary for one photo.
- `preview.html`: 320x240 preview with a dropdown for each part (same coordinates as faces.h).

## Try it
Preview (no key needed):
    cd face-extractor
    npx serve
    open http://localhost:3000/preview.html

Extractor (needs a free key from aistudio.google.com):
    npm install
    PowerShell: $env:GEMINI_API_KEY="your-key"
    node extract.mjs path\to\photo.jpg

## Status
- Done: photo → part IDs, preview for the main parts. The web app can save one face per home and expose it for the device to fetch.
- Not done: M5 code that calls the character API, drawing that face on the device, some parts (square / cat-eye glasses, extras) on the device firmware.
- The web preview was checked in the browser. The M5 has not been connected to this API, and the device screen does not show the saved face yet.
- Free Gemini API is sometimes slow or busy (503).
