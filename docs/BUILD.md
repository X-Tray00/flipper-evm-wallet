# Build and reproduce

## Pins

- Momentum mntm-012 / commit e1784e7418d8b074e971983ceb6fef0f37e52ae4 / API87.1.
- SDK SHA256: c90530c3e0aa7bc4356b8521df20d23d7b4a5435ab58a2530ffb696200408cc2.
- Trezor firmware crypto commit30d30b37751c2d8e75da5746e18864948095f5db. Selected unmodified sources and retained license files are inventoried in `vendor/trezor-crypto.sha256.json`.
- uFBT0.2.6; Python dependencies in requirements-dev.txt. Node22.22.3 / npm12.2.1. Exact JS versions and integrity hashes in package-lock.json. Foundry1.7.1; Solidity0.8.38.

Install Python3, CMake, a C compiler and Node22.22.3. Create `.venv` and install requirements. Run `npm ci --ignore-scripts` with npm12.2.1. Do not run an automatic major-version dependency update as part of a build.

`scripts/setup-sdk.sh` downloads the SDK, verifies its checksum and installs it into `.ufbt/` in this repository. It does not replace `~/.ufbt`. `scripts/build-firmware.sh` refuses another Momentum SDK, verifies vendored file hashes and runs uFBT's API import check. A successful build writes `dist/firmware-build.json`; packaging checks its FAP hash.

```sh
scripts/setup-sdk.sh
scripts/build-firmware.sh
npm run build
node scripts/package.mjs
```

Output: `dist/packages/flipper-evm-wallet-mntm012-dev.fap`, extension ZIP and SHA256SUMS. These names intentionally say **dev**, not beta. To install, exit any current Flipper app, use qFlipper to copy the FAP under Tools, then open it manually. Check the firmware version first. Do not overwrite ZeroFIDO or flash a new full firmware.

## Tests

Use the commands in the README. Host-only executables use a public, synthetic Permit registry that is **not** compiled by `application.fam`. Trezor BIP32/BIP39 caching is disabled in both builds. Hardware randomness comes from Furi; native randomness uses the operating system's CSPRNG. No fallback pseudo-random generator is used in wallet code.

On macOS the local Apple Clang ASan runtime failed before main even for a minimal hello-world executable. CMake therefore selects UBSan on Apple and ASan+UBSan elsewhere. Linux CI is required for ASan evidence. Sanitizer jobs and parser mutation counts are models, not device tests.

`tests/browser.mjs` creates and deletes its own temporary Chromium profile. It never opens or modifies the user's Chrome profile. It tests extension permissions/provider behavior and saves a UI screenshot. Direct Chrome WebHID access to physical hardware requires a user-granted device permission and is a separate acceptance gate.

## Hardware probe

With the device in the main menu and qFlipper closed:

```sh
.venv/bin/python scripts/hardware_probe.py \
  --port /dev/cu.usbmodemflip_DEVICE1 \
  --sdk /absolute/path/to/sdk \
  --fap dist/flipper_evm_wallet.fap
```

This replaces only this project's FAP under Tools and starts `probe`. It runs public BIP44/address/signature vectors, authenticated device wrapping and tamper rejection, then checks100 locked status cycles. It does not load the user vault, generate a user seed, approve anything or broadcast transactions. The full self-check can take around85s on the tested device before USB HID appears. This timing is for the combined self-check, not a measured single PIN unlock.

No beta release is permitted by `scripts/check-release.py` until each mandatory evidence entry is complete. The script does not fabricate external/manual results. Keep development packages separate from accepted public release assets.
