import { readFile, writeFile, mkdir, copyFile, readdir, stat } from 'node:fs/promises';
import { execFileSync } from 'node:child_process';
import { createHash } from 'node:crypto';
import path from 'node:path';
const root = process.cwd(),
  release = path.join(root, 'dist/packages');
await mkdir(release, { recursive: true });
const fap = await readFile('dist/flipper_evm_wallet.fap');
const receipt = JSON.parse(await readFile('dist/firmware-build.json', 'utf8'));
if (createHash('sha256').update(fap).digest('hex') !== receipt.sha256)
  throw new Error('FAP differs from the verified build receipt');
const licenses = [];
for (const [key, pkg] of Object.entries(
  JSON.parse(await readFile('package-lock.json', 'utf8')).packages,
)) {
  if (!key || pkg.dev) continue;
  const dir = path.join(root, key);
  let files = [];
  try {
    files = await readdir(dir);
  } catch {
    continue;
  }
  for (const file of files.filter((n) => /^(license|licence|copying)(\.|$)/i.test(n))) {
    const p = path.join(dir, file);
    if ((await stat(p)).isFile())
      licenses.push(
        `\n===== ${key}@${pkg.version} / ${file} =====\n` + (await readFile(p, 'utf8')),
      );
  }
}
await writeFile('dist/extension/THIRD_PARTY_LICENSES.txt', licenses.join('\n'));
await copyFile('LICENSE', 'dist/extension/LICENSE');
const zip = path.join(release, 'flipper-evm-wallet-extension-dev.zip');
execFileSync('python3', [
  '-c',
  `import pathlib,zipfile
root=pathlib.Path('dist/extension')
with zipfile.ZipFile(${JSON.stringify(zip)},'w',zipfile.ZIP_DEFLATED) as z:
 for p in sorted(root.rglob('*')):
  if p.is_file():
   info=zipfile.ZipInfo(str(p.relative_to(root)),(2026,10,23,0,0,0));info.compress_type=zipfile.ZIP_DEFLATED;info.external_attr=0o644<<16;z.writestr(info,p.read_bytes())
`,
]);
await copyFile(
  'dist/flipper_evm_wallet.fap',
  path.join(release, 'flipper-evm-wallet-mntm012-dev.fap'),
);
const notices =
  (await readFile('vendor/trezor-crypto/LICENSE', 'utf8')) +
  '\n' +
  (await readFile('vendor/trezor-crypto/chacha20poly1305/LICENSE', 'utf8'));
await writeFile(path.join(release, 'FIRMWARE_THIRD_PARTY_LICENSES.txt'), notices);
for (const file of ['README.md', 'LICENSE', 'SECURITY.md'])
  await copyFile(file, path.join(release, file));
await mkdir(path.join(release, 'docs'), { recursive: true });
for (const file of (await readdir('docs')).filter((n) => n.endsWith('.md') || n.endsWith('.json')))
  await copyFile(path.join('docs', file), path.join(release, 'docs', file));
let sums = '';
for (const name of (await readdir(release))
  .filter((n) => n.endsWith('.zip') || n.endsWith('.fap'))
  .sort())
  sums +=
    createHash('sha256')
      .update(await readFile(path.join(release, name)))
      .digest('hex') +
    '  ' +
    name +
    '\n';
await writeFile(path.join(release, 'SHA256SUMS'), sums);
console.log(sums);
