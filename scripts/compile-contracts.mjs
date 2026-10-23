import solc from 'solc';
import { readFileSync, mkdirSync, writeFileSync } from 'node:fs';
const input = {
  language: 'Solidity',
  sources: { 'TestAssets.sol': { content: readFileSync('contracts/TestAssets.sol', 'utf8') } },
  settings: {
    optimizer: { enabled: true, runs: 200 },
    evmVersion: 'cancun',
    outputSelection: {
      '*': { '*': ['abi', 'evm.bytecode.object', 'evm.deployedBytecode.object'] },
    },
  },
};
const output = JSON.parse(
  solc.compile(JSON.stringify(input), {
    import(path) {
      try {
        return { contents: readFileSync('node_modules/' + path, 'utf8') };
      } catch {
        return { error: 'Import not found: ' + path };
      }
    },
  }),
);
for (const e of output.errors || []) console.error(e.formattedMessage);
if (output.errors?.some((e) => e.severity === 'error')) process.exit(1);
mkdirSync('build/contracts', { recursive: true });
for (const [name, artifact] of Object.entries(output.contracts['TestAssets.sol']))
  writeFileSync(
    `build/contracts/${name}.json`,
    JSON.stringify({ ...artifact, compiler: solc.version() }, null, 2) + '\n',
  );
console.log('Compiled testnet fixtures with ' + solc.version());
