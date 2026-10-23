#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
mkdir -p build
sdk_zip=build/flipper-z-f7-sdk-mntm-012.zip
if [ ! -f "$sdk_zip" ]; then
 curl -fL --retry 2 'https://github.com/Next-Flip/Momentum-Firmware/releases/download/mntm-012/flipper-z-f7-sdk-mntm-012.zip' -o "$sdk_zip"
fi
python3 - <<'PY'
import hashlib
from pathlib import Path
expected='c90530c3e0aa7bc4356b8521df20d23d7b4a5435ab58a2530ffb696200408cc2'
if hashlib.sha256(Path('build/flipper-z-f7-sdk-mntm-012.zip').read_bytes()).hexdigest()!=expected:raise SystemExit('SDK checksum mismatch')
PY
UFBT_HOME="$PWD/.ufbt" .venv/bin/ufbt update --local "$sdk_zip"
