"""
Imperium Bot — Database layer (GitHub Gist backend)

All data lives in a single private GitHub Gist as a JSON file.
Set these in your environment / Render dashboard:
  GIST_ID          — the ID of your private gist (from the URL)
  GITHUB_TOKEN     — a personal access token with the `gist` scope

The gist must contain a single file named `imperium_db.json`.
If the file doesn't exist yet, the first write will create it.
"""

import json
import hashlib
import os
import urllib.request
import urllib.error
from datetime import datetime, timezone
from typing import Optional

GIST_ID      = os.getenv("GIST_ID", "")
GITHUB_TOKEN = os.getenv("GITHUB_TOKEN", "")
GIST_FILENAMES = ["imperium_db.json", "imprmdb.json"]
GIST_FILENAME = GIST_FILENAMES[0]

print(
    f"[DB] GIST_ID set: {bool(GIST_ID)}, GITHUB_TOKEN set: {bool(GITHUB_TOKEN)}, "
    f"supported filenames: {GIST_FILENAMES}",
    flush=True,
)

_HEADERS = {
    "Authorization": f"token {GITHUB_TOKEN}",
    "Accept":        "application/vnd.github+json",
    "Content-Type":  "application/json",
    "User-Agent":    "ImperiumBot/1.0",
}


# ─── Gist I/O ─────────────────────────────────────────────────────────────────

def _gist_url() -> str:
    return f"https://api.github.com/gists/{GIST_ID}"


def _load() -> dict:
    """Fetch the current DB from the Gist. Returns empty schema on any error."""
    if not GIST_ID or not GITHUB_TOKEN:
        print("[DB] _load missing GIST_ID or GITHUB_TOKEN", flush=True)
        return {"keys": {}}
    try:
        req = urllib.request.Request(_gist_url(), headers=_HEADERS, method="GET")
        with urllib.request.urlopen(req, timeout=10) as resp:
            gist = json.loads(resp.read().decode("utf-8"))
        files = gist.get("files", {})
        filename = next((name for name in GIST_FILENAMES if name in files), None)
        if not filename:
            if len(files) == 1:
                filename = next(iter(files))
                print(f"[DB] _load using single gist file: {filename}", flush=True)
            else:
                return {"keys": {}}
        else:
            print(f"[DB] _load using configured gist file: {filename}", flush=True)
        content = files[filename].get("content", "{}")
        return json.loads(content)
    except Exception as e:
        print(f"[DB] _load error: {e}", flush=True)
        return {"keys": {}}


def _save(data: dict):
    """Push updated DB back to the Gist."""
    if not GIST_ID or not GITHUB_TOKEN:
        print("[DB] GIST_ID or GITHUB_TOKEN not set — skipping save.", flush=True)
        return
    filename = None
    try:
        req = urllib.request.Request(_gist_url(), headers=_HEADERS, method="GET")
        with urllib.request.urlopen(req, timeout=10) as resp:
            gist = json.loads(resp.read().decode("utf-8"))
        files = gist.get("files", {})
        filename = next((name for name in GIST_FILENAMES if name in files), None)
        if not filename:
            filename = next(iter(files), GIST_FILENAME)
    except Exception:
        filename = GIST_FILENAME

    print(f"[DB] _save using gist file: {filename}", flush=True)
    payload = json.dumps({
        "files": {
            filename: {
                "content": json.dumps(data, indent=2)
            }
        }
    }).encode("utf-8")
    try:
        req = urllib.request.Request(
            _gist_url(), data=payload, headers=_HEADERS, method="PATCH"
        )
        with urllib.request.urlopen(req, timeout=10):
            pass
    except Exception as e:
        print(f"[DB] _save error: {e}", flush=True)


def _now() -> str:
    return datetime.now(timezone.utc).isoformat()


# ─── Password hashing (SHA-256) ───────────────────────────────────────────────

def hash_password(password: str) -> str:
    return hashlib.sha256(password.encode("utf-8")).hexdigest()


def verify_password(password: str, hashed: str) -> bool:
    return hash_password(password) == hashed


# ─── Key helpers ──────────────────────────────────────────────────────────────

def key_exists(key: str) -> bool:
    return key in _load()["keys"]


def get_key(key: str) -> Optional[dict]:
    return _load()["keys"].get(key)


def get_all_keys() -> dict:
    return _load()["keys"]


def create_key(key: str, duration: str, generated_by: int) -> dict:
    data = _load()
    record = {
        "duration":           duration,
        "generated_by":       generated_by,
        "generated_at":       _now(),
        "disabled":           False,
        "claimed_by_discord": None,
        "username":           None,
        "password_hash":      None,
        "hwid":               None,
        "registered_at":      None,
    }
    data["keys"][key] = record
    _save(data)
    return record


def disable_key(key: str) -> bool:
    data = _load()
    if key not in data["keys"]:
        return False
    data["keys"][key]["disabled"] = True
    _save(data)
    return True


def enable_key(key: str) -> bool:
    data = _load()
    if key not in data["keys"]:
        return False
    data["keys"][key]["disabled"] = False
    _save(data)
    return True


def delete_key(key: str) -> bool:
    data = _load()
    if key not in data["keys"]:
        return False
    del data["keys"][key]
    _save(data)
    return True


def delete_keys(keys: list[str]) -> tuple[list[str], list[str]]:
    """Returns (deleted, not_found)."""
    data = _load()
    deleted, not_found = [], []
    for k in keys:
        if k in data["keys"]:
            del data["keys"][k]
            deleted.append(k)
        else:
            not_found.append(k)
    _save(data)
    return deleted, not_found


def update_key(key: str, **kwargs) -> bool:
    data = _load()
    if key not in data["keys"]:
        return False
    data["keys"][key].update(kwargs)
    _save(data)
    return True


def register_key(key: str, username: str, password: str, discord_id: int) -> tuple[bool, str]:
    """
    Claim a key for the first time.
    Returns (success, error_message).
    """
    data = _load()
    if key not in data["keys"]:
        return False, "Key not found."
    rec = data["keys"][key]
    if rec["disabled"]:
        return False, "This key has been disabled."
    if rec["claimed_by_discord"] is not None:
        return False, "This key has already been claimed."
    # Enforce username uniqueness
    for v in data["keys"].values():
        if v.get("username") and v["username"].lower() == username.lower():
            return False, "That username is already taken."
    rec["claimed_by_discord"] = discord_id
    rec["username"]           = username
    rec["password_hash"]      = hash_password(password)
    rec["registered_at"]      = _now()
    _save(data)
    return True, ""


def reset_hwid(key: str) -> bool:
    data = _load()
    if key not in data["keys"]:
        return False
    data["keys"][key]["hwid"] = None
    _save(data)
    return True


# ─── Auth (called by the HTTP /auth endpoint) ─────────────────────────────────

def authenticate(username: str, password: str, hwid: str) -> tuple[bool, str]:
    """
    Validates credentials and manages HWID binding.
    Returns (success, message).
    """
    data = _load()
    for key, rec in data["keys"].items():
        if rec.get("username") and rec["username"].lower() == username.lower():
            if rec["disabled"]:
                return False, "Your key has been disabled. Contact support."
            if not verify_password(password, rec["password_hash"]):
                return False, "Invalid password."
            if rec["hwid"] is None:
                # First launch — bind HWID
                rec["hwid"] = hwid
                _save(data)
                return True, "OK"
            elif rec["hwid"] != hwid:
                return False, "HWID mismatch. Contact support to reset."
            return True, "OK"
    return False, "Username not found."

# ─── Download URL (stored in Gist, set via /setdownload) ─────────────────────

async def get_download_url() -> str:
    data = _load()
    return data.get("download_url", "")


async def set_download_url(url: str):
    data = _load()
    data["download_url"] = url
    _save(data)
