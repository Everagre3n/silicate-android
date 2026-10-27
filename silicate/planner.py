"""Level plans: AI (Gemini) or offline random."""
import random

from . import llm
from .spec import (DIFFICULTIES, ELEMENTS, GAMEMODES, MOODS, STYLES, THEMES, sanitize)

SYSTEM = """You are a Geometry Dash level designer. You do NOT write object data; you write a high-level PLAN as JSON and a compiler builds the level, enforcing jump physics and decorating it.

Output ONE JSON object:
{
  "name": string (<=20 chars),
  "theme": one of %(themes)s,
  "difficulty": one of %(diffs)s,
  "bpm": integer 60-220,
  "sections": [ {
      "gamemode": one of %(modes)s,
      "speed": one of "0.5x","1x","2x","3x","4x",
      "mood": one of %(moods)s,
      "bars": integer 2-32 (4 beats each),
      "style": one of %(styles)s   (cube sections only; chooses the structure family),
      "density": number 0-1 (obstacles per bar; ship/wave: how twisty and hazardous),
      "elements": OPTIONAL list for cube sections only, each {"t": name, <param>: int}
  } ]
}
Cube element vocabulary (name: param range): %(elems)s.
"stairs"/"platform"/"up"/"down" change floor height; "gap" is a pit on raised floor; "pad_gap"/"orb_gap" are jumps using yellow pads/orbs.

Design rules:
- Structure the level like music: calm intro -> build -> drop -> break -> bigger drop -> calm outro. Vary mood per section; "drop" is the visual/energy peak.
- Do not switch gamemode more often than every 6 bars. Put ship/wave/ball/ufo in the middle, start and end on cube unless asked otherwise.
- Fast speeds (3x,4x) only in drops on hard/insane. Easy levels stay on 1x with low density.
- Total 5-9 sections unless the user asks for a length. Prefer omitting "elements" and picking a style; only give elements to craft a specific motif.
- Be creative with the name and theme to match the user's request. Return JSON only."""


def system_prompt():
    elems = "; ".join("%s(%s %d-%d)" % (k, v[0], v[1], v[2]) for k, v in ELEMENTS.items())
    return SYSTEM % {
        "themes": list(THEMES), "diffs": list(DIFFICULTIES), "modes": list(GAMEMODES),
        "moods": list(MOODS), "styles": list(STYLES), "elems": elems,
    }


def ai_plan(prompt, api_key, model, difficulty=None, theme=None, bars=None):
    hints = []
    if difficulty:
        hints.append("difficulty: %s" % difficulty)
    if theme:
        hints.append("theme: %s" % theme)
    if bars:
        hints.append("about %d bars in total" % bars)
    user = "Design a level: %s" % (prompt or "surprise me")
    if hints:
        user += "\nConstraints: " + "; ".join(hints)
    raw = llm.generate_json(api_key, model, system_prompt(), user)
    plan = sanitize(raw)
    if difficulty:
        plan["difficulty"] = difficulty
    if theme:
        plan["theme"] = theme
    return plan


def offline_plan(seed=None, difficulty="normal", theme=None, bars=64):
    rng = random.Random(seed)
    arc = [("calm", "cube"), ("build", "cube"), ("drop", "cube"), ("break", "ship"),
           ("build", "cube"), ("drop", "wave"), ("drop", "cube"), ("calm", "cube")]
    per = max(4, bars // len(arc))
    secs = []
    for mood, mode in arc:
        secs.append({
            "gamemode": mode, "speed": "2x" if mood == "drop" and difficulty in ("hard", "insane") else "1x",
            "mood": mood, "bars": per, "style": rng.choice(STYLES),
            "density": {"calm": 0.3, "build": 0.5, "drop": 0.8, "break": 0.4}[mood],
        })
    return sanitize({"name": "Offline %s" % (seed if seed is not None else "Mix"),
                     "theme": theme or rng.choice(THEMES), "difficulty": difficulty,
                     "bpm": rng.choice((110, 128, 140, 150)), "sections": secs}, rng)
