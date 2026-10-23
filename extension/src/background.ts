import {
  bytesToHex,
  hexToBytes,
  serializeTransaction,
  recoverTransactionAddress,
  recoverAddress,
  hashMessage,
  hashTypedData,
  getAddress,
  keccak256,
  toHex,
  encodeFunctionData,
  type Hex,
  type Address,
} from 'viem';
import { WalletHid, envelope } from './hid';
import {
  CHAINS,
  chainId,
  address,
  quantity,
  hex,
  webOrigin,
  accountIndex,
  object,
  exactKeys,
  sameAddress,
  fail,
  WalletError,
  type ChainId,
} from './policy';
import { rpc } from './rpc';

type Grant = { address: Address; account: number };
type Asset = {
  chain: ChainId;
  address: Address;
  type: 'ERC20' | 'ERC721' | 'ERC1155';
  id?: string;
};
type History = {
  hash: Hex;
  chain: ChainId;
  origin: string;
  at: string;
  from: Address;
  to: Address;
  value: string;
};
type State = {
  chain: ChainId;
  account: number;
  locale: 'bg' | 'en';
  grants: Record<string, Grant>;
  addresses: Record<string, Address>;
  assets: Asset[];
  history: History[];
};
const defaults: State = {
  chain: 11155111,
  account: 0,
  locale: 'bg',
  grants: {},
  addresses: {},
  assets: [],
  history: [],
};
let state: State = structuredClone(defaults);
const hid = new WalletHid();
const ready = (async () => {
  await chrome.storage.local.setAccessLevel({ accessLevel: 'TRUSTED_CONTEXTS' });
  await chrome.storage.session.setAccessLevel({ accessLevel: 'TRUSTED_CONTEXTS' });
  const saved = await chrome.storage.local.get('wallet');
  if (saved.wallet) {
    state = { ...defaults, ...saved.wallet };
    state.chain = chainId(state.chain);
    state.account = accountIndex(state.account);
  }
})();
async function save() {
  await chrome.storage.local.set({ wallet: state });
}
function err(e: unknown) {
  return {
    code: e instanceof WalletError ? e.code : -32603,
    message: e instanceof Error ? e.message : 'Wallet error',
  };
}
const ownURL = () => chrome.runtime.getURL('index.html');
function assertUI(sender: chrome.runtime.MessageSender) {
  if (sender.id !== chrome.runtime.id || sender.url?.split(/[?#]/)[0] !== ownURL())
    fail(4100, 'Extension page required');
}
function site(sender: chrome.runtime.MessageSender) {
  if (
    sender.id !== chrome.runtime.id ||
    sender.frameId !== 0 ||
    sender.tab?.id === undefined ||
    !sender.documentId
  )
    fail(4100, 'Top-level browser document required');
  return { origin: webOrigin(sender.url), tabId: sender.tab.id, documentId: sender.documentId };
}
async function alive(s: ReturnType<typeof site>) {
  try {
    const result = await chrome.tabs.sendMessage(
      s.tabId,
      { kind: 'ping' },
      { documentId: s.documentId },
    );
    if (!result?.alive) throw new Error();
  } catch {
    fail(4001, 'The requesting page changed or closed');
  }
}
async function event(origin: string, event: string, value: unknown) {
  for (const tab of await chrome.tabs.query({})) {
    if (tab.id === undefined) continue;
    void chrome.tabs
      .sendMessage(tab.id, { kind: 'originEvent', origin, event, value })
      .catch(() => {});
  }
}
// Only the content bridge whose current origin matches delivers origin events.
async function revoke(origin: string) {
  delete state.grants[origin];
  await save();
  await event(origin, 'accountsChanged', []);
  if (pending?.origin === origin) await rejectPending('Site permission revoked');
}

type Pending = {
  id: string;
  origin: string;
  title: string;
  details: string;
  phase: 'review' | 'device';
  site?: ReturnType<typeof site>;
  run: () => Promise<unknown>;
  resolve: (v: unknown) => void;
  reject: (e: unknown) => void;
  timer: ReturnType<typeof setTimeout>;
};
let pending: Pending | undefined;
async function rejectPending(message = 'Request rejected') {
  const p = pending;
  if (!p) return;
  pending = undefined;
  clearTimeout(p.timer);
  p.reject(new WalletError(4001, message));
  await hid.cancel();
}
async function ask(
  origin: string,
  title: string,
  details: string,
  run: () => Promise<unknown>,
  requestSite?: ReturnType<typeof site>,
) {
  if (pending) fail(-32002, 'Another wallet request is active');
  const result = new Promise((resolve, reject) => {
    const id = crypto.randomUUID();
    const timer = setTimeout(() => {
      if (pending?.id === id) void rejectPending('Review expired');
    }, 230000);
    pending = {
      id,
      origin,
      title,
      details,
      phase: 'review',
      site: requestSite,
      run,
      resolve,
      reject,
      timer,
    };
  });
  void chrome.windows
    .create({
      url: ownURL() + '?approval=1',
      type: 'popup',
      width: 510,
      height: 720,
    })
    .catch(() => rejectPending('Unable to open the approval window'));
  return result;
}
async function approve(id: string) {
  const p = pending;
  if (!p || p.id !== id || p.phase !== 'review') fail(4001, 'Request is no longer active');
  p.phase = 'device';
  void (async () => {
    try {
      if (p.site) await alive(p.site);
      const result = await p.run();
      if (pending !== p) fail(4001, 'Request cancelled');
      if (p.site) await alive(p.site);
      p.resolve(result);
    } catch (e) {
      p.reject(e);
    } finally {
      if (pending === p) pending = undefined;
      clearTimeout(p.timer);
    }
  })();
  return true;
}
function current(p: Pending | undefined) {
  if (!p || pending !== p) fail(4001, 'Request cancelled');
}
async function deviceAddress(index: number, origin: string) {
  await hid.connect();
  const data = await hid.request(2, envelope(index, origin));
  if (data.length !== 20) fail(-32603, 'Invalid address response');
  return getAddress(bytesToHex(data));
}
function grant(origin: string) {
  const g = state.grants[origin];
  if (!g) fail(4100, 'Connect this site before requesting wallet access');
  return g;
}
function signature(data: Uint8Array) {
  if (data.length !== 65 || data[64] > 1) fail(-32603, 'Invalid device signature');
  return {
    r: bytesToHex(data.slice(0, 32)),
    s: bytesToHex(data.slice(32, 64)),
    yParity: data[64] as 0 | 1,
  };
}
function sigHex(data: Uint8Array) {
  signature(data);
  const out = data.slice();
  out[64] += 27;
  return bytesToHex(out);
}
async function sendTransaction(origin: string, raw: unknown, g: Grant, chain: ChainId) {
  const p = pending;
  if (chain === 84532)
    fail(4200, 'Base transaction signing is blocked until the separate L1 fee policy is resolved.');
  const tx = object(raw);
  exactKeys(tx, [
    'from',
    'to',
    'value',
    'data',
    'gas',
    'nonce',
    'maxFeePerGas',
    'maxPriorityFeePerGas',
    'chainId',
    'type',
    'accessList',
  ]);
  if (!sameAddress(address(tx.from), g.address))
    fail(4100, 'Transaction sender does not match selected account');
  if (tx.chainId !== undefined && chainId(tx.chainId) !== chain)
    fail(4901, 'Transaction network differs from selected network');
  if (tx.type !== undefined && tx.type !== '0x2' && tx.type !== 2)
    fail(4200, 'Only EIP-1559 transactions are supported');
  if (tx.accessList !== undefined && (!Array.isArray(tx.accessList) || tx.accessList.length))
    fail(4200, 'Access lists are unsupported');
  const to = address(tx.to),
    value = tx.value === undefined ? 0n : quantity(tx.value),
    data = tx.data === undefined ? '0x' : hex(tx.data);
  const nonce = quantity(
    tx.nonce ?? (await rpc(chain, 'eth_getTransactionCount', [g.address, 'pending'])),
    BigInt(Number.MAX_SAFE_INTEGER),
  );
  let priority =
    tx.maxPriorityFeePerGas === undefined
      ? quantity(await rpc(chain, 'eth_maxPriorityFeePerGas'), (1n << 64n) - 1n)
      : quantity(tx.maxPriorityFeePerGas, (1n << 64n) - 1n);
  let fee: bigint;
  if (tx.maxFeePerGas === undefined) {
    const block = await rpc(chain, 'eth_getBlockByNumber', ['latest', false]);
    fee = 2n * quantity(block.baseFeePerGas) + priority;
  } else fee = quantity(tx.maxFeePerGas, (1n << 64n) - 1n);
  if (fee < priority || fee >= 1n << 64n) fail(-32602, 'Unsupported gas fee');
  const gas = quantity(
    tx.gas ??
      (await rpc(chain, 'eth_estimateGas', [{ from: g.address, to, value: toHex(value), data }])),
    (1n << 32n) - 1n,
  );
  const prepared = {
    type: 'eip1559' as const,
    chainId: chain,
    nonce: Number(nonce),
    maxPriorityFeePerGas: priority,
    maxFeePerGas: fee,
    gas,
    to,
    value,
    data,
    accessList: [],
  };
  const unsigned = serializeTransaction(prepared);
  current(p);
  if (p?.site) await alive(p.site);
  await hid.connect();
  const signed = await hid.request(3, envelope(g.account, origin, hexToBytes(unsigned)));
  current(p);
  const serialized = serializeTransaction(prepared, signature(signed));
  const signer = await recoverTransactionAddress({ serializedTransaction: serialized });
  if (!sameAddress(signer, g.address))
    fail(-32603, 'Device signature does not match selected account');
  if (p?.site) {
    await alive(p.site);
    if (state.grants[origin] !== g) fail(4100, 'Site permission changed');
  }
  if (state.chain !== chain) fail(4901, 'Selected network changed');
  current(p);
  const expectedHash = keccak256(serialized);
  const hash = hex(await rpc(chain, 'eth_sendRawTransaction', [serialized]), 32);
  if (hash.toLowerCase() !== expectedHash.toLowerCase())
    fail(-32603, 'RPC returned a different transaction hash');
  state.history.unshift({
    hash,
    chain,
    origin,
    at: new Date().toISOString(),
    from: g.address,
    to,
    value: value.toString(),
  });
  state.history = state.history.slice(0, 200);
  await save();
  return hash;
}
function uintWord(v: unknown, max = (1n << 256n) - 1n) {
  if (typeof v !== 'string' && typeof v !== 'number') fail(-32602, 'Invalid typed integer');
  if (typeof v === 'number' && !Number.isSafeInteger(v)) fail(-32602, 'Unsafe numeric integer');
  if (!/^(?:0x[0-9a-fA-F]+|[0-9]+)$/.test(String(v))) fail(-32602, 'Invalid typed integer');
  const n = BigInt(v);
  if (n < 0 || n > max) fail(-32602, 'Typed integer outside supported range');
  return n;
}
const permitFields = [
  { name: 'owner', type: 'address' },
  { name: 'spender', type: 'address' },
  { name: 'value', type: 'uint256' },
  { name: 'nonce', type: 'uint256' },
  { name: 'deadline', type: 'uint256' },
];
const domainFields = [
  { name: 'name', type: 'string' },
  { name: 'version', type: 'string' },
  { name: 'chainId', type: 'uint256' },
  { name: 'verifyingContract', type: 'address' },
];
async function permit(origin: string, input: unknown, g: Grant, chain: ChainId) {
  const p = pending;
  const typed = object(typeof input === 'string' ? JSON.parse(input) : input);
  exactKeys(typed, ['types', 'primaryType', 'domain', 'message']);
  const types = object(typed.types),
    d = object(typed.domain),
    m = object(typed.message);
  exactKeys(types, ['EIP712Domain', 'Permit']);
  exactKeys(d, ['name', 'version', 'chainId', 'verifyingContract']);
  exactKeys(m, ['owner', 'spender', 'value', 'nonce', 'deadline']);
  if (
    typed.primaryType !== 'Permit' ||
    JSON.stringify(types.Permit) !== JSON.stringify(permitFields) ||
    (types.EIP712Domain && JSON.stringify(types.EIP712Domain) !== JSON.stringify(domainFields))
  )
    fail(4200, 'Only canonical EIP-2612 Permit is supported');
  if (chainId(d.chainId) !== chain || !sameAddress(address(m.owner), g.address))
    fail(4100, 'Permit owner or network mismatch');
  if (typeof d.name !== 'string' || typeof d.version !== 'string')
    fail(-32602, 'Permit domain missing');
  const contract = address(d.verifyingContract),
    spender = address(m.spender),
    value = uintWord(m.value, (1n << 128n) - 1n),
    nonce = uintWord(m.nonce),
    deadline = uintWord(m.deadline, (1n << 64n) - 1n);
  const payload = new Uint8Array(136);
  new DataView(payload.buffer).setUint32(0, chain, true);
  payload.set(hexToBytes(contract), 4);
  payload.set(hexToBytes(g.address), 24);
  payload.set(hexToBytes(spender), 44);
  payload.set(hexToBytes(toHex(value, { size: 32 })), 64);
  payload.set(hexToBytes(toHex(nonce, { size: 32 })), 96);
  payload.set(hexToBytes(toHex(deadline, { size: 8 })), 128);
  await hid.connect();
  const result = await hid.request(5, envelope(g.account, origin, payload));
  current(p);
  const hash = hashTypedData({
    domain: { name: d.name, version: d.version, chainId: chain, verifyingContract: contract },
    primaryType: 'Permit',
    types: { Permit: permitFields },
    message: { owner: g.address, spender, value, nonce, deadline },
  });
  const recovered = await recoverAddress({ hash, signature: signature(result) });
  if (!sameAddress(recovered, g.address))
    fail(-32603, 'Permit signature or device contract metadata mismatch');
  current(p);
  if (state.grants[origin] !== g || state.chain !== chain)
    fail(4100, 'Permission or network changed');
  return sigHex(result);
}
async function siteRequest(m: any, sender: chrome.runtime.MessageSender) {
  const s = site(sender),
    origin = s.origin;
  const params = m.params ?? [];
  if (!Array.isArray(params)) fail(-32602, 'Positional parameters required');
  if (m.method === 'eth_chainId') return toHex(state.chain);
  if (m.method === 'net_version') return String(state.chain);
  if (m.method === 'eth_accounts')
    return state.grants[origin] ? [state.grants[origin].address] : [];
  if (m.method === 'eth_requestAccounts') {
    if (state.grants[origin]) return [state.grants[origin].address];
    const index = state.account;
    return ask(
      origin,
      'Connect site',
      `Share public account ${index + 1} with this site. Confirm the address on Flipper.`,
      async () => {
        const p = pending;
        const addr = await deviceAddress(index, origin);
        current(p);
        await alive(s);
        state.grants[origin] = { account: index, address: addr };
        state.addresses[index] = addr;
        await save();
        await event(origin, 'accountsChanged', [addr]);
        return [addr];
      },
      s,
    );
  }
  const g = grant(origin),
    chain = state.chain;
  if (m.method === 'wallet_revokePermissions') {
    await revoke(origin);
    return null;
  }
  if (m.method === 'wallet_getPermissions')
    return [
      {
        parentCapability: 'eth_accounts',
        caveats: [{ type: 'restrictReturnedAccounts', value: [g.address] }],
      },
    ];
  if (m.method === 'wallet_switchEthereumChain') {
    if (params.length !== 1) fail(-32602, 'Expected chain');
    const selected = chainId(object(params[0]).chainId);
    return ask(
      origin,
      'Change test network',
      CHAINS[selected].name,
      async () => {
        state.chain = selected;
        await save();
        for (const o of Object.keys(state.grants)) await event(o, 'chainChanged', toHex(selected));
        return null;
      },
      s,
    );
  }
  if (m.method === 'eth_sendTransaction') {
    if (params.length !== 1) fail(-32602, 'Expected one transaction');
    return ask(
      origin,
      'Review transaction',
      JSON.stringify(params[0], null, 2),
      () => sendTransaction(origin, params[0], g, chain),
      s,
    );
  }
  if (m.method === 'personal_sign') {
    if (params.length !== 2 || !sameAddress(address(params[1]), g.address))
      fail(-32602, 'Expected message and connected address');
    const message = hex(params[0], 1536);
    return ask(
      origin,
      'Sign in with Ethereum',
      new TextDecoder('utf-8', { fatal: true }).decode(hexToBytes(message)),
      async () => {
        const p = pending;
        await hid.connect();
        const result = await hid.request(4, envelope(g.account, origin, hexToBytes(message)));
        current(p);
        const recovered = await recoverAddress({
          hash: hashMessage({ raw: message }),
          signature: signature(result),
        });
        if (!sameAddress(recovered, g.address)) fail(-32603, 'SIWE signature verification failed');
        current(p);
        if (state.grants[origin] !== g || state.chain !== chain)
          fail(4100, 'Permission or network changed');
        return sigHex(result);
      },
      s,
    );
  }
  if (m.method === 'eth_signTypedData_v4') {
    if (params.length !== 2 || !sameAddress(address(params[0]), g.address))
      fail(-32602, 'Expected connected account and typed data');
    return ask(
      origin,
      'Permit token spending',
      typeof params[1] === 'string' ? params[1] : JSON.stringify(params[1], null, 2),
      () => permit(origin, params[1], g, chain),
      s,
    );
  }
  const reads = new Set([
    'eth_getBalance',
    'eth_getTransactionCount',
    'eth_blockNumber',
    'eth_getBlockByNumber',
    'eth_getTransactionReceipt',
    'eth_getTransactionByHash',
    'eth_call',
    'eth_estimateGas',
    'eth_gasPrice',
    'eth_maxPriorityFeePerGas',
    'eth_getCode',
  ]);
  if (reads.has(m.method)) return rpc(chain, m.method, params);
  fail(4200, 'Unsupported method. Raw signing, arbitrary typed data and mainnet are disabled.');
}
async function ui(m: any, sender: chrome.runtime.MessageSender) {
  assertUI(sender);
  switch (m.action) {
    case 'state':
      return {
        state,
        connected: hid.connected,
        pending: pending
          ? {
              id: pending.id,
              origin: pending.origin,
              title: pending.title,
              details: pending.details,
              phase: pending.phase,
            }
          : null,
      };
    case 'connect':
      await hid.connect();
      return true;
    case 'disconnect':
      await rejectPending();
      await hid.close();
      return true;
    case 'approve':
      return approve(String(m.id));
    case 'reject':
      if (pending?.id !== m.id) fail(4001, 'Request expired');
      await rejectPending();
      return true;
    case 'locale':
      state.locale = m.value === 'en' ? 'en' : 'bg';
      await save();
      return true;
    case 'account':
      await rejectPending('Account changed');
      state.account = accountIndex(m.value);
      for (const o of Object.keys(state.grants)) await revoke(o);
      await save();
      return true;
    case 'chain':
      await rejectPending('Network changed');
      state.chain = chainId(m.value);
      await save();
      for (const o of Object.keys(state.grants)) await event(o, 'chainChanged', toHex(state.chain));
      return true;
    case 'address': {
      if (pending) fail(-32002, 'Another request is active');
      const index = state.account;
      const addr = await deviceAddress(index, 'chrome-extension://' + chrome.runtime.id);
      state.addresses[index] = addr;
      await save();
      return addr;
    }
    case 'balance': {
      const addr = state.addresses[state.account];
      return addr ? rpc(state.chain, 'eth_getBalance', [addr, 'latest']) : null;
    }
    case 'revoke':
      await revoke(String(m.origin));
      return true;
    case 'send': {
      const addr = state.addresses[state.account];
      if (!addr) fail(4100, 'Verify account address on Flipper first');
      const chain = state.chain,
        g = { address: addr, account: state.account };
      const tx = { ...object(m.tx), from: addr };
      return ask('Wallet', 'Send test transaction', JSON.stringify(tx, null, 2), () =>
        sendTransaction('Wallet', tx, g, chain),
      );
    }
    case 'watch': {
      const v = object(m.asset);
      if (!['ERC20', 'ERC721', 'ERC1155'].includes(String(v.type)))
        fail(-32602, 'Unsupported asset type');
      const asset: Asset = {
        chain: state.chain,
        address: address(v.address),
        type: v.type as Asset['type'],
      };
      if (asset.type !== 'ERC20') asset.id = uintWord(v.id).toString();
      if (state.assets.length >= 100) fail(-32602, 'Watch list is full');
      if (!state.assets.some((a) => JSON.stringify(a) === JSON.stringify(asset)))
        state.assets.push(asset);
      await save();
      return true;
    }
    case 'unwatch':
      state.assets = state.assets.filter((_, i) => i !== m.index);
      await save();
      return true;
    case 'assetBalance': {
      const a = state.assets[Number(m.index)],
        addr = state.addresses[state.account];
      if (!a || !addr) fail(-32602, 'Missing asset or account');
      let data: Hex;
      if (a.type === 'ERC20')
        data = encodeFunctionData({
          abi: [
            {
              type: 'function',
              name: 'balanceOf',
              stateMutability: 'view',
              inputs: [{ type: 'address' }],
              outputs: [{ type: 'uint256' }],
            },
          ],
          functionName: 'balanceOf',
          args: [addr],
        });
      else if (a.type === 'ERC721')
        data = encodeFunctionData({
          abi: [
            {
              type: 'function',
              name: 'ownerOf',
              stateMutability: 'view',
              inputs: [{ type: 'uint256' }],
              outputs: [{ type: 'address' }],
            },
          ],
          functionName: 'ownerOf',
          args: [BigInt(a.id!)],
        });
      else
        data = encodeFunctionData({
          abi: [
            {
              type: 'function',
              name: 'balanceOf',
              stateMutability: 'view',
              inputs: [{ type: 'address' }, { type: 'uint256' }],
              outputs: [{ type: 'uint256' }],
            },
          ],
          functionName: 'balanceOf',
          args: [addr, BigInt(a.id!)],
        });
      return rpc(a.chain, 'eth_call', [{ to: a.address, data }, 'latest']);
    }
    case 'receipt': {
      const h = state.history.find((h) => h.hash === m.hash);
      if (!h) fail(-32602, 'Unknown transaction');
      return rpc(h.chain, 'eth_getTransactionReceipt', [h.hash]);
    }
    default:
      fail(4200, 'Unsupported wallet action');
  }
}
chrome.runtime.onMessage.addListener((message, sender, respond) => {
  void (async () => {
    try {
      await ready;
      if (!message || typeof message !== 'object') fail(-32602, 'Invalid message');
      const result =
        message.kind === 'ui'
          ? await ui(message, sender)
          : message.kind === 'rpc'
            ? await siteRequest(message, sender)
            : fail(4200, 'Unknown channel');
      respond({ result });
    } catch (e) {
      respond({ error: err(e) });
    }
  })();
  return true;
});
chrome.action.onClicked.addListener(() => void chrome.tabs.create({ url: ownURL() }));
chrome.tabs.onRemoved.addListener((id) => {
  if (pending?.site?.tabId === id) void rejectPending('Requesting tab closed');
});
chrome.tabs.onUpdated.addListener((id, change) => {
  if (pending?.site?.tabId === id && (change.status === 'loading' || change.url))
    void rejectPending('Requesting page navigated');
});
