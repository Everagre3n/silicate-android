"""Minimal Gemini REST client (generateContent, JSON mode)."""
import json
import re
import urllib.error
import urllib.parse
import urllib.request

API = "https://generativelanguage.googleapis.com/v1beta"


class LLMError(RuntimeError):
    pass


def _post(api_key, model, body, timeout):
    url = "%s/models/%s:generateContent" % (API, urllib.parse.quote(model, safe="-._"))
    req = urllib.request.Request(
        url,
        data=json.dumps(body).encode(),
        headers={"Content-Type": "application/json", "x-goog-api-key": api_key},
    )
    try:
        with urllib.request.urlopen(req, timeout=timeout) as r:
            return json.loads(r.read())
    except urllib.error.HTTPError as e:
        try:
            msg = json.loads(e.read())["error"]["message"]
        except Exception:
            msg = e.reason
        hint = {
            400: " (check the API key and model name)",
            403: " (key rejected or not allowed for this model)",
            404: " (unknown model name)",
            429: " (free-tier rate limit; wait a minute or pick another model)",
        }.get(e.code, "")
        raise LLMError("Gemini HTTP %d: %s%s" % (e.code, msg, hint)) from None
    except urllib.error.URLError as e:
        raise LLMError("Network error: %s" % e.reason) from None


def extract_json(text: str):
    text = text.strip()
    text = re.sub(r"^```(?:json)?\s*|\s*```$", "", text)
    try:
        return json.loads(text)
    except ValueError:
        i, j = text.find("{"), text.rfind("}")
        if i >= 0 and j > i:
            return json.loads(text[i : j + 1])
        raise


def generate_json(api_key, model, system, prompt, temperature=0.9, timeout=120, retries=2):
    body = {
        "systemInstruction": {"parts": [{"text": system}]},
        "contents": [{"role": "user", "parts": [{"text": prompt}]}],
        "generationConfig": {"temperature": temperature, "responseMimeType": "application/json"},
    }
    last = None
    for _ in range(retries + 1):
        data = _post(api_key, model, body, timeout)
        try:
            parts = data["candidates"][0]["content"]["parts"]
            text = "".join(p.get("text", "") for p in parts if not p.get("thought"))
            return extract_json(text)
        except (KeyError, IndexError, ValueError) as e:
            last = e
    raise LLMError("Model did not return valid JSON: %r" % (last,))
