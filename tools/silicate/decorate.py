"""Themes and decoration.

Rule that matters: solid decoration is only placed in cells the player can never
reach (above the jump envelope, inside walls, above ceilings). Block hitboxes
ignore z-layer, so decorating the playable space would kill or block the player.
"""
import math
import random

from . import objects as O
from .gdformat import Obj

THEMES = {
    #           bg            ground        line          obj           fill          fill2         dec1          dec2          edge
    "neon": [(10, 8, 40), (20, 14, 70), (0, 255, 255), (30, 30, 90), (20, 20, 70), (28, 26, 96), (60, 20, 140), (0, 160, 200), (0, 255, 220)],
    "fire": [(40, 6, 6), (60, 14, 8), (255, 160, 0), (110, 24, 10), (70, 14, 8), (90, 20, 10), (160, 40, 10), (255, 110, 0), (255, 210, 60)],
    "ice": [(10, 30, 60), (20, 50, 90), (200, 240, 255), (50, 110, 170), (30, 70, 120), (40, 90, 140), (90, 160, 220), (160, 220, 255), (240, 255, 255)],
    "forest": [(8, 30, 16), (14, 46, 20), (150, 255, 120), (30, 90, 40), (20, 60, 30), (26, 76, 36), (40, 120, 60), (110, 200, 90), (200, 255, 150)],
    "void": [(4, 4, 8), (12, 12, 20), (200, 200, 255), (40, 40, 60), (24, 24, 36), (32, 32, 48), (70, 40, 120), (110, 110, 170), (230, 230, 255)],
    "sunset": [(60, 20, 60), (90, 30, 70), (255, 140, 100), (150, 50, 80), (100, 30, 70), (120, 40, 80), (220, 80, 90), (255, 170, 80), (255, 230, 150)],
    "candy": [(60, 20, 80), (90, 40, 110), (255, 150, 230), (200, 90, 190), (140, 60, 150), (170, 80, 170), (255, 120, 200), (120, 220, 255), (255, 255, 255)],
}
CHANNEL_ORDER = [O.CH_BG, O.CH_GROUND, O.CH_LINE, O.CH_OBJ, O.CH_FILL, O.CH_FILL2, O.CH_DEC1, O.CH_DEC2, O.CH_EDGE]


def _mix(a, b, t):
    return tuple(int(round(x + (y - x) * t)) for x, y in zip(a, b))


def palette(theme: str, mood: str) -> dict:
    base = dict(zip(CHANNEL_ORDER, THEMES[theme]))
    edge = base[O.CH_EDGE]
    out = {}
    for ch, c in base.items():
        if mood == "calm":
            c = _mix(c, (0, 0, 0), 0.25)
        elif mood == "build":
            c = _mix(c, edge, 0.12) if ch not in (O.CH_BG, O.CH_GROUND) else c
        elif mood == "drop":
            c = _mix(c, edge, 0.3) if ch not in (O.CH_BG,) else _mix(c, (0, 0, 0), 0.35)
        elif mood == "break":
            g = sum(c) // 3
            c = _mix(c, (g, g, g), 0.5)
        out[ch] = c
    return out


def block_style(role: str, c: int, r: int, depth: int = 0):
    """(object id, props) for a solid tile. role: top | fill | wall_edge."""
    if role == "top":
        return O.BLOCK, {O.K_MAIN_COLOR: O.CH_EDGE, O.K_ZLAYER: O.Z_B1}
    if role == "edge":
        return O.BLOCK, {O.K_MAIN_COLOR: O.CH_OBJ, O.K_ZLAYER: O.Z_B2}
    ch = O.CH_FILL if (c + r) % 2 == 0 else O.CH_FILL2
    if c % 4 == 0:
        ch = O.CH_DEC1  # support-column look
    elif depth and depth % 5 == 0:
        ch = O.CH_OBJ
    return O.BLOCK, {O.K_MAIN_COLOR: ch, O.K_ZLAYER: O.Z_B3}


def _deco(id_, x, y, ch, z, scale=None, rot=None):
    p = {O.K_MAIN_COLOR: ch, O.K_ZLAYER: z, O.K_HIGH_DETAIL: 1, O.K_NO_TOUCH: 1}
    if scale and scale != 1:
        p[O.K_SCALE] = scale
    if rot:
        p[O.K_ROT] = rot
    return Obj(id_, x, y, p)


def decorate_sky(world, rng: random.Random, c0: int, c1: int, mood: str):
    """Hanging diamond chains + floating diamonds, only above the reachable zone."""
    out = []
    c = c0 + rng.randint(2, 5)
    while c < c1:
        lo = world.deco_floor(c)
        if lo is None:
            c += 3
            continue
        kind = rng.random()
        if kind < 0.45:  # hanging chain, tapering diamonds
            top = lo + rng.randint(6, 9)
            n = rng.randint(3, 6)
            for i in range(n):
                s = 0.35 + 0.1 * i
                out.append(_deco(O.BLOCK, c * 30 + 15, (top - i) * 30 + 15, O.CH_DEC1, O.Z_B4, s, 45))
            out.append(_deco(O.BLOCK, c * 30 + 15, (top - n) * 30 + 15, O.CH_EDGE, O.Z_B3, 0.9, 45))
        elif kind < 0.8:  # floating twinkles
            for _ in range(rng.randint(2, 4)):
                cc = c + rng.randint(-2, 2)
                rr = world.deco_floor(cc)
                if rr is None:
                    continue
                y = (rr + rng.randint(1, 8)) * 30 + 15
                out.append(_deco(O.BLOCK, cc * 30 + 15, y, rng.choice((O.CH_DEC2, O.CH_EDGE)), O.Z_B3, rng.choice((0.3, 0.5, 0.7)), 45))
        else:  # tall back pillar with cap
            h = rng.randint(3, 5)
            for i in range(h):
                out.append(_deco(O.BLOCK, c * 30 + 15, (lo + 5 + i) * 30 + 15, O.CH_DEC1, O.Z_B4, 0.6))
            out.append(_deco(O.BLOCK, c * 30 + 15, (lo + 5 + h) * 30 + 15, O.CH_DEC2, O.Z_B4, 0.9, 45))
        c += rng.randint(4, 9) if mood != "calm" else rng.randint(7, 13)
    return out
