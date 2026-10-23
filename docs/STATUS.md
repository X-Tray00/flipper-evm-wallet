# Implementation status — 2026-10-23

**Development build; public beta acceptance is incomplete. No real-money use or external audit.**

## Implementation delivered

The Momentum application, crypto/vault/transaction core, USB protocol, Chrome extension, site permissions, BG/EN UI, local demo, test contracts, build/CI/package tooling, recovery guides, threat model and pilot materials are implemented. See README and PROTOCOL for the actual supported subset.

Two functional requirements remain deliberately closed:

1. Base transaction signing refuses because the specified maximum total fee cannot be enforced by Base EIP-1559 fields. See FEE_POLICY. Sepolia and Arbitrum support the signed gas fee cap.
2. Permit parsing/hashing and independent tests exist, but the production allowlist is empty. It requires confirmed testnet deployments, reviewed domain metadata and device tests. Host fixtures never enter the production FAP.

## Verified locally

| Evidence | Result | Scope |
| --- | --- | --- |
| Native C core, storage and parser tests | 3 suites passed, UBSan | Host only; Apple ASan runtime unavailable |
| Independent address/hash/signature/vault checks | 37 passed, viem/noble + Node OpenSSL | Host only |
| Parser mutations | 70,000 random and structured mutations plus approval boundaries | Host only; no hardware fuzz proof |
| Extension protocol and policy | 11 tests passed | Includes late cancelled replies |
| TypeScript and extension/demo production builds | Passed | Source checks/build only |
| Chromium extension tests | Passed, version in artifacts/browser-results.json | Isolated temporary profile; no HID signing |
| Contract fixtures | 2 Foundry tests passed, covering 3 allowed chain IDs and mainnet rejection | Local EVM, not public-network receipts |
| Dependency vulnerability database check | No reported vulnerabilities at time of run | No claim of absence of vulnerabilities |
| FAP compilation and API imports | Momentum mntm-012 / API87.1 passed | See firmware build receipt |

## Device evidence

Connected device reports Momentum mntm-012 (e1784e74), API87.1. The wallet is installed separately under Tools. The read-only engineering probe uses public zero-entropy fixtures and does not open a user vault.

Observed probe: all five independent addresses, known signature, authenticated vault with the actual device wrapping key, tamper rejection, USB session and rejection of locked signing. It completes 100 **locked status** exchanges; these are not 100 physical signing/cancel cycles. Hash-bound final results are recorded in artifacts/hardware-probe.json when the probe completes.

## Required before public beta

- Physical new-wallet/backup quiz, PIN change, lock/unplug, and recovery of the same five addresses.
- Fresh hold and all-pages review; preheld/stale/remote button rejection; competing request, timeout, disconnect tests on hardware.
- SD damage and actual interrupted-write recovery; 100 physical sign/cancel cycles; worst-case heap and stack measurement.
- Chrome with granted WebHID permission through the entire site-to-device flow. Brave remains unverified.
- Faucet funds and confirmed supported-operation receipts on the planned networks; reviewed Permit deployments; real SIWE verification.
- Resolve the Base total-fee requirement. Full three-network support is not claimed.
- Independent security review, clean-build reproducibility evidence and downloaded release reinstallation. External audit remains a later prerequisite for considering real assets.

The user is away. Physical entry/approval is intentionally not replaced by qFlipper remote input. No user seed/PIN has been created, read or transmitted by these tests. No public-network transaction has been broadcast.

The machine-readable acceptance checklist stays false where evidence is incomplete. A development source publication and development artifacts are distinct from an accepted beta release.
