import { build } from 'vite';
import { mkdir, copyFile, rm } from 'node:fs/promises';
import path from 'node:path';
const out = path.resolve('dist/extension');
await rm(out, { recursive: true, force: true });
await mkdir(out, { recursive: true });
for (const name of ['background', 'provider', 'content']) {
  await build({
    configFile: false,
    build: {
      outDir: out,
      emptyOutDir: false,
      lib: {
        entry: `extension/src/${name}.ts`,
        formats: [name === 'background' ? 'es' : 'iife'],
        name: `FEW_${name}`,
        fileName: () => `${name}.js`,
      },
      minify: false,
      sourcemap: true,
    },
  });
}
await build({
  configFile: false,
  root: 'extension',
  base: './',
  build: { outDir: out, emptyOutDir: false, minify: false, sourcemap: true },
});
await copyFile('extension/manifest.json', path.join(out, 'manifest.json'));
