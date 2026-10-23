# Fee policy and unresolved Base requirement

The requested device review promises a **maximum total transaction fee**, not only an estimate. This is part of what the user approves.

- **Sepolia:** gasLimit × maxFeePerGas bounds execution cost.
- **Arbitrum Sepolia:** the signed gas limit covers execution and gas allocated for L1 posting, under the current Nitro fee model. Both use the same cap. An underestimated gas limit can make a transaction fail; it does not increase that signed limit.
- **Base Sepolia:** the separate L1 fee depends on network state at inclusion. `getL1FeeUpperBound` is an estimate against current oracle state, not a cap enforced by the signed EIP-1559 fields. Firmware therefore refuses Base **transactions**. Network discovery, read requests and restricted SIWE remain available; Permit additionally requires reviewed deployments.

This is an explicit unfinished requirement. Enabling Base transfers needs the user's decision to accept an execution cap plus a clearly labelled untrusted L1 fee estimate, or a different transaction/account design that enforces a total cap. Neither change has been silently adopted. The three-network release gate remains blocked.

Primary references checked 2026-10-23:

- [Arbitrum gas estimation and total fee formula](https://developer.arbitrum.io/arbitrum-essentials/how-to-estimate-gas)
- [Base network fees and oracle estimates](https://docs.base.org/specifications/transactions/network-fees)
- [OP Stack fee components](https://docs.optimism.io/op-stack/transactions/fees)
