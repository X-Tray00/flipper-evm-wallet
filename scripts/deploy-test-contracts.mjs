// Requires a separately funded, disposable test-only deployer supplied by the operator.
// NEVER use this script with a Flipper wallet seed or a real-money private key.
import { createPublicClient, createWalletClient, http, keccak256, encodeDeployData } from 'viem';
import { privateKeyToAccount } from 'viem/accounts';
import { sepolia, baseSepolia, arbitrumSepolia } from 'viem/chains';
import { readFileSync, writeFileSync, openSync, fsyncSync, closeSync, renameSync } from 'node:fs';
const networks = {
  11155111: { chain: sepolia, rpc: 'https://ethereum-sepolia-rpc.publicnode.com' },
  84532: { chain: baseSepolia, rpc: 'https://sepolia.base.org' },
  421614: { chain: arbitrumSepolia, rpc: 'https://sepolia-rollup.arbitrum.io/rpc' },
};
const id = Number(process.argv[2]),
  network = networks[id];
if (!network) throw Error('Select an allowed test chain ID');
const key = process.env.TEST_DEPLOYER_PRIVATE_KEY;
if (!/^0x[0-9a-fA-F]{64}$/.test(key || ''))
  throw Error(
    'Supply a disposable TEST_DEPLOYER_PRIVATE_KEY privately. Nothing has been submitted.',
  );
const account = privateKeyToAccount(key),
  publicClient = createPublicClient({ chain: network.chain, transport: http(network.rpc) }),
  wallet = createWalletClient({ account, chain: network.chain, transport: http(network.rpc) });
if ((await publicClient.getChainId()) !== id) throw Error('RPC chain mismatch');
const manifest = JSON.parse(readFileSync('contracts/deployments.json', 'utf8'));
const entries = manifest.networks[id];
function saveManifest() {
  manifest.status = 'Deployment work in progress; not approved for firmware registry';
  const temporary = 'contracts/deployments.pending.json';
  writeFileSync(temporary, JSON.stringify(manifest, null, 2) + '\n', { flag: 'w' });
  const fd = openSync(temporary, 'r');
  fsyncSync(fd);
  closeSync(fd);
  renameSync(temporary, 'contracts/deployments.json');
}

for (const name of ['FlipperTestToken', 'FlipperTestNFT', 'FlipperTestMultiToken']) {
  const artifact = JSON.parse(readFileSync(`build/contracts/${name}.json`, 'utf8'));
  const existing = entries[name];
  if (existing?.address) {
    const deployed = await publicClient.getCode({ address: existing.address });
    if (!deployed || keccak256(deployed) !== existing.runtimeCodeHash)
      throw Error('Previously recorded runtime changed; manual review required');
    continue;
  }
  let hash = existing?.transactionHash;
  if (!hash) {
    const prepared = await wallet.prepareTransactionRequest({
      account,
      data: encodeDeployData({ abi: artifact.abi, bytecode: '0x' + artifact.evm.bytecode.object }),
    });
    const serializedTransaction = await wallet.signTransaction(prepared);
    hash = keccak256(serializedTransaction);
    // Persist the identity BEFORE broadcast. Unknown outcomes never cause an automatic duplicate.
    entries[name] = { status: 'pending', transactionHash: hash, compiler: artifact.compiler };
    saveManifest();
    const submitted = await publicClient.sendRawTransaction({ serializedTransaction });
    if (submitted !== hash) throw Error('RPC returned a different deployment hash');
  }
  const receipt = await publicClient.waitForTransactionReceipt({ hash });
  if (receipt.status !== 'success' || !receipt.contractAddress) throw Error('Deployment failed');
  const code = await publicClient.getCode({ address: receipt.contractAddress });
  if (!code || code === '0x') throw Error('Missing deployed code');
  entries[name] = {
    address: receipt.contractAddress,
    transactionHash: hash,
    blockNumber: receipt.blockNumber.toString(),
    runtimeCodeHash: keccak256(code),
    compiler: artifact.compiler,
  };
  // Save each confirmed step so partial runs retain verifiable receipts.
  manifest.networks[id] = entries;
  manifest.status = 'Deployments recorded; not yet approved for firmware registry';
  saveManifest();
  console.log(name, receipt.contractAddress, hash);
}
console.log(
  'Verify runtime and EIP2612 domain, then review registry changes and run hardware tests. No firmware registry was changed automatically.',
);
