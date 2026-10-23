# Security policy

## Intended use

Fresh, disposable test wallets only. This project is not FIDO certified, a certified signing device, a secure element wallet, or an audited custody product. Mainnet signing is rejected by the device parser, not merely hidden by the extension.

Flipper's own [crypto API documentation](https://developer.flipper.net/flipperzero/doxygen/furi__hal__crypto_8h.html) states that its hardware/firmware is not designed as a secure key vault. Physical debugging, malicious firmware and a modified FAP can read private material. A compromised computer can lie about its domain and RPC responses. Always inspect the device screen, including complete addresses and raw units.

## Reporting a vulnerability

Use the GitHub repository's **Security → Report a vulnerability** private reporting channel when enabled. If unavailable, open an issue containing only a request for a private contact channel; do not include exploits, seed phrases, PINs, SD records, user addresses or sensitive logs in a public issue. There is currently no paid bug bounty, response-time SLA or dedicated staffed incident service.

Include the commit/build hash, Momentum version, affected operation, minimal synthetic reproduction, expected/actual behavior and whether the result was observed on hardware or a host model. Use public test vectors only.

## Release blockers

Any key extraction through the protocol, signature without fresh physical approval, mismatch between displayed and signed fields, parser memory corruption or origin-permission bypass blocks beta publication. Report unsupported behavior and incomplete gates as failures or pending, never as successful checks.

An independent external audit is required before considering real assets, and the stock Flipper hardware limitations remain after an audit. No such audit has been performed.

## Current limitations

- No hardware anti-rollback counter or verified boot. SD snapshots can restore older PIN protection; session backoff resets on application restart.
- PIN processing is deliberately expensive and can take substantial time. Secrets are wiped at the next safe boundary of an in-flight cryptographic operation; USB loss invalidates pending authorization and prevents its response.
- No claim of secure erasure of old SD sectors, protection against firmware/debugger access, resistance to all timing/power attacks, or trustworthy host-reported origins.
- Contract metadata is not inferred from websites or token symbols. Calling a recognized selector does not prove a contract's behavior.
- Sepolia/Arbitrum total gas cost is bounded by gasLimit × maxFeePerGas. Base transactions are refused because the separate L1 charge is not bounded by these fields; see docs/FEE_POLICY.md.
- SIWE uses the device RTC as UTC, supports a deliberately bounded ASCII subset, and rejects unsupported optional fields. A wrong device clock can cause rejection or incorrect time checks.
- Development USB VID/PID uses the existing ST/Flipper identifier with vendor usage page `0xff71`. A commercial distribution must resolve USB identifier allocation/authorization.
- Physical approval, power-cut SD testing and 100 real approval/sign/cancel cycles are still pending. See the acceptance ledger.
