#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
python3 "$ROOT/tools/fetch_seed.py" "$ROOT/resources/seed.json"
python3 "$ROOT/tools/build_db.py" "$ROOT/resources/seed.json" "$ROOT/resources/reference.db"
cmake -S "$ROOT" -B "$ROOT/build" -DCMAKE_BUILD_TYPE=Release -DAL_BUILD_TESTS=ON
cmake --build "$ROOT/build" -j"$(nproc)"
"$ROOT/build/al-core-test" "$ROOT/resources/reference.db"
mkdir -p "$ROOT/dist/linux/resources"
cp "$ROOT/build/adventure-land-reference-os" "$ROOT/dist/linux/"
cp "$ROOT/resources/reference.db" "$ROOT/dist/linux/resources/"
strip "$ROOT/dist/linux/adventure-land-reference-os" 2>/dev/null || true
echo "Built: $ROOT/dist/linux/adventure-land-reference-os"
