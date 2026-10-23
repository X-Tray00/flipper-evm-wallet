import { getAddress, isAddress, type Address, type Hex } from 'viem';
export const CHAINS = {
  11155111: {
    name: 'Sepolia',
    rpc: 'https://ethereum-sepolia-rpc.publicnode.com',
    explorer: 'https://sepolia.etherscan.io',
  },
  84532: {
    name: 'Base Sepolia',
    rpc: 'https://sepolia.base.org',
    explorer: 'https://sepolia.basescan.org',
  },
  421614: {
    name: 'Arbitrum Sepolia',
    rpc: 'https://sepolia-rollup.arbitrum.io/rpc',
    explorer: 'https://sepolia.arbiscan.io',
  },
} as const;
export type ChainId = keyof typeof CHAINS;
export class WalletError extends Error {
  constructor(
    public code: number,
    message: string,
  ) {
    super(message);
  }
}
export function fail(code: number, message: string): never {
  throw new WalletError(code, message);
}
export function chainId(value: unknown): ChainId {
  const id = Number(value);
  if (!Number.isSafeInteger(id) || !(id in CHAINS))
    fail(4902, 'Only Sepolia, Base Sepolia and Arbitrum Sepolia are supported');
  return id as ChainId;
}
export function address(value: unknown): Address {
  if (typeof value !== 'string' || !isAddress(value)) fail(-32602, 'Invalid address');
  return getAddress(value);
}
export function hex(value: unknown, max = 8192): Hex {
  if (
    typeof value !== 'string' ||
    !/^0x(?:[0-9a-fA-F]{2})*$/.test(value) ||
    value.length > 2 + max * 2
  )
    fail(-32602, 'Invalid bounded hex data');
  return value as Hex;
}
export function quantity(value: unknown, max = (1n << 256n) - 1n): bigint {
  if (typeof value !== 'string' || !/^0x(?:0|[1-9a-fA-F][0-9a-fA-F]*)$/.test(value))
    fail(-32602, 'Noncanonical quantity');
  const n = BigInt(value);
  if (n > max) fail(-32602, 'Quantity exceeds supported range');
  return n;
}
export function webOrigin(url: string | undefined): string {
  if (!url) fail(4100, 'Missing browser origin');
  const u = new URL(url);
  if (
    u.protocol !== 'https:' &&
    !(u.protocol === 'http:' && (u.hostname === 'localhost' || u.hostname === '127.0.0.1'))
  )
    fail(4100, 'HTTPS or local demo required');
  if (u.origin.length > 127) fail(-32602, 'Origin too long');
  return u.origin;
}
export function sameAddress(a: string, b: string) {
  return a.toLowerCase() === b.toLowerCase();
}
export function accountIndex(value: unknown) {
  const n = Number(value);
  if (!Number.isInteger(n) || n < 0 || n >= 5) fail(-32602, 'Account must be 0–4');
  return n;
}
export function object(value: unknown): Record<string, unknown> {
  if (!value || typeof value !== 'object' || Array.isArray(value)) fail(-32602, 'Expected object');
  return value as Record<string, unknown>;
}
export function exactKeys(obj: Record<string, unknown>, allowed: string[]) {
  if (Object.keys(obj).some((k) => !allowed.includes(k))) fail(-32602, 'Unsupported fields');
}
