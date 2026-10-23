from pathlib import Path
import json,hashlib,sys
root=Path(__file__).resolve().parents[1]
manifest=json.loads((root/'vendor/trezor-crypto.sha256.json').read_text())
actual={str(p.relative_to(root/'vendor/trezor-crypto')):hashlib.sha256(p.read_bytes()).hexdigest() for p in (root/'vendor/trezor-crypto').rglob('*') if p.is_file()}
if actual!=manifest:sys.exit('Vendored Trezor sources differ from their pinned manifest')
print(f'Verified {len(actual)} vendored source/license files')
