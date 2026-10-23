import { chromium } from '@playwright/test';
import assert from 'node:assert/strict';
import { resolve } from 'node:path';
import { createServer } from 'node:http';
import { mkdtemp, rm, mkdir, writeFile } from 'node:fs/promises';
import { tmpdir } from 'node:os';
const profile = await mkdtemp(tmpdir() + '/few-browser-');
const server = createServer((_req, res) => {
  res.setHeader('Content-Type', 'text/html');
  res.end('<!doctype html><title>Wallet isolation test</title><h1>Local wallet test</h1>');
});
await new Promise((resolve) => server.listen(0, '127.0.0.1', resolve));
const port = server.address().port;
let ctx;
try {
  const extension = resolve('dist/extension');
  ctx = await chromium.launchPersistentContext(profile, {
    channel: 'chromium',
    headless: true,
    args: [`--disable-extensions-except=${extension}`, `--load-extension=${extension}`],
    viewport: { width: 1200, height: 900 },
  });
  let worker = ctx.serviceWorkers()[0];
  if (!worker) worker = await ctx.waitForEvent('serviceworker');
  const id = worker.url().split('/')[2];
  const errors = [];
  const ui = await ctx.newPage();
  ui.on('pageerror', (e) => errors.push(e.message));
  await ui.goto(`chrome-extension://${id}/index.html`);
  await ui.getByRole('heading', { name: 'Твоят ключ. Твоето решение.' }).waitFor();
  const uiState = await ui.evaluate(() =>
    chrome.runtime.sendMessage({ kind: 'ui', action: 'state' }),
  );
  assert.equal(uiState.result.connected, false);
  assert.equal(uiState.result.state.chain, 11155111);
  await ui.screenshot({ path: 'artifacts/wallet-ui-bg.png', fullPage: true });
  await ui.getByRole('button', { name: 'EN', exact: true }).click();
  await ui.getByRole('heading', { name: 'Your key. Your decision.' }).waitFor();
  await ui.getByRole('button', { name: 'Send', exact: false }).first().click();
  await ui.getByRole('button', { name: 'Review on Flipper' }).waitFor();
  assert.equal(await ui.getByRole('button', { name: 'Review on Flipper' }).isDisabled(), true);
  const site = await ctx.newPage();
  await site.goto(`http://127.0.0.1:${port}/`);
  await site.waitForFunction(() => !!window.ethereum);
  assert.deepEqual(
    await site.evaluate(() => window.ethereum.request({ method: 'eth_accounts' })),
    [],
  );
  assert.equal(
    await site.evaluate(() => window.ethereum.request({ method: 'eth_chainId' })),
    '0xaa36a7',
  );
  const discovery = await site.evaluate(
    () =>
      new Promise((resolve) => {
        window.addEventListener(
          'eip6963:announceProvider',
          (e) => resolve({ name: e.detail.info.name, frozen: Object.isFrozen(e.detail.provider) }),
          { once: true },
        );
        window.dispatchEvent(new Event('eip6963:requestProvider'));
      }),
  );
  assert.deepEqual(discovery, { name: 'Flipper EVM Wallet', frozen: true });
  const code = await site.evaluate(async () => {
    try {
      await window.ethereum.request({ method: 'eth_sign', params: ['0x0', '0x0'] });
      return 0;
    } catch (e) {
      return e.code;
    }
  });
  assert.equal(code, 4100);
  // A page can forge messages but cannot impersonate an extension UI sender.
  const denial = await site.evaluate(
    () =>
      new Promise((resolve) => {
        const id = 'forged-ui';
        const topic = 'flipper-evm-wallet-v1';
        window.addEventListener('message', (e) => {
          if (e.data?.id === id && e.data.direction === 'response') resolve(e.data);
        });
        window.postMessage(
          {
            topic,
            direction: 'request',
            id,
            method: 'state',
            params: [],
            kind: 'ui',
            action: 'state',
          },
          location.origin,
        );
      }),
  );
  assert.ok(denial.error);
  assert.equal(denial.result, undefined);
  // No HID mock is used. User rejects the real extension's origin permission UI.
  const request = site.evaluate(async () => {
    try {
      return { result: await window.ethereum.request({ method: 'eth_requestAccounts' }) };
    } catch (e) {
      return { error: e.code };
    }
  });
  let approval = await ctx.waitForEvent('page');
  await approval.getByRole('button', { name: 'Reject', exact: true }).waitFor();
  await approval.getByRole('button', { name: 'Reject', exact: true }).click();
  assert.deepEqual(await request, { error: 4001 });
  assert.deepEqual(
    await site.evaluate(() => window.ethereum.request({ method: 'eth_accounts' })),
    [],
  );
  await approval.close();
  // Navigation must cancel requests, never transfer permission to a new document.
  const navigating = site.evaluate(() => {
    window.ethereum.request({ method: 'eth_requestAccounts' }).catch(() => {});
  });
  await navigating;
  approval = await ctx.waitForEvent('page');
  await approval.getByRole('button', { name: 'Reject', exact: true }).waitFor();
  await site.goto(`http://127.0.0.1:${port}/next`);
  await approval.waitForFunction(() => !document.querySelector('.approval'));
  const other = await ctx.newPage();
  await other.goto(`http://localhost:${port}/`);
  await other.waitForFunction(() => !!window.ethereum);
  assert.deepEqual(
    await other.evaluate(() => window.ethereum.request({ method: 'eth_accounts' })),
    [],
  );
  // Other wallets are not overwritten by our main-world provider.
  const coexist = await ctx.newPage();
  await coexist.addInitScript(() => {
    window.ethereum = { otherWallet: true };
  });
  await coexist.goto(`http://127.0.0.1:${port}/coexist`);
  assert.equal(await coexist.evaluate(() => window.ethereum.otherWallet), true);
  assert.deepEqual(errors, []);
  const result = {
    browser: ctx.browser()?.version(),
    profile: 'isolated temporary profile',
    checks: [
      'extension UI bg/en',
      'no address before permission',
      'EIP6963',
      'UI impersonation denied',
      'permission rejection',
      'navigation cancels',
      'second-origin isolation',
      'other wallet coexistence',
    ],
    hardwareHID: false,
  };
  await writeFile('artifacts/browser-results.json', JSON.stringify(result, null, 2) + '\n');
  console.log(JSON.stringify(result));
} finally {
  if (ctx) await ctx.close();
  await new Promise((r) => server.close(r));
  await rm(profile, { recursive: true, force: true });
}
