# Development user guide

Use a fresh disposable test wallet. Do not import a funded wallet. This build has not passed the public-beta acceptance gates.

Install the FAP in `/ext/apps/Tools/` using qFlipper on Momentum mntm-012/API87.1. Load `dist/extension` unpacked in a dedicated Chrome test profile. Open EVM Wallet on Flipper; it takes exclusive USB ownership and disables Bluetooth until exit.

## Device controls

- Home: Up/Down selects actions; Left/Right selects one of five accounts; OK enters; Back exits.
- PIN: Up/Down selects a digit, Right appends it, Left deletes, OK submits. Use 8–12 digits and repeat the new PIN.
- New wallet: record all 24 English words. Left/Right changes word; after visiting all words, OK starts a three-word quiz.
- Word entry: Up/Down chooses a letter, Right appends, Left deletes, OK completes a unique prefix. Four letters are sufficient for English BIP39 words; enter shorter words in full.
- Review: visit every page, release OK, then hold physical OK for two seconds. Back rejects. Remote input cannot approve.
- Expensive PIN derivation can take time. Keep USB connected while setup completes.

## Connect and sign

Unlock on device, choose Connect Flipper in the extension and select `EVM Wallet Testnet`. Verify an address physically before receiving test assets. Obtain faucet ETH only on the selected testnet. Use Send for ETH, raw ERC20 units/allowances and NFT transfers. Compare the full recipient, contract, action and total gas fee on device.

Sepolia/Arbitrum show the total gas fee cap. Base transactions are currently refused because the separate L1 fee is not capped by EIP-1559; see FEE_POLICY.md. Unknown token names/decimals are not trusted. Recognized selectors do not prove a contract's behavior. Permit remains unavailable until reviewed deployments populate the device registry.

Sites must request account access first. Revoke access in Connected sites; changing accounts revokes previous grants. SIWE requires a correct UTC device clock and supported bounded message format. The host-provided origin is not independently authenticated by Flipper.

## Recover

Choose Restore 24 words from the locked menu, confirm replacement, enter words on device and set a new PIN. Invalid checksums do not replace the current wallet. Compare all five public addresses after restoration. Derivation is `m/44'/60'/0'/0/i`, i=0..4, English BIP39, empty BIP39 passphrase.

Never send recovery words or PINs to support. SD ciphertext alone cannot recover on another Flipper; use the words. A missing primary record after interrupted rename can use the previous committed encrypted record. An existing corrupt primary fails closed. Old SD snapshots and modified firmware remain outside the protection guarantee.

Lock manually, or let two minutes of inactivity lock the wallet. Active review expires after three minutes. USB disconnect cancels and locks. Exit physically to return USB ownership to qFlipper.
