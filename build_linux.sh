#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
cd "$ROOT"
node tools/fetch_official.mjs resources/official_snapshot.json
python3 tools/build_db.py resources/official_snapshot.json resources/reference.db
cmake -S . -B build-linux -G Ninja -DCMAKE_BUILD_TYPE=Release -DAL_BUILD_TESTS=ON
cmake --build build-linux --parallel
./build-linux/al-core-test resources/reference.db
mkdir -p dist/linux/resources
cp build-linux/adventure-land-reference-os dist/linux/
cp resources/reference.db dist/linux/resources/
echo "Built: $ROOT/dist/linux/adventure-land-reference-os"
