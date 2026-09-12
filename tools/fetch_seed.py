#!/usr/bin/env python3
import hashlib
import json
import os
import pathlib
import sys
import urllib.request

DEFAULT_URL = "http://riflex91.bplaced.net/assets/adventure/reference/assets/data/seed.json"
DEFAULT_SHA256 = "594529988485782f9a8e3ee9382601fb600413b7c2dc56e2e86a8ccdbd75c981"

url = os.environ.get("AL_SEED_URL", DEFAULT_URL)
expected_sha256 = os.environ.get("AL_SEED_SHA256", DEFAULT_SHA256).strip().lower()
out = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else "resources/seed.json")
out.parent.mkdir(parents=True, exist_ok=True)

req = urllib.request.Request(url, headers={"User-Agent": "AdventureLandReferenceOS-Desktop-Build/0.1"})
try:
    with urllib.request.urlopen(req, timeout=30) as response:
        payload = response.read()
except Exception as exc:
    raise SystemExit(f"Failed to download seed from {url}: {exc}")

actual_sha256 = hashlib.sha256(payload).hexdigest()
if expected_sha256 and actual_sha256 != expected_sha256:
    raise SystemExit(
        "Seed integrity check failed: "
        f"expected sha256={expected_sha256}, got sha256={actual_sha256}. "
        "If the curated seed was intentionally updated, review it and update AL_SEED_SHA256/DEFAULT_SHA256."
    )

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
print(f"seed_url={url}")
print(f"seed_sha256={actual_sha256}")
print(f"seed_bytes={len(payload)}")
print("counts=" + ", ".join(f"{k}:{len(data[k])}" for k in required))
