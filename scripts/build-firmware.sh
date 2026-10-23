#!/bin/sh
set -eu
# Momentum GatherSources uses a Python set; fix its iteration/link order.
export PYTHONHASHSEED=0
cd "$(dirname "$0")/.."
python3 scripts/verify-vendor.py
python3 scripts/prepare-build.py
# Use a project-local SDK when available. Never replace another project's SDK.
if [ -d .ufbt/current ]; then export UFBT_HOME="$PWD/.ufbt"; fi
python3 - <<'PY'
import os,json
from pathlib import Path
home=Path(os.environ.get('UFBT_HOME',str(Path.home()/'.ufbt')))
meta=json.loads((home/'current/components.json').read_text())['meta']
if meta['version']!='mntm-012' or meta['hw_target']!='f7':raise SystemExit('Requires pinned Momentum mntm-012 SDK; run scripts/setup-sdk.sh')
PY
.venv/bin/ufbt
python3 - <<'PY'
from pathlib import Path
import hashlib,json,os,subprocess
home=Path(os.environ.get('UFBT_HOME',str(Path.home()/'.ufbt')))
p=Path('dist/flipper_evm_wallet.fap')
# Debuglink contains the debug ELF CRC, which varies with absolute host paths.
# Runtime sections and API imports remain unchanged. Keep the separate debug ELF.
subprocess.run([str(home/'toolchain/current/bin/arm-none-eabi-objcopy'),'--remove-section=.gnu_debuglink',str(p)],check=True)
Path('dist/firmware-build.json').write_text(json.dumps({'sdk':'mntm-012','api':'87.1','sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'release_status':'development','normalization':'removed host-specific GNU debuglink CRC'},indent=2)+'\n')
PY
