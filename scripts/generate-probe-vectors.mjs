// Public, disposable vectors. No wallet or user key material is read.
import { mnemonicToAccount } from 'viem/accounts';
import { serializeTransaction, keccak256, recoverAddress } from 'viem';
import { execFileSync } from 'node:child_process';
import { writeFileSync } from 'node:fs';
import assert from 'node:assert/strict';
const phrase = Array(23).fill('abandon').join(' ') + ' art';
const account = mnemonicToAccount(phrase);
const tx = serializeTransaction({
  type: 'eip1559',
  chainId: 11155111,
  nonce: 0,
  gas: 21000n,
  maxFeePerGas: 30000000000n,
  maxPriorityFeePerGas: 2000000000n,
  to: '0x1111111111111111111111111111111111111111',
  value: 1n,
  accessList: [],
});
const native = JSON.parse(
  execFileSync('build/native/wallet_vectors', ['tx', tx.slice(2)], { encoding: 'utf8' }),
);
assert.equal(native.error, 0);
assert.equal(native.hash, keccak256(tx));
const signature = {
  r: native.signature.slice(0, 66),
  s: '0x' + native.signature.slice(66, 130),
  yParity: parseInt(native.signature.slice(130), 16),
};
assert.equal(await recoverAddress({ hash: native.hash, signature }), account.address);
const bytes = (h) =>
  '{' +
  Array.from(
    Buffer.from(h.replace(/^0x/, ''), 'hex'),
    (n) => '0x' + n.toString(16).padStart(2, '0'),
  ).join(',') +
  '}';
const addresses = Array.from(
  { length: 5 },
  (_, i) => mnemonicToAccount(phrase, { addressIndex: i }).address,
);
writeFileSync(
  'firmware/probe_vectors.h',
  '/* Generated PUBLIC zero-entropy test vectors. Not user credentials. */\n#pragma once\n#include <stdint.h>\nstatic const uint8_t probe_addresses[5][20]={' +
    addresses.map(bytes).join(',') +
    '};\nstatic const uint8_t probe_transaction[]=' +
    bytes(tx) +
    ';\nstatic const uint8_t probe_signature[65]=' +
    bytes(native.signature) +
    ';\n',
);
