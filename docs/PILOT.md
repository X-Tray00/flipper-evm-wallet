# Public testnet pilot — preparation

Status: prepared, not launched. No testers recruited, transactions claimed or revenue booked.

## Offer A: technical support

Assistance installing a verified build, diagnosing supported Momentum/Chrome configurations and recovering a disposable test wallet using the user's own device. Support never asks for seed words, PINs or keys. Scope, response windows and pricing must be agreed before any paid engagement. There is no current SLA or custody service.

## Offer B: integration development

Implement bounded contract-call renderers, testnet integrations and compatibility work, with explicit supported methods, firmware review screens and negative tests. Every new operation must fail closed outside its approved schema. Production custody or certification is not included.

## Demand experiment

Targets, not forecasts:10 external testers; at least8 complete onboarding, backup recovery, address verification and one supported testnet transaction from the guide; at least3 explicitly request a paid pilot for support or integration. Record failures and support time, not just signups. Stop or revise the offer if hardware/UX constraints dominate the benefit.

## Demo and feedback

Run `npm run demo` locally. Demonstrate connection, account grant/revocation, an ETH transfer, a supported asset call, rejection and SIWE. Demonstrations must show the actual hardware confirmation and use valueless test assets. No claim that a host simulation is a device signing demo.

The demo feedback form downloads a JSON file locally; it sends nothing automatically. Ask testers to share it deliberately through an agreed channel. Questions: configuration, steps completed, failure point, clarity of the device review, recovery experience, support minutes and optional interest in support/integration. Never collect seed/PIN, SD ciphertext, balances of real wallets or unnecessary personal data.

MIT permits others to reuse and sell the code. Commercial differentiation must come from tested compatibility, usability, maintenance and service quality. Product claims must continue to disclose stock Flipper security limitations and the absence of an independent audit. A commercial USB identifier strategy and brand/legal review remain required before distribution as a commercial hardware wallet.
