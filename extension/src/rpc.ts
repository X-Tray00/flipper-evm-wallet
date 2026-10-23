import { CHAINS, fail, type ChainId } from './policy';
let nextId = 1;
async function post(chain: ChainId, method: string, params: unknown[] = []): Promise<any> {
  const id = nextId++;
  const response = await fetch(CHAINS[chain].rpc, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ jsonrpc: '2.0', id, method, params }),
    credentials: 'omit',
    referrerPolicy: 'no-referrer',
    signal: AbortSignal.timeout(15000),
  });
  if (!response.ok) fail(-32603, `RPC HTTP ${response.status}`);
  const text = await response.text();
  if (text.length > 2_000_000) fail(-32603, 'RPC response too large');
  const data = JSON.parse(text);
  if (data.id !== id || data.jsonrpc !== '2.0') fail(-32603, 'Invalid RPC response');
  if (data.error) fail(-32603, String(data.error.message || 'RPC failed').slice(0, 300));
  return data.result;
}
export async function rpc(chain: ChainId, method: string, params: unknown[] = []) {
  const reported = await post(chain, 'eth_chainId');
  if (reported !== `0x${chain.toString(16)}`)
    fail(4901, 'RPC endpoint returned a different network');
  return method === 'eth_chainId' ? reported : post(chain, method, params);
}
