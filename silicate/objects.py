"""Geometry Dash object IDs and property keys.

Everything the generator emits comes from this one table. IDs are the
well-known 2.x ones; if one is wrong for your build, fix it here only.
"""

# --- solid / hazard -------------------------------------------------------
BLOCK = 1
SPIKE = 8          # full spike
SPIKE_SMALL = 39   # short ground spike

# --- portals --------------------------------------------------------------
PORTAL = {"cube": 12, "ship": 13, "ball": 47, "ufo": 111, "wave": 660}
PORTAL_GRAV_DOWN, PORTAL_GRAV_UP = 10, 11

# --- speed portals: name -> (portal id, units/sec, header kA4 value) -------
SPEEDS = {
    "0.5x": (200, 251.16, 1),
    "1x": (201, 311.58, 0),
    "2x": (202, 387.42, 2),
    "3x": (203, 468.0, 3),
    "4x": (1334, 576.0, 4),
}

# --- pads / orbs / misc ---------------------------------------------------
PAD_YELLOW = 35
ORB_YELLOW = 36
COIN = 1329
COLOR_TRIGGER = 899
PULSE_TRIGGER = 1006
END_TRIGGER = 3600

# --- property keys --------------------------------------------------------
K_ROT, K_R, K_G, K_B, K_DUR = 6, 7, 8, 9, 10
K_MAIN_COLOR, K_DETAIL_COLOR, K_TARGET_COLOR = 21, 22, 23
K_ZLAYER, K_ZORDER, K_SCALE, K_OPACITY = 24, 25, 32, 35
K_PULSE_IN, K_PULSE_HOLD, K_PULSE_OUT, K_PULSE_MODE = 45, 46, 47, 48
K_PULSE_TARGET, K_PULSE_TARGET_TYPE = 51, 52
K_HIGH_DETAIL, K_NO_TOUCH = 103, 121

# z-layers (bottom to top)
Z_B4, Z_B3, Z_B2, Z_B1, Z_T1 = -3, -1, 1, 3, 5

# Color channels: specials + custom channels used by the decorator
CH_BG, CH_GROUND, CH_LINE, CH_OBJ = 1000, 1001, 1002, 1004
CH_FILL, CH_FILL2, CH_DEC1, CH_DEC2, CH_EDGE = 1, 2, 3, 4, 5
