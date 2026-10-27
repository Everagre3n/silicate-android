"""Geometry Dash level string / .gmd encoding and decoding."""
import base64
import gzip
from dataclasses import dataclass, field
from xml.sax.saxutils import escape


def _fmt(v):
    if isinstance(v, bool):
        return "1" if v else "0"
    if isinstance(v, float):
        return ("%.3f" % v).rstrip("0").rstrip(".")
    return str(v)


@dataclass
class Obj:
    id: int
    x: float
    y: float
    props: dict = field(default_factory=dict)

    def encode(self) -> str:
        parts = ["1", str(self.id), "2", _fmt(self.x), "3", _fmt(self.y)]
        for k in sorted(self.props):
            parts += [str(k), _fmt(self.props[k])]
        return ",".join(parts) + ";"


def color_entry(channel, rgb, blending=False):
    r, g, b = rgb
    return "1_%d_2_%d_3_%d_11_255_12_255_13_255_4_-1_6_%d_7_1_15_1_18_0_8_1" % (r, g, b, channel)


def build_header(channels: dict, start_speed=0, start_mode=0, bg=1, ground=1) -> str:
    colors = "|".join(color_entry(ch, rgb) for ch, rgb in sorted(channels.items())) + "|"
    return (
        "kS38,%s,kA13,0,kA15,0,kA16,0,kA14,,kA6,%d,kA7,%d,kA25,0,kA17,0,kA18,0,kS39,0,"
        "kA2,%d,kA3,0,kA8,0,kA4,%d,kA9,0,kA10,0,kA22,0,kA23,0,kA24,0,kA27,1,kA40,1,"
        "kA41,1,kA42,1,kA28,0,kA29,0,kA31,1,kA32,1,kA36,0,kA43,0,kA44,0,kA45,1,kA46,0,"
        "kA33,1,kA34,1,kA35,0,kA37,1,kA38,1,kA39,1,kA19,0,kA26,0,kA20,0,kA21,0,kA11,0;"
    ) % (colors, bg, ground, start_mode, start_speed)


def encode_level(header: str, objs) -> str:
    raw = (header + "".join(o.encode() for o in objs)).encode()
    return base64.urlsafe_b64encode(gzip.compress(raw, mtime=0)).decode()


def decode_level(data: str):
    raw = gzip.decompress(base64.urlsafe_b64decode(data + "=" * (-len(data) % 4))).decode()
    chunks = raw.split(";")
    objs = []
    for ch in chunks[1:]:
        if not ch:
            continue
        f = ch.split(",")
        kv = dict(zip(f[0::2], f[1::2]))
        props = {int(k): v for k, v in kv.items() if k not in ("1", "2", "3")}
        objs.append(Obj(int(kv["1"]), float(kv["2"]), float(kv["3"]), props))
    return chunks[0], objs


def to_gmd(name: str, level_string: str, author="silicate", desc="") -> str:
    return (
        '<?xml version="1.0"?><plist version="1.0" gjver="2.0"><dict>'
        "<k>kCEK</k><i>4</i><k>k2</k><s>%s</s><k>k3</k><s>%s</s><k>k4</k><s>%s</s>"
        "<k>k5</k><s>%s</s><k>k13</k><t /><k>k21</k><i>2</i><k>k50</k><i>35</i>"
        "</dict></plist>"
    ) % (escape(name), base64.b64encode(escape(desc).encode()).decode(), level_string, escape(author))
