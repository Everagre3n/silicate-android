import json
import unittest
from unittest import mock

from silicate import objects as O
from silicate import planner
from silicate.builder import compile_level
from silicate.gdformat import Obj, build_header, decode_level, encode_level
from silicate.spec import difficulty_cfg, sanitize


def build(seed=1, diff="hard", theme="neon"):
    plan = planner.offline_plan(seed, diff, theme, 96)
    return plan, compile_level(plan, seed)


class Tests(unittest.TestCase):
    def test_roundtrip(self):
        objs = [Obj(1, 15, 15, {24: -1}), Obj(8, 45.5, 15)]
        h, back = decode_level(encode_level(build_header({1000: (1, 2, 3)}), objs))
        self.assertEqual([(o.id, o.x, o.y) for o in back], [(1, 15, 15), (8, 45.5, 15)])
        self.assertEqual(back[0].props, {24: "-1"})

    def test_deterministic(self):
        self.assertEqual(build(5)[1][0], build(5)[1][0])

    def test_solid_decor_never_in_playable_space(self):
        for seed in range(8):
            _, (_, objs, w) = build(seed)
            for o in objs:
                if o.props.get(O.K_NO_TOUCH):
                    col, row = int(o.x // 30), int(o.y // 30)
                    self.assertGreater(row, w.reach[col], (seed, col, row))

    def test_no_overlapping_solids(self):
        for seed in range(8):
            _, (_, objs, _) = build(seed)
            seen = set()
            for o in objs:
                if o.id in (O.BLOCK, O.SPIKE, O.SPIKE_SMALL) and not o.props.get(O.K_NO_TOUCH):
                    if o.id == O.BLOCK and o.props.get(O.K_SCALE):
                        continue
                    key = (o.x, o.y, o.id == O.BLOCK)
                    self.assertNotIn(key, seen, (seed, key))
                    seen.add(key)

    def test_pits_obey_jump_limits(self):
        cfg = difficulty_cfg("normal", "1x")
        plan = sanitize({"sections": [{"gamemode": "cube", "bars": 4, "elements": [
            {"t": "up", "dh": 2}, {"t": "gap", "w": 6}, {"t": "spike", "n": 3}]}], "difficulty": "normal"})
        _, objs, w = compile_level(plan, 1)
        pit_spikes = [o for o in objs if o.id == O.SPIKE and o.y == 15]
        self.assertLessEqual(len(pit_spikes), cfg["max_gap"] + cfg["max_spike"])

    def test_every_mode_builds_and_returns_to_ground(self):
        plan = sanitize({"difficulty": "insane", "sections": [
            {"gamemode": m, "bars": 6, "density": 0.9} for m in ("cube", "ship", "wave", "ball", "ufo", "cube")]})
        _, objs, w = compile_level(plan, 2)
        ids = {o.id for o in objs}
        self.assertTrue({13, 660, 47, 111}.issubset(ids))
        self.assertEqual(w.floor_h[max(w.floor_h)], 0)

    def test_sanitize_clamps_garbage(self):
        p = sanitize({"bpm": 9999, "theme": "x", "sections": [
            {"gamemode": "robot", "speed": "9x", "bars": 999,
             "elements": [{"t": "gap", "w": 99}, {"t": "nuke"}]}]})
        s = p["sections"][0]
        self.assertEqual((p["bpm"], s["gamemode"], s["speed"], s["bars"]), (220, "cube", "1x", 32))
        self.assertEqual(s["elements"], [{"t": "gap", "w": 6}])

    def test_ai_plan_with_mocked_gemini(self):
        reply = {"candidates": [{"content": {"parts": [{"text": "```json\n" + json.dumps({
            "name": "Mock", "theme": "ice", "difficulty": "easy", "bpm": 120,
            "sections": [{"gamemode": "cube", "mood": "calm", "bars": 4}]}) + "\n```"}]}}]}
        with mock.patch("silicate.llm._post", return_value=reply) as post:
            plan = planner.ai_plan("icy easy", "KEY", "gemini-flash-latest")
        self.assertEqual(plan["theme"], "ice")
        self.assertEqual(post.call_args[0][1], "gemini-flash-latest")
        compile_level(plan, 1)


if __name__ == "__main__":
    unittest.main()
