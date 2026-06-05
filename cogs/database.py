"""
Imperium Bot — Database layer (GitHub Gist backend)

All data lives in a single private GitHub Gist as a JSON file.
Set these in your environment / Render dashboard:
  GIST_ID          — the ID of your private gist (from the URL)
  GITHUB_TOKEN     — a personal access token with the `gist` scope
  GITHUB_REPO      — owner/repo for Release hosting, e.g. "markjones22347-ops/imprm"
                     (used by /sethookloaderdll to host the DLL as a Release asset)

The gist must contain a single file named `imperium_db.json`.
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
GITHUB_REPO  = os.getenv("GITHUB_REPO", "markjones22347-ops/imprm")
GIST_FILENAMES = ["imperium_db.json", "imprmdb.json"]
GIST_FILENAME  = GIST_FILENAMES[0]

print(
    f"[DB] GIST_ID set: {bool(GIST_ID)}, GITHUB_TOKEN set: {bool(GITHUB_TOKEN)}, "
    f"GITHUB_REPO: {GITHUB_REPO}, supported filenames: {GIST_FILENAMES}",
    flush=True,
)

_HEADERS = {
    "Authorization": f"token {GITHUB_TOKEN}",
    "Accept":        "application/vnd.github+json",
    "Content-Type":  "application/json",
    "User-Agent":    "ImperiumBot/1.0",
}

# ─── Default product catalogue (seeded if absent from Gist) ──────────────────
# Each entry: { "display_name": str, "url": str }
DEFAULT_PRODUCTS: dict[str, dict] = {
    "imperium":  {"display_name": "Imperium",       "url": ""},
    "emu":       {"display_name": "Emu Bypass",      "url": ""},
    "popup":     {"display_name": "Popup Bypass",    "url": ""},
    "valorant":  {"display_name": "Valorant",        "url": ""},
    "csgo":      {"display_name": "CS:GO",           "url": ""},
    "private":   {"display_name": "Private",         "url": ""},
}

def _gist_url() -> str:
    return f"https://api.github.com/gists/{GIST_ID}"


def _load() -> dict:
    """Fetch the current DB from the Gist. Returns empty schema on any error."""
    if not GIST_ID or not GITHUB_TOKEN:
        print("[DB] _load missing GIST_ID or GITHUB_TOKEN", flush=True)
        return _empty_schema()
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
                return _empty_schema()
        else:
            print(f"[DB] _load using configured gist file: {filename}", flush=True)
        content = files[filename].get("content", "{}")
        data = json.loads(content)
        # Back-fill missing top-level keys
        if "keys" not in data:
            data["keys"] = {}
        if "products" not in data:
            data["products"] = DEFAULT_PRODUCTS.copy()
        return data
    except Exception as e:
        print(f"[DB] _load error: {e}", flush=True)
        return _empty_schema()


def _empty_schema() -> dict:
    return {
        "keys": {},
        "products": DEFAULT_PRODUCTS.copy(),
    }


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
    print(f"[DB] create_key loaded {len(data.get('keys', {}))} existing keys", flush=True)
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
        "products":           [],
        "product_links":      {},
    }
    data["keys"][key] = record
    _save(data)
    print(f"[DB] create_key saved, now {len(data.get('keys', {}))} keys total", flush=True)
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
    """Claim a key for the first time. Returns (success, error_message)."""
    data = _load()
    if key not in data["keys"]:
        return False, "Key not found."
    rec = data["keys"][key]
    if rec["disabled"]:
        return False, "This key has been disabled."
    if rec["claimed_by_discord"] is not None:
        return False, "This key has already been claimed."
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
    On success message is JSON:
      {
        "products": ["private", ...],
        "links":    {"private": "https://...", ...},
        "display_names": {"private": "Private", ...}
      }
    Per-product links fall back to the global product catalogue URL if not
    set on the key directly.
    """
    data = _load()
    catalogue = data.get("products", {})

    for key, rec in data["keys"].items():
        if rec.get("username") and rec["username"].lower() == username.lower():
            if rec["disabled"]:
                return False, "Your key has been disabled. Contact support."
            if not verify_password(password, rec["password_hash"]):
                return False, "Invalid password."
            if rec["hwid"] is None:
                rec["hwid"] = hwid
                _save(data)
            elif rec["hwid"] != hwid:
                return False, "HWID mismatch. Contact support to reset."

            products = rec.get("products", [])
            product_links: dict[str, str] = {}
            display_names: dict[str, str] = {}
            for p in products:
                per_key_url = rec.get("product_links", {}).get(p, "")
                global_url  = catalogue.get(p, {}).get("url", "")
                product_links[p] = per_key_url or global_url
                display_names[p] = catalogue.get(p, {}).get("display_name", p)

            response = json.dumps({
                "products":      products,
                "links":         product_links,
                "display_names": display_names,
            })
            return True, response

    return False, "Username not found."


# ─── Per-key product management ───────────────────────────────────────────────

def assign_products_to_key(key: str, products: list[str]) -> bool:
    """Assign a list of products to a key. Overwrites existing products."""
    data = _load()
    if key not in data["keys"]:
        return False
    data["keys"][key]["products"] = products
    _save(data)
    return True


def set_product_link_on_key(key: str, product: str, download_url: str) -> bool:
    """Set a per-key download link override for a specific product."""
    data = _load()
    if key not in data["keys"]:
        return False
    if "product_links" not in data["keys"][key]:
        data["keys"][key]["product_links"] = {}
    data["keys"][key]["product_links"][product] = download_url
    _save(data)
    return True


# Keep old name for compatibility
def set_product_link(key: str, product: str, download_url: str) -> bool:
    return set_product_link_on_key(key, product, download_url)


def get_products_for_key(key: str) -> dict:
    """Returns {products: [...], links: {...}} for a key."""
    data = _load()
    if key not in data["keys"]:
        return {"products": [], "links": {}}
    rec = data["keys"][key]
    return {
        "products": rec.get("products", []),
        "links":    rec.get("product_links", {}),
    }


def remove_product_from_key(key: str, product: str) -> bool:
    data = _load()
    if key not in data["keys"]:
        return False
    products = data["keys"][key].get("products", [])
    if product not in products:
        return False
    products.remove(product)
    data["keys"][key]["products"] = products
    links = data["keys"][key].get("product_links", {})
    if product in links:
        del links[product]
        data["keys"][key]["product_links"] = links
    _save(data)
    return True


# ─── Global product catalogue ─────────────────────────────────────────────────

def get_all_products() -> dict[str, dict]:
    """
    Returns the global product catalogue.
    Schema: { slug: { display_name: str, url: str }, ... }
    """
    return _load().get("products", DEFAULT_PRODUCTS.copy())


def upsert_product(slug: str, display_name: str, url: str) -> None:
    """Add a new product or update an existing one in the catalogue."""
    data = _load()
    if "products" not in data:
        data["products"] = DEFAULT_PRODUCTS.copy()
    data["products"][slug] = {"display_name": display_name, "url": url}
    _save(data)


def delete_product(slug: str) -> bool:
    """Remove a product from the global catalogue."""
    data = _load()
    if slug not in data.get("products", {}):
        return False
    del data["products"][slug]
    _save(data)
    return True


def set_product_global_url(slug: str, url: str) -> bool:
    """Update only the global URL for an existing product."""
    data = _load()
    if slug not in data.get("products", {}):
        return False
    data["products"][slug]["url"] = url
    _save(data)
    return True


# ─── Legacy single download_url (kept for /download command) ─────────────────

async def get_download_url() -> str:
    data = _load()
    # Prefer the "imperium" product global URL, fall back to legacy field
    imperium_url = data.get("products", {}).get("imperium", {}).get("url", "")
    return imperium_url or data.get("download_url", "")


async def set_download_url(url: str):
    data = _load()
    data["download_url"] = url
    # Also sync to the imperium product catalogue entry
    if "products" not in data:
        data["products"] = DEFAULT_PRODUCTS.copy()
    if "imperium" not in data["products"]:
        data["products"]["imperium"] = {"display_name": "Imperium", "url": ""}
    data["products"]["imperium"]["url"] = url
    _save(data)


# ─── GitHub Release asset hosting (for /sethookloaderdll) ────────────────────
#
# Flow:
#   1. Look for a Release tagged "hookloader-dll" in GITHUB_REPO.
#   2. If it exists, delete the old "private.dll" asset (if any) and upload
#      the new bytes as "private.dll".
#   3. If no release exists, create it first.
#   4. The resulting browser_download_url is stored as the global URL for the
#      "private" product slug, so the hookloader auto-downloads it.
#
# Requires GITHUB_TOKEN to have  repo  scope (releases + assets).

_RELEASE_TAG  = "hookloader-dll"
_RELEASE_NAME = "Hookloader DLL (managed by bot)"
_DLL_ASSET    = "private.dll"


def _gh_api(path: str, method: str = "GET",
            data: bytes | None = None,
            content_type: str = "application/json") -> dict | None:
    """Simple wrapper for GitHub REST API calls. Returns parsed JSON or None."""
    url = f"https://api.github.com/repos/{GITHUB_REPO}{path}"
    headers = dict(_HEADERS)
    headers["Content-Type"] = content_type
    req = urllib.request.Request(url, data=data, headers=headers, method=method)
    try:
        with urllib.request.urlopen(req, timeout=20) as resp:
            body = resp.read()
            return json.loads(body) if body else {}
    except urllib.error.HTTPError as e:
        body = e.read().decode("utf-8", errors="replace")
        print(f"[GH] {method} {path} → HTTP {e.code}: {body[:300]}", flush=True)
        return None
    except Exception as ex:
        print(f"[GH] {method} {path} error: {ex}", flush=True)
        return None


def _get_or_create_release() -> dict | None:
    """Return the hookloader-dll Release object, creating it if needed."""
    # Try to fetch by tag
    rel = _gh_api(f"/releases/tags/{_RELEASE_TAG}")
    if rel and "id" in rel:
        return rel

    # Create it
    payload = json.dumps({
        "tag_name":   _RELEASE_TAG,
        "name":       _RELEASE_NAME,
        "body":       "Auto-managed by Imperium Bot. Do not edit manually.",
        "draft":      False,
        "prerelease": False,
    }).encode()
    rel = _gh_api("/releases", method="POST", data=payload)
    if rel and "id" in rel:
        print(f"[GH] Created release id={rel['id']}", flush=True)
        return rel
    return None


def upload_hookloader_dll(dll_bytes: bytes) -> tuple[bool, str]:
    """
    Upload dll_bytes as private.dll to the hookloader-dll GitHub Release.
    Returns (success, download_url_or_error_message).
    On success also updates the 'private' product global URL in the Gist.
    """
    if not GITHUB_TOKEN or not GITHUB_REPO:
        return False, "GITHUB_TOKEN or GITHUB_REPO not configured."

    rel = _get_or_create_release()
    if not rel:
        return False, "Failed to get or create the GitHub Release."

    release_id = rel["id"]

    # Delete existing asset with the same name if present
    assets = _gh_api(f"/releases/{release_id}/assets") or []
    for asset in assets:
        if isinstance(asset, dict) and asset.get("name") == _DLL_ASSET:
            _gh_api(f"/assets/{asset['id']}", method="DELETE")
            print(f"[GH] Deleted old asset id={asset['id']}", flush=True)
            break

    # Upload new asset via upload_url
    # upload_url template looks like: https://uploads.github.com/repos/.../assets{?name,label}
    upload_url = rel.get("upload_url", "").split("{")[0]
    if not upload_url:
        return False, "Release has no upload_url."

    upload_full = f"{upload_url}?name={_DLL_ASSET}"
    headers = {
        "Authorization": f"token {GITHUB_TOKEN}",
        "Content-Type":  "application/octet-stream",
        "Accept":        "application/vnd.github+json",
        "User-Agent":    "ImperiumBot/1.0",
    }
    req = urllib.request.Request(
        upload_full, data=dll_bytes, headers=headers, method="POST"
    )
    try:
        with urllib.request.urlopen(req, timeout=60) as resp:
            asset_info = json.loads(resp.read())
    except urllib.error.HTTPError as e:
        err = e.read().decode("utf-8", errors="replace")
        return False, f"Upload failed (HTTP {e.code}): {err[:200]}"
    except Exception as ex:
        return False, f"Upload error: {ex}"

    download_url = asset_info.get("browser_download_url", "")
    if not download_url:
        return False, "Upload succeeded but no download URL returned."

    # Persist the new URL as the global "private" product URL
    data = _load()
    if "products" not in data:
        data["products"] = DEFAULT_PRODUCTS.copy()
    if "private" not in data["products"]:
        data["products"]["private"] = {"display_name": "Private", "url": ""}
    data["products"]["private"]["url"] = download_url
    _save(data)

    print(f"[GH] DLL uploaded: {download_url}", flush=True)
    return True, download_url
