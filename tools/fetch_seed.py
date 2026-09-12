#!/usr/bin/env python3
import hashlib
import json
import os
import pathlib
import ssl
import sys
import time
import urllib.request

HTTPS_URL = "https://riflex91.bplaced.net/assets/adventure/reference/assets/data/seed.json"
HTTP_URL = "http://riflex91.bplaced.net/assets/adventure/reference/assets/data/seed.json"
DEFAULT_SHA256 = "594529988485782f9a8e3ee9382601fb600413b7c2dc56e2e86a8ccdbd75c981"

configured_url = os.environ.get("AL_SEED_URL", "").strip()
expected_sha256 = os.environ.get("AL_SEED_SHA256", DEFAULT_SHA256).strip().lower()
out = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else "resources/seed.json")
out.parent.mkdir(parents=True, exist_ok=True)

if not expected_sha256:
    raise SystemExit("AL_SEED_SHA256 must not be empty: the build requires a pinned seed digest")

urls = []
for candidate in (configured_url, HTTPS_URL, HTTP_URL):
    if candidate and candidate not in urls:
        urls.append(candidate)

# bplaced currently serves a self-signed TLS certificate. We intentionally allow
# that transport here because the exact payload is independently authenticated by
# the mandatory SHA-256 pin below. A changed payload is rejected before use.
unverified_tls = ssl._create_unverified_context()
payload = None
used_url = None
errors = []

for url in urls:
    for attempt in range(1, 4):
        req = urllib.request.Request(url, headers={"User-Agent": "AdventureLandReferenceOS-Desktop-Build/0.1"})
        try:
            kwargs = {"timeout": 20}
            if url.lower().startswith("https://"):
                kwargs["context"] = unverified_tls
            with urllib.request.urlopen(req, **kwargs) as response:
                candidate = response.read()
            digest = hashlib.sha256(candidate).hexdigest()
            if digest != expected_sha256:
                raise RuntimeError(
                    f"integrity mismatch: expected {expected_sha256}, received {digest}"
                )
            payload = candidate
            used_url = url
            break
        except Exception as exc:
            errors.append(f"{url} attempt {attempt}: {exc}")
            if attempt < 3:
                time.sleep(attempt * 2)
    if payload is not None:
        break

if payload is None:
    raise SystemExit("Failed to obtain pinned seed:\n" + "\n".join(errors))

actual_sha256 = hashlib.sha256(payload).hexdigest()
try:
    data = json.loads(payload.decode("utf-8"))
except Exception as exc:
    raise SystemExit(f"Downloaded seed is not valid UTF-8 JSON: {exc}")

required = {"items": 40, "monsters": 30, "skills": 40, "classes": 7, "maps": 5}
for section, minimum in required.items():
    value = data.get(section)
    count = len(value) if isinstance(value, (list, dict)) else 0
    if count < minimum:
        raise SystemExit(f"Seed validation failed: {section} has {count}, expected at least {minimum}")

out.write_bytes(payload)
print(f"seed_url={used_url}")
print(f"seed_sha256={actual_sha256}")
print(f"seed_bytes={len(payload)}")
print("counts=" + ", ".join(f"{k}:{len(data[k])}" for k in required))
