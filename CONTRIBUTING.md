# Contributing

Use synthetic keys and testnets. Keep changes narrowly scoped and preserve dependency licenses. Never add a USB command for secret export, PIN entry, arbitrary hash signing or automated approval. Never include a production bypass for physical buttons.

Before a pull request: run native tests, independent vectors, TypeScript checks, extension tests/build and browser tests. Parser or policy changes need adversarial cases and review-page assertions. Storage changes need interrupted-write cases. Firmware changes need an API import check and a separate hardware evidence record. Clearly label any unrun test.

Keep `core/registry.c` empty until a testnet deployment has a verified address, chain, bytecode/receipt evidence, EIP2612 domain and successful hardware/browser tests. Host-only fixture entries belong in `tests/native/registry_fixture.c` and must never be copied into a device build without deployment review.

Changing a dependency requires updating its exact pin, integrity manifest, notices and differential results. No telemetry or hidden network endpoints. The release gate is `docs/acceptance.json`; changing a boolean is not evidence. Attach the corresponding results and test receipts.
