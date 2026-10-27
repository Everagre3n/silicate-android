import argparse
import json
import sys
from pathlib import Path

from . import config, planner
from .builder import compile_level
from .gdformat import decode_level, to_gmd
from .spec import DIFFICULTIES, THEMES


def cmd_config(a):
    cfg = config.load()
    if a.api_key:
        cfg["api_key"] = a.api_key
    if a.model:
        cfg["model"] = a.model
    if a.api_key or a.model:
        config.save(cfg)
    k = cfg.get("api_key")
    print("config file:", config.config_path())
    print("model      :", cfg.get("model") or config.DEFAULT_MODEL + " (default)")
    print("api key    :", ("…" + k[-4:]) if k else "not set (you will be asked on first use)")


def cmd_generate(a):
    model = config.get_model(a.model)
    if a.offline:
        plan = planner.offline_plan(a.seed, a.difficulty or "normal", a.theme, a.bars or 64)
    else:
        key = config.get_api_key(a.api_key)
        if not key:
            sys.exit("No API key. Pass --api-key, set GEMINI_API_KEY, or run `silicate config --api-key KEY`.")
        print("Asking %s for a plan…" % model)
        try:
            plan = planner.ai_plan(" ".join(a.prompt), key, model, a.difficulty, a.theme, a.bars)
        except Exception as e:  # LLMError / bad plan
            sys.exit("Generation failed: %s" % e)
    data, objs, _ = compile_level(plan, a.seed)
    out = Path(a.out)
    out.mkdir(parents=True, exist_ok=True)
    stem = "".join(ch if ch.isalnum() else "_" for ch in plan["name"]) or "level"
    (out / (stem + ".gmd")).write_text(to_gmd(plan["name"], data))
    (out / (stem + ".txt")).write_text(data)
    (out / (stem + ".plan.json")).write_text(json.dumps(plan, indent=2))
    print("'%s' (%s, %s, %d bpm): %d sections, %d objects" % (
        plan["name"], plan["theme"], plan["difficulty"], plan["bpm"], len(plan["sections"]), len(objs)))
    print("wrote %s.{gmd,txt,plan.json} in %s" % (stem, out))


def cmd_inspect(a):
    text = Path(a.file).read_text()
    if "<s>" in text:
        text = text.split("<k>k4</k><s>")[1].split("</s>")[0]
    header, objs = decode_level(text.strip())
    counts = {}
    for o in objs:
        counts[o.id] = counts.get(o.id, 0) + 1
    print("%d objects, length %d blocks" % (len(objs), max(o.x for o in objs) // 30))
    for i, n in sorted(counts.items(), key=lambda kv: -kv[1]):
        print("  id %-5d x%d" % (i, n))


def main(argv=None):
    p = argparse.ArgumentParser(prog="silicate", description="AI Geometry Dash level generator")
    sub = p.add_subparsers(dest="cmd", required=True)
    g = sub.add_parser("generate", help="generate a level from a text prompt")
    g.add_argument("prompt", nargs="*", help="describe the level")
    g.add_argument("--model", help="Gemini model id (default: %s)" % config.DEFAULT_MODEL)
    g.add_argument("--api-key")
    g.add_argument("--difficulty", choices=DIFFICULTIES)
    g.add_argument("--theme", choices=THEMES)
    g.add_argument("--bars", type=int, help="approximate total length in bars")
    g.add_argument("--seed", type=int)
    g.add_argument("--offline", action="store_true", help="skip the AI, use the random planner")
    g.add_argument("--out", default="out")
    g.set_defaults(fn=cmd_generate)
    c = sub.add_parser("config", help="show/set model and API key")
    c.add_argument("--model")
    c.add_argument("--api-key")
    c.set_defaults(fn=cmd_config)
    i = sub.add_parser("inspect", help="summarise a .gmd or level-string file")
    i.add_argument("file")
    i.set_defaults(fn=cmd_inspect)
    a = p.parse_args(argv)
    a.fn(a)


if __name__ == "__main__":
    main()
