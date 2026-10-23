import {
  parseEther,
  toHex,
  encodeFunctionData,
  recoverMessageAddress,
  recoverTypedDataAddress,
} from 'viem';
import { createSiweMessage } from 'viem/siwe';
const el = (id: string) => document.getElementById(id) as HTMLInputElement;
const wallets: any[] = [];
let provider: any,
  account = `0x` as `0x${string}`;
window.addEventListener('eip6963:announceProvider', ((event: CustomEvent) => {
  if (wallets.some((w) => w.info.uuid === event.detail.info.uuid)) return;
  wallets.push(event.detail);
  const o = document.createElement('option');
  o.value = String(wallets.length - 1);
  o.textContent = event.detail.info.name;
  el('wallets').append(o);
}) as EventListener);
window.dispatchEvent(new Event('eip6963:requestProvider'));
const run = (id: string, action: () => Promise<unknown>) =>
  (el(id).onclick = async () => {
    try {
      el('result').textContent = 'Изчакай прегледа и потвърди на Flipper…';
      const r = await action();
      el('result').textContent = JSON.stringify(
        r,
        (_, v) => (typeof v === 'bigint' ? v.toString() : v),
        2,
      );
    } catch (e) {
      el('result').textContent = e instanceof Error ? e.message : String(e);
    }
  });
run('connect', async () => {
  provider = wallets[Number(el('wallets').value)]?.provider;
  if (!provider) throw Error('Инсталирай разширението и презареди страницата.');
  [account] = await provider.request({ method: 'eth_requestAccounts' });
  el('account').textContent = account;
  return account;
});
run('revoke', () =>
  provider.request({ method: 'wallet_revokePermissions', params: [{ eth_accounts: {} }] }),
);
run('switch', () =>
  provider.request({
    method: 'wallet_switchEthereumChain',
    params: [{ chainId: toHex(Number(el('chain').value)) }],
  }),
);
run('send', () =>
  provider.request({
    method: 'eth_sendTransaction',
    params: [{ from: account, to: el('to').value, value: toHex(parseEther(el('value').value)) }],
  }),
);
run('siwe', async () => {
  const chainId = Number(await provider.request({ method: 'eth_chainId' }));
  const message = createSiweMessage({
    domain: location.host,
    address: account,
    uri: location.origin,
    version: '1',
    chainId,
    nonce: crypto.randomUUID().replaceAll('-', ''),
    statement: 'Sign in to the local Flipper Wallet demo.',
    issuedAt: new Date(),
    expirationTime: new Date(Date.now() + 600000),
  });
  const signature = await provider.request({
    method: 'personal_sign',
    params: [toHex(message), account],
  });
  const recovered = await recoverMessageAddress({ message, signature });
  if (recovered.toLowerCase() !== account.toLowerCase())
    throw Error('Signature verification failed');
  return { verified: true, account: recovered, scope: 'Local demo verification only' };
});
run('rejectTest', async () => {
  try {
    await provider.request({ method: 'eth_sign', params: [account, '0x' + '00'.repeat(32)] });
    throw Error('UNEXPECTED: raw signing was allowed');
  } catch (e: any) {
    if (e.code !== 4200) throw e;
    return { rejected: true, code: e.code };
  }
});
run('asset', () => {
  const op = el('operation').value,
    to = el('to').value,
    id = BigInt(el('token').value),
    amount = BigInt(el('units').value);
  let name: string, types: string[], args: any[];
  if (op.startsWith('ERC20')) {
    name = op === 'ERC20 transfer' ? 'transfer' : 'approve';
    types = ['address', 'uint256'];
    args = [to, op === 'ERC20 revoke' ? 0n : amount];
  } else if (op === 'ERC721') {
    name = 'safeTransferFrom';
    types = ['address', 'address', 'uint256'];
    args = [account, to, id];
  } else {
    name = 'safeTransferFrom';
    types = ['address', 'address', 'uint256', 'uint256', 'bytes'];
    args = [account, to, id, amount, '0x'];
  }
  const data = encodeFunctionData({
    abi: [
      {
        type: 'function',
        name,
        stateMutability: 'nonpayable',
        inputs: types.map((type) => ({ type })),
        outputs: [],
      },
    ],
    functionName: name,
    args,
  });
  return provider.request({
    method: 'eth_sendTransaction',
    params: [{ from: account, to: el('contract').value, value: '0x0', data }],
  });
});
run('permit', async () => {
  const chainId = Number(await provider.request({ method: 'eth_chainId' }));
  const verifyingContract = el('contract').value as `0x${string}`;
  const data = encodeFunctionData({
    abi: [
      {
        type: 'function',
        name: 'nonces',
        stateMutability: 'view',
        inputs: [{ type: 'address' }],
        outputs: [{ type: 'uint256' }],
      },
    ],
    functionName: 'nonces',
    args: [account],
  });
  const nonce = BigInt(
    await provider.request({
      method: 'eth_call',
      params: [{ to: verifyingContract, data }, 'latest'],
    }),
  );
  const typed = {
    domain: { name: 'Flipper Test Token', version: '1', chainId, verifyingContract },
    primaryType: 'Permit' as const,
    types: {
      EIP712Domain: [
        { name: 'name', type: 'string' },
        { name: 'version', type: 'string' },
        { name: 'chainId', type: 'uint256' },
        { name: 'verifyingContract', type: 'address' },
      ],
      Permit: [
        { name: 'owner', type: 'address' },
        { name: 'spender', type: 'address' },
        { name: 'value', type: 'uint256' },
        { name: 'nonce', type: 'uint256' },
        { name: 'deadline', type: 'uint256' },
      ],
    },
    message: {
      owner: account,
      spender: el('to').value as `0x${string}`,
      value: BigInt(el('units').value),
      nonce,
      deadline: BigInt(Math.floor(Date.now() / 1000) + 600),
    },
  } as const;
  const signature = await provider.request({
    method: 'eth_signTypedData_v4',
    params: [account, JSON.stringify(typed, (_, v) => (typeof v === 'bigint' ? v.toString() : v))],
  });
  const recovered = await recoverTypedDataAddress({
    ...typed,
    types: { Permit: typed.types.Permit },
    signature,
  });
  if (recovered.toLowerCase() !== account.toLowerCase()) throw Error('Permit signature mismatch');
  return {
    verified: true,
    account: recovered,
    signature,
    typed,
    scope: 'Local verification; not submitted on-chain',
  };
});
el('download').onclick = () => {
  const data = {
    schema: 1,
    configuration: el('configuration').value,
    feedback: el('feedback').value,
    paidPilotInterest: el('interest').value,
  };
  const url = URL.createObjectURL(
    new Blob([JSON.stringify(data, null, 2)], { type: 'application/json' }),
  );
  const a = document.createElement('a');
  a.href = url;
  a.download = 'flipper-wallet-feedback.json';
  a.click();
  URL.revokeObjectURL(url);
};
