import assert from 'node:assert/strict';
import { execFileSync } from 'node:child_process';
import { pbkdf2Sync, createDecipheriv } from 'node:crypto';
import { mnemonicToAccount } from 'viem/accounts';
import { createSiweMessage } from 'viem/siwe';
import {
  serializeTransaction,
  keccak256,
  recoverAddress,
  encodeFunctionData,
  hashMessage,
  hashTypedData,
  toHex,
} from 'viem';
const executable = process.env.WALLET_VECTOR_BIN || 'build/native/wallet_vectors';
const query = (...args) => JSON.parse(execFileSync(executable, args, { encoding: 'utf8' }));
const mnemonic = Array(23).fill('abandon').join(' ') + ' art';
let tests = 0;
for (let i = 0; i < 5; i++) {
  const account = mnemonicToAccount(mnemonic, { addressIndex: i });
  assert.equal(query('address', String(i)).address.toLowerCase(), account.address.toLowerCase());
  tests++;
}
const account = mnemonicToAccount(mnemonic),
  target = '0x1111111111111111111111111111111111111111';
const base = {
  type: 'eip1559',
  chainId: 11155111,
  nonce: 7,
  gas: 21000n,
  maxPriorityFeePerGas: 2000000000n,
  maxFeePerGas: 30000000000n,
  to: target,
  value: 1234500000000000000n,
  data: '0x',
  accessList: [],
};
async function good(tx) {
  const wire = serializeTransaction(tx),
    got = query('tx', wire.slice(2));
  assert.equal(got.error, 0);
  assert.equal(got.hash, keccak256(wire));
  const sig = {
    r: got.signature.slice(0, 66),
    s: '0x' + got.signature.slice(66, 130),
    yParity: Number.parseInt(got.signature.slice(130), 16),
  };
  assert.equal(
    (await recoverAddress({ hash: got.hash, signature: sig })).toLowerCase(),
    account.address.toLowerCase(),
  );
  for (const p of got.pages) assert.ok(Buffer.from(p.textHex, 'hex').length <= 80);
  const feePage = got.pages.find((p) => p.title === 'Max total fee (wei)');
  assert.equal(
    Buffer.from(feePage.textHex, 'hex').toString(),
    (tx.gas * tx.maxFeePerGas).toString(),
  );
  tests++;
}
await good(base);
assert.notEqual(query('tx', serializeTransaction({ ...base, chainId: 84532 }).slice(2)).error, 0);
tests++;
await good({ ...base, chainId: 421614 });
await good({ ...base, gas: (1n << 32n) - 1n, maxFeePerGas: (1n << 64n) - 1n });
const call = (name, types, args) =>
  encodeFunctionData({
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
for (const data of [
  call('transfer', ['address', 'uint256'], [target, 999n]),
  call('approve', ['address', 'uint256'], [target, 12n]),
  call('approve', ['address', 'uint256'], [target, 0n]),
  call('safeTransferFrom', ['address', 'address', 'uint256'], [account.address, target, 17n]),
  call(
    'safeTransferFrom',
    ['address', 'address', 'uint256', 'bytes'],
    [account.address, target, 17n, '0x'],
  ),
  call(
    'safeTransferFrom',
    ['address', 'address', 'uint256', 'uint256', 'bytes'],
    [account.address, target, 17n, 3n, '0x'],
  ),
])
  await good({ ...base, value: 0n, gas: 100000n, data });
for (const tx of [
  { ...base, chainId: 1 },
  { ...base, data: '0xdeadbeef' },
  {
    ...base,
    data: call('approve', ['address', 'uint256'], [target, (1n << 256n) - 1n]),
    value: 0n,
  },
  { ...base, accessList: [{ address: target, storageKeys: [] }] },
  {
    ...base,
    data: call('safeTransferFrom', ['address', 'address', 'uint256'], [target, target, 17n]),
    value: 0n,
  },
]) {
  assert.notEqual(query('tx', serializeTransaction(tx).slice(2)).error, 0);
  tests++;
}
const wire = serializeTransaction(base);
for (const v of [wire + '00', '0x01' + wire.slice(4), wire.slice(0, -2)]) {
  assert.notEqual(query('tx', v.slice(2)).error, 0);
  tests++;
}
const siwe = `example.com wants you to sign in with your Ethereum account:\n${account.address}\n\nSign in to the local test.\n\nURI: https://example.com/login\nVersion: 1\nChain ID: 11155111\nNonce: abcdef123456\nIssued At: 2026-10-23T10:00:00Z\nExpiration Time: 2026-10-23T11:00:00Z`;
const now = String(Date.parse('2026-10-23T10:30:00Z') / 1000);
const noStatement = createSiweMessage({
  domain: 'example.com',
  address: account.address,
  uri: 'https://example.com',
  version: '1',
  chainId: 11155111,
  nonce: 'abcdefgh',
  issuedAt: new Date('2026-10-23T10:00:00Z'),
});
assert.equal(
  query('siwe', Buffer.from(noStatement).toString('hex'), 'https://example.com', now).error,
  0,
);
tests++;
const got = query('siwe', Buffer.from(siwe).toString('hex'), 'https://example.com', now);
assert.equal(got.error, 0);
assert.equal(got.hash, hashMessage(siwe));
tests++;
for (const bad of [
  siwe.replace('Chain ID: 11155111', 'Chain ID: 1'),
  siwe.replace('example.com wants', 'evil.com wants'),
  siwe + '\nChain ID: 1',
  siwe.replace('Nonce: abcdef123456', 'Nonce: abc'),
  siwe.replace('2026-10-23T11:00:00Z', '2026-10-23T09:00:00Z'),
  siwe.replace(account.address, target),
]) {
  assert.notEqual(
    query('siwe', Buffer.from(bad).toString('hex'), 'https://example.com', now).error,
    0,
  );
  tests++;
}
// Native test registry only. Production firmware has no unreviewed deployments.
const permitMessage = {
  owner: account.address,
  spender: target,
  value: 123n,
  nonce: 7n,
  deadline: BigInt(now) + 3600n,
};
const domain = {
  name: 'Flipper Test Token',
  version: '1',
  chainId: 11155111,
  verifyingContract: target,
};
const permitTypes = {
  Permit: [
    { name: 'owner', type: 'address' },
    { name: 'spender', type: 'address' },
    { name: 'value', type: 'uint256' },
    { name: 'nonce', type: 'uint256' },
    { name: 'deadline', type: 'uint256' },
  ],
};
const permit = Buffer.alloc(136);
permit.writeUInt32LE(11155111);
Buffer.from(target.slice(2), 'hex').copy(permit, 4);
Buffer.from(account.address.slice(2), 'hex').copy(permit, 24);
Buffer.from(target.slice(2), 'hex').copy(permit, 44);
Buffer.from(toHex(123n, { size: 32 }).slice(2), 'hex').copy(permit, 64);
Buffer.from(toHex(7n, { size: 32 }).slice(2), 'hex').copy(permit, 96);
permit.writeBigUInt64BE(permitMessage.deadline, 128);
const signedPermit = query('permit', permit.toString('hex'), now);
assert.equal(signedPermit.error, 0);
assert.equal(
  signedPermit.hash,
  hashTypedData({ domain, types: permitTypes, primaryType: 'Permit', message: permitMessage }),
);
tests++;
for (const index of [0, 4, 24, 64]) {
  const bad = Buffer.from(permit);
  bad[index] ^= 1;
  assert.notEqual(query('permit', bad.toString('hex'), now).error, 0);
  tests++;
}
// Independent OpenSSL implementation verifies the exact vault AEAD and KDF format.
const record = Buffer.from(execFileSync(executable, ['vault'], { encoding: 'utf8' }).trim(), 'hex');
assert.equal(record.length, 128);
const key = pbkdf2Sync('12345678', record.subarray(12, 28), 100000, 32, 'sha256');
const dec = createDecipheriv('chacha20-poly1305', key, record.subarray(44, 56), {
  authTagLength: 16,
});
dec.setAAD(record.subarray(0, 64), { plaintextLength: 48 });
dec.setAuthTag(record.subarray(112, 128));
const plain = Buffer.concat([dec.update(record.subarray(64, 112)), dec.final()]);
assert.deepEqual(plain.subarray(0, 32), Buffer.alloc(32));
tests++;
console.log(
  JSON.stringify({
    independent_checks: tests,
    implementations: ['viem/noble', 'Node OpenSSL'],
    hardware: false,
  }),
);
