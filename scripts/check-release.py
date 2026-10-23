"""Evidence presence gate. Does not substitute for reviewing the evidence."""
from pathlib import Path
import json,sys
root=Path(__file__).resolve().parents[1]
x=json.loads((root/'docs/acceptance.json').read_text())
failed=[]
for name,g in x['gates'].items():
 if not g['passed'] or not g['evidence'] or any(not (root/p).is_file() for p in g['evidence']):failed.append(name)
if failed:
 print('PUBLIC BETA BLOCKED. Incomplete evidence gates:')
 for name in failed:print('- '+name)
 sys.exit(1)
print('Evidence files present. Manual release review still required.')
