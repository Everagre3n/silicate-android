"""Level plan schema: sanitising whatever the model returns into something buildable."""
import random

from .objects import SPEEDS

GAMEMODES = ("cube", "ship", "wave", "ball", "ufo")
MOODS = ("calm", "build", "drop", "break")
STYLES = ("mixed", "stairs", "towers", "spike_run", "pit_run", "orb_run", "pad_run")
DIFFICULTIES = ("easy", "normal", "hard", "insane")
THEMES = ("neon", "fire", "ice", "forest", "void", "sunset", "candy")

ELEMENTS = {  # name -> (param, lo, hi, default)
    "flat": ("n", 1, 12, 3),
    "spike": ("n", 1, 3, 1),
    "gap": ("w", 1, 6, 2),
    "up": ("dh", 1, 2, 1),
    "down": ("dh", 1, 5, 1),
    "platform": ("n", 2, 10, 4),
    "stairs": ("n", 2, 5, 3),
    "pad_gap": ("w", 1, 6, 3),
    "orb_gap": ("w", 1, 6, 4),
    "coin": ("n", 1, 1, 1),
}


def clamp(v, lo, hi, default):
    try:
        v = int(round(float(v)))
    except (TypeError, ValueError):
        return default
    return max(lo, min(hi, v))


def sanitize(plan: dict, rng=None) -> dict:
    rng = rng or random.Random()
    if not isinstance(plan, dict):
        raise ValueError("plan must be a JSON object")
    out = {
        "name": str(plan.get("name") or "Untitled")[:20],
        "theme": plan.get("theme") if plan.get("theme") in THEMES else rng.choice(THEMES),
        "difficulty": plan.get("difficulty") if plan.get("difficulty") in DIFFICULTIES else "normal",
        "bpm": clamp(plan.get("bpm"), 60, 220, 128),
        "sections": [],
    }
    for s in plan.get("sections") or []:
        if not isinstance(s, dict):
            continue
        mode = s.get("gamemode") if s.get("gamemode") in GAMEMODES else "cube"
        speed = s.get("speed") if s.get("speed") in SPEEDS else "1x"
        sec = {
            "gamemode": mode,
            "speed": speed,
            "mood": s.get("mood") if s.get("mood") in MOODS else "build",
            "bars": clamp(s.get("bars"), 2, 32, 8),
            "style": s.get("style") if s.get("style") in STYLES else "mixed",
            "density": max(0.0, min(1.0, float(s.get("density", 0.5) or 0.5))),
            "elements": [],
        }
        for e in s.get("elements") or []:
            name = e.get("t") if isinstance(e, dict) else None
            if name in ELEMENTS and mode == "cube":
                p, lo, hi, d = ELEMENTS[name]
                sec["elements"].append({"t": name, p: clamp(e.get(p), lo, hi, d)})
        out["sections"].append(sec)
    if not out["sections"]:
        raise ValueError("plan has no sections")
    return out


def difficulty_cfg(difficulty: str, speed: str) -> dict:
    """Jump-physics limits in blocks (1 block = 30 units). Cube jump ~4.05 blocks
    long and ~2.2 high at 1x; it scales with speed."""
    ups = SPEEDS[speed][1]
    ratio = ups / 311.58
    jump_len = 4.05 * ratio
    f = {"easy": 0.6, "normal": 0.7, "hard": 0.8, "insane": 0.85}[difficulty]
    min_flat = {"easy": 5, "normal": 4, "hard": 3, "insane": 2}[difficulty]
    return {
        "max_gap": max(2, int(jump_len * f)),
        "max_spike": 2 if difficulty in ("easy", "normal") else 3,
        "max_rise": 1 if difficulty == "easy" else 2,
        "min_flat": max(2, round(min_flat * ratio)),
        "ship_gap": {"easy": 6, "normal": 5, "hard": 4, "insane": 4}[difficulty],
        "wave_gap": {"easy": 5, "normal": 4, "hard": 4, "insane": 3}[difficulty],
        "ratio": ratio,
    }
