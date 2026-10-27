# Silicate

AI level generator for Geometry Dash. Python, standard library only, so it runs on a PC or in Termux on Android.

**How it works.** Gemini never writes raw object data. It writes a short *plan* (theme, bpm, sections, moods, gamemodes, structure styles). A deterministic compiler then builds the level:

- **Structures**: stairs, towers/platforms, spike runs, pits with spikes, pad and orb jumps, and ship / wave / ball / UFO corridors with pillars. All sizes are clamped to cube-jump physics for the chosen difficulty and speed (max gap, spike width, rise, spacing).
- **Decoration**: two-tone checkered fills with accent columns, edge trim on every surface, hanging diamond chains and floating sparkles, mood-based colour triggers per section and bar-synced pulses. Solid decoration is placed only where the player can never reach (above the jump envelope, behind walls), because block hitboxes ignore z-layer. This is covered by a test.

## Use

```
python -m silicate config --api-key YOUR_KEY       # or just run generate and you'll be asked
python -m silicate generate "an icy hard level with a ship drop" --difficulty hard
python -m silicate generate "chill forest cruise" --model gemini-2.5-flash-lite
python -m silicate generate --offline --seed 7      # no AI, random planner
python -m silicate inspect out/Some_Level.gmd
```

- **API key is required.** Get a free one at https://aistudio.google.com/apikey. Lookup order: `--api-key`, `$GEMINI_API_KEY`, saved config, interactive prompt (which offers to save it, chmod 600).
- **Default model** is `gemini-flash-latest` (free-tier Gemini Flash). Change it with `--model`, `$SILICATE_MODEL`, or `config --model`.
- Output goes to `out/`: `.gmd` (import with GDShare/Geode-style tools), `.txt` (raw level string) and `.plan.json` (what the AI decided).

## Caveats

- Not verified in-game yet. Object IDs and property keys live in `silicate/objects.py` (z-layers, pulse and no-touch keys are best-effort). If something looks wrong in your build, fix it there.
- Robot and spider modes, slopes and custom song sync are not implemented.
- Importing on Android normally needs a mod loader or save-file editing; the game APK itself is never touched.

Tests: `python -m unittest discover -s tests`
