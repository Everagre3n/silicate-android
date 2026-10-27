# Silicate

An AI level generator for Geometry Dash, as a [Geode](https://geode-sdk.org) mod. Describe a level, Gemini plans it, and Silicate builds and decorates it.

**How it works.** Gemini never writes raw object data. It writes a short *plan* (theme, bpm, sections, moods, gamemodes, structure styles). A deterministic compiler (`src/core`) then builds the level:

- **Structures**: stairs, towers/platforms, spike runs, pits with spikes, pad and orb jumps, and ship / wave / ball / UFO corridors with pillars. Sizes are clamped to cube-jump physics for the chosen difficulty and speed (max gap, spike width, rise, spacing).
- **Decoration**: two-tone checkered fills with accent columns, edge trim, hanging diamond chains and floating sparkles, mood-based colour triggers per section and bar-synced pulses. Solid decoration is only placed where the player can never reach (above the jump envelope, behind walls), because block hitboxes ignore z-layer. A test covers this.

## Install

1. Open the repo's **Actions** tab, pick the latest green *Build Silicate* run, and download the **silicate-geode** artifact (a zip).
2. Unzip it. Copy the `.geode` file for your platform (Android 32/64-bit, or Windows) into Geode's `mods` folder, or install it from the Geode launcher / in-game mod list.
3. Mod settings: paste your **Gemini API key** (free at https://aistudio.google.com/apikey), optionally change the **model** (default `gemini-flash-latest`, the free-tier Flash model) and **difficulty**.

## Use

Open **My Levels**, tap the **AI** button, type a description (e.g. "icy hard level with a ship drop"), and press **Generate**. The new level is added to the top of My Levels and can be opened in the editor.

## Status and caveats

- The mod compiles in CI for Android 32/64-bit and Windows against Geode 5.11.2 / GD 2.2081. It has **not been run in the game yet**, so the popup, the Gemini call and the look of generated levels are unverified.
- Object IDs and property keys live in `src/core/objects.hpp` (z-layers, pulse and no-touch keys are best-effort). If something looks wrong in your game version, fix it there.
- Robot and spider modes, slopes and music sync are not implemented.

## Development

- `src/core/` is plain C++ with no Geode dependency. Test it anywhere: `./tests/run.sh`
- `src/main.cpp` is the Geode UI layer (button, popup, Gemini request, saving the level).
- `tools/` holds the original Python implementation (`python -m silicate generate --offline`) used as a reference.
