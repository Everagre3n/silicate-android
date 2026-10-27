"""Compile a sanitised plan into Geometry Dash objects.

Grid: column c / row r -> x = 30c+15, y = 30r+15 (row 0 sits on the ground).
Gameplay is built from rule-bound elements (jump limits come from spec.difficulty_cfg),
then decorated; solid decoration never enters the playable envelope.
"""
import math
import random

from . import decorate
from . import objects as O
from .objects import SPEEDS
from .gdformat import Obj, build_header, encode_level
from .spec import difficulty_cfg

START_COL = 10
HEAD = 9          # cube: rows above the floor treated as reachable
CEIL_TOP = 10     # ship/wave/ball/ufo arenas: rows 0..CEIL_TOP-1 are playable at most


class World:
    def __init__(self):
        self.objs = []
        self.floor_h = {}   # col -> solid floor height (rows 0..h-1)
        self.reach = {}     # col -> highest row the player/orbs can reach
        self.ceil = {}      # col -> lowest solid ceiling row (arena modes)
        self.last_hazard = -99

    def put(self, id_, c, r, props=None):
        self.objs.append(Obj(id_, c * 30 + 15, r * 30 + 15, dict(props or {})))

    def floor(self, c, n, h, arena_reach=None):
        for col in range(c, c + n):
            self.set_floor(col, h)
            self.reach[col] = max(self.reach.get(col, 0), arena_reach if arena_reach is not None else h + HEAD)
        return c + n

    def set_floor(self, col, h):
        self.floor_h[col] = h
        for r in range(h):
            role = "top" if r == h - 1 else "fill"
            id_, p = decorate.block_style(role, col, r, h - 1 - r)
            self.put(id_, col, r, p)

    def deco_floor(self, col):
        """First row where solid decoration is allowed in this column."""
        if col not in self.reach:
            return None
        lo = self.reach[col] + 1
        if col in self.ceil:
            return None  # ceiling arenas: don't decorate the arena; walls handle it
        return lo

    @property
    def end_col(self):
        return max(self.reach) + 1 if self.reach else START_COL


# ---------------------------------------------------------------- cube ----
def _pad(w, c, h, cfg):
    gap = c - w.last_hazard
    if gap < cfg["min_flat"]:
        return w.floor(c, cfg["min_flat"] - gap, h)
    return c


def run_cube(w: World, c: int, h: int, elems: list, cfg: dict):
    for e in elems:
        t = e["t"]
        if t == "flat":
            c = w.floor(c, e["n"], h)
        elif t == "spike":
            n = min(e["n"], cfg["max_spike"])
            c = _pad(w, c, h, cfg)
            w.floor(c, n, h)
            for i in range(n):
                w.put(O.SPIKE, c + i, h)
            c += n
            w.last_hazard = c
        elif t in ("gap", "pad_gap", "orb_gap"):
            extra = {"gap": 0, "pad_gap": 2, "orb_gap": 3}[t]
            n = min(e["w"], cfg["max_gap"] + extra)
            c = _pad(w, c, h, cfg)
            if h == 0:  # no pits at ground level: use a spike run instead
                n = min(n, cfg["max_spike"] + extra)
                if t == "pad_gap":
                    w.put(O.PAD_YELLOW, c - 1, 0)
                    n = min(n, 5)
                start = c
                w.floor(c, n, 0, HEAD + (4 if extra else 0))
                for i in range(n):
                    w.put(O.SPIKE, c + i, 0)
                if t == "orb_gap":
                    w.put(O.ORB_YELLOW, start + n // 2, 2)
                c += n
            else:
                if t == "pad_gap":
                    w.put(O.PAD_YELLOW, c - 1, h)
                for i in range(n):
                    w.set_floor(c + i, 0)
                    w.reach[c + i] = h + HEAD + (4 if extra else 0)
                    w.put(O.SPIKE, c + i, 0)
                if t == "orb_gap":
                    w.put(O.ORB_YELLOW, c + n // 2, h + 2)
                c += n
            w.last_hazard = c
            c = w.floor(c, 2, h)
        elif t == "up":
            c = w.floor(c, 2, h)
            h = min(6, h + min(e["dh"], cfg["max_rise"]))
        elif t == "down":
            h = max(0, h - e["dh"])
        elif t == "platform":
            c = w.floor(c, 2, h)
            h = min(6, h + 1)
            c = w.floor(c, e["n"], h)
            h = max(0, h - 1)
        elif t == "stairs":
            for _ in range(e["n"]):
                c = w.floor(c, 3, h)
                h = min(6, h + 1)
            c = w.floor(c, 3, h)
            h = max(0, h - e["n"])
        elif t == "coin":
            c = w.floor(c, 2, h)
            w.put(O.COIN, c - 1, h + 2)
    return c, h


def gen_cube(style: str, cols: int, density: float, cfg: dict, rng: random.Random) -> list:
    """Turn a structure style into an element list covering about `cols` columns."""
    cols_left, out = cols, []

    def add(e, cost):
        nonlocal cols_left
        out.append(e)
        cols_left -= cost

    flat_lo = 1 + int((1 - density) * 4)
    while cols_left > 0:
        s = style if style != "mixed" else rng.choice(
            ("stairs", "towers", "spike_run", "pit_run", "orb_run", "pad_run"))
        add({"t": "flat", "n": rng.randint(flat_lo, flat_lo + 3)}, 4)
        if s == "stairs":
            n = rng.randint(2, 4)
            add({"t": "stairs", "n": n}, n * 3 + 3)
            add({"t": "spike", "n": rng.randint(1, cfg["max_spike"])}, 4)
        elif s == "towers":
            add({"t": "platform", "n": rng.randint(3, 7)}, 9)
            if rng.random() < density:
                add({"t": "gap", "w": rng.randint(2, cfg["max_gap"])}, 7)
        elif s == "spike_run":
            for _ in range(rng.randint(1, 2 + int(density * 3))):
                add({"t": "spike", "n": rng.randint(1, cfg["max_spike"])}, 6)
        elif s == "pit_run":
            add({"t": "up", "dh": 1}, 3)
            for _ in range(rng.randint(1, 3)):
                add({"t": "flat", "n": rng.randint(3, 5)}, 4)
                add({"t": "gap", "w": rng.randint(2, cfg["max_gap"])}, 7)
            add({"t": "down", "dh": 1}, 1)
        elif s == "orb_run":
            add({"t": "orb_gap", "w": rng.randint(3, cfg["max_gap"] + 3)}, 9)
        elif s == "pad_run":
            add({"t": "pad_gap", "w": rng.randint(2, cfg["max_gap"] + 2)}, 9)
        if rng.random() < 0.15:
            add({"t": "coin", "n": 1}, 2)
    return out


# ------------------------------------------------------ arena modes -----
def _corridor(w: World, c, cols, gap, slope_cols, wobble, rng, pillars=False, spikes=False):
    """Parallel floor/ceiling corridor following a slope-limited random path."""
    top = CEIL_TOP - gap
    fs, f, target = [], 0, 0
    for i in range(cols):
        if i % 6 == 0 or f == target:
            target = rng.randint(0, max(0, min(top, wobble)))
        if i % slope_cols == 0:
            f += (target > f) - (target < f)
        fs.append(max(0, min(top, f)))
    for i in range(cols):  # glide back to the ground at the end, slope-limited
        fs[i] = min(fs[i], (cols - 1 - i) // slope_cols)
    next_pillar = c + 8
    for i in range(cols):
        col = c + i
        f = fs[i]
        w.set_floor(col, f)
        cs = f + gap
        w.ceil[col] = cs
        w.reach[col] = cs - 1
        for r in range(cs, CEIL_TOP + 2):
            role = "top" if r == cs else "fill"
            id_, p = decorate.block_style(role, col, r)
            w.put(id_, col, r, p)
        if pillars and col >= next_pillar and cols - i > 10:
            p_h = rng.randint(1, max(1, gap - 4))
            if rng.random() < 0.5:
                for r in range(f, f + p_h):
                    w.put(O.BLOCK, col, r, {O.K_MAIN_COLOR: O.CH_OBJ, O.K_ZLAYER: O.Z_B1})
            else:
                for r in range(cs - p_h, cs):
                    w.put(O.BLOCK, col, r, {O.K_MAIN_COLOR: O.CH_OBJ, O.K_ZLAYER: O.Z_B1})
            next_pillar = col + rng.randint(6, 9)
        elif spikes and gap >= 5 and col % 7 == 0 and 3 < i < cols - 6:
            if rng.random() < 0.5:
                w.put(O.SPIKE_SMALL, col, f)
            else:
                w.put(O.SPIKE_SMALL, col, cs - 1, {O.K_ROT: 180})
    return c + cols


def _ball(w: World, c, cols, density, rng):
    gap = 5
    last_side, hazard_at = 0, c + 4
    for i in range(cols):
        col = c + i
        w.set_floor(col, 0)
        w.ceil[col] = gap
        w.reach[col] = gap - 1
        for r in range(gap, CEIL_TOP + 2):
            id_, p = decorate.block_style("top" if r == gap else "fill", col, r)
            w.put(id_, col, r, p)
        if col >= hazard_at and i < cols - 6:
            side = rng.choice((0, 1)) if rng.random() < 0.7 else last_side
            n = rng.randint(1, 2)
            for k in range(n):
                if side == 0:
                    w.put(O.SPIKE, col + k, 0)
                else:
                    w.put(O.SPIKE, col + k, gap - 1, {O.K_ROT: 180})
            last_side = side
            hazard_at = col + n + (7 if side != last_side else 6) + int((1 - density) * 4)
    return c + cols


def build_arena(w, mode, c, cols, cfg, sec, rng):
    d = sec["density"]
    if mode == "ship":
        gap = cfg["ship_gap"] + (1 if sec["mood"] == "calm" else 0)
        return _corridor(w, c, cols, gap, 2, 2 + int(d * 4), rng, pillars=d > 0.55, spikes=d > 0.4)
    if mode == "wave":
        return _corridor(w, c, cols, cfg["wave_gap"], 1, 2 + int(d * 5), rng)
    if mode == "ufo":
        return _corridor(w, c, cols, 7, 3, 2 + int(d * 3), rng, pillars=True)
    return _ball(w, c, cols, d, rng)


# --------------------------------------------------------- compiler -----
def triggers_for_mood(theme, mood, x, prev_pal=None):
    pal = decorate.palette(theme, mood)
    out = []
    for ch, rgb in pal.items():
        if prev_pal is not None and prev_pal.get(ch) == rgb:
            continue
        out.append(Obj(O.COLOR_TRIGGER, x, 30 * 16 + 15, {
            O.K_R: rgb[0], O.K_G: rgb[1], O.K_B: rgb[2], O.K_DUR: 1.0,
            O.K_TARGET_COLOR: ch, O.K_OPACITY: 1, O.K_HIGH_DETAIL: 1}))
    return pal, out


def pulse_triggers(c0, c1, bpm, ups, every_bars=1):
    bar_cols = (4 * 60.0 / bpm) * ups / 30.0
    out, c = [], float(c0)
    while c < c1:
        out.append(Obj(O.PULSE_TRIGGER, c * 30 + 15, 30 * 17 + 15, {
            O.K_R: 255, O.K_G: 255, O.K_B: 255, O.K_PULSE_IN: 0, O.K_PULSE_HOLD: 0.05,
            O.K_PULSE_OUT: 0.35, O.K_PULSE_MODE: 0, O.K_PULSE_TARGET: O.CH_EDGE,
            O.K_PULSE_TARGET_TYPE: 0, O.K_HIGH_DETAIL: 1}))
        c += bar_cols * every_bars
    return out


def compile_level(plan: dict, seed=None):
    rng = random.Random(seed)
    w = World()
    theme, diff = plan["theme"], plan["difficulty"]
    c, h, mode = START_COL, 0, "cube"
    cur_speed = None
    first_speed = plan["sections"][0]["speed"]
    prev_pal, extras = None, []
    c = w.floor(c, 8, 0)
    deco_ranges = []

    for sec in plan["sections"]:
        cfg = difficulty_cfg(diff, sec["speed"])
        ups = SPEEDS[sec["speed"]][1]
        bar_cols = max(4, int((4 * 60.0 / plan["bpm"]) * ups / 30.0))
        cols = sec["bars"] * bar_cols
        start = c

        if sec["speed"] != cur_speed and not (cur_speed is None and sec["speed"] == first_speed):
            w.put(SPEEDS[sec["speed"]][0], c, 2)
        cur_speed = sec["speed"]

        if sec["gamemode"] != mode:
            if h:  # drop to ground before a mode change
                c, h = run_cube(w, c, h, [{"t": "down", "dh": h}], cfg)
            c = w.floor(c, 3, 0)
            w.put(O.PORTAL[sec["gamemode"]], c, 2 if sec["gamemode"] == "cube" else 3)
            c = w.floor(c, 2, 0)
            mode = sec["gamemode"]
            start = c

        prev_pal, trig = triggers_for_mood(theme, sec["mood"], start * 30 + 15, prev_pal)
        extras += trig

        if mode == "cube":
            elems = sec["elements"] or gen_cube(sec["style"], cols, sec["density"], cfg, rng)
            c, h = run_cube(w, c, h, elems, cfg)
            c = w.floor(c, cfg["min_flat"], h)
        else:
            c = w.floor(c, 3, 0, CEIL_TOP)
            c = build_arena(w, mode, c, cols, cfg, sec, rng)
            c = w.floor(c, 3, 0, CEIL_TOP)
        extras += pulse_triggers(start, c, plan["bpm"], ups, 1 if sec["mood"] == "drop" else 2)
        deco_ranges.append((start, c, sec["mood"]))

    if h:
        c, h = run_cube(w, c, h, [{"t": "down", "dh": h}], cfg)
    if mode != "cube":
        w.put(O.PORTAL["cube"], c, 2)
        c = w.floor(c, 3, 0)
    c = w.floor(c, 8, 0)
    extras.append(Obj(O.END_TRIGGER, c * 30 + 15, 30 * 3 + 15, {}))

    deco = []
    for a, b, mood in deco_ranges:
        deco += decorate.decorate_sky(w, rng, a, b, mood)

    channels = decorate.palette(theme, plan["sections"][0]["mood"])
    header = build_header(channels, start_speed=SPEEDS[first_speed][2],
                          bg=rng.randint(1, 20), ground=rng.randint(1, 17))
    objs = w.objs + deco + extras
    return encode_level(header, objs), objs, w
