"""Persistent settings: API key and model. Stdlib only so it runs in Termux."""
import getpass
import json
import os
import sys
from pathlib import Path

DEFAULT_MODEL = "gemini-flash-latest"  # free-tier Gemini Flash alias


def config_path() -> Path:
    base = os.environ.get("SILICATE_HOME")
    if base:
        return Path(base) / "config.json"
    xdg = os.environ.get("XDG_CONFIG_HOME") or str(Path.home() / ".config")
    return Path(xdg) / "silicate" / "config.json"


def load() -> dict:
    try:
        return json.loads(config_path().read_text())
    except (OSError, ValueError):
        return {}


def save(cfg: dict) -> None:
    p = config_path()
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text(json.dumps(cfg, indent=2))
    try:
        os.chmod(p, 0o600)
    except OSError:
        pass


def get_model(cli_model=None) -> str:
    return cli_model or os.environ.get("SILICATE_MODEL") or load().get("model") or DEFAULT_MODEL


def get_api_key(cli_key=None, interactive=True):
    """Resolve key: --api-key > $GEMINI_API_KEY > saved config > prompt."""
    key = cli_key or os.environ.get("GEMINI_API_KEY") or load().get("api_key")
    if key:
        return key.strip()
    if not interactive or not sys.stdin.isatty():
        return None
    print("A Gemini API key is required (free at https://aistudio.google.com/apikey).")
    key = getpass.getpass("Enter Gemini API key: ").strip()
    if not key:
        return None
    if input("Save it to %s? [Y/n] " % config_path()).strip().lower() in ("", "y", "yes"):
        cfg = load()
        cfg["api_key"] = key
        save(cfg)
    return key
