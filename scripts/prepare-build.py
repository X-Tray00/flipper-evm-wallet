from pathlib import Path
root=Path(__file__).resolve().parents[1]
target=root/'lib/crypto'
target.mkdir(parents=True,exist_ok=True)
for p in (root/'vendor/trezor-crypto').rglob('*.c'):
    link=target/(p.parent.name+'_'+p.name)
    if not link.exists():link.symlink_to(Path('../../')/p.relative_to(root))
