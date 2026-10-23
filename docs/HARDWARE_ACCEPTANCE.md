# Remaining hands-on acceptance

Use a fresh disposable test wallet, the pinned Momentum build and a dedicated Chrome profile. Keep the recovery words and PIN on paper/device; never put them in screenshots, logs, bug reports or this repository. This checklist has **not** been performed by automation.

Record build SHA256, firmware version, browser version, date, test name and pass/fail. Store sensitive local material only under ignored `artifacts/private/`. Publish only redacted evidence. A checklist tick without an observed result does not close an acceptance gate.

## 1. Setup and recovery

1. Close qFlipper before opening EVM Wallet. Confirm Bluetooth advertising stops and the normal serial/RPC interface disappears while the wallet is active.
2. Create a new wallet on the device. Enter an 8–12 digit PIN twice. View all 24 words, keep them offline, and complete all three randomly chosen backup questions.
3. Unlock, verify all five public addresses on the device and record the public addresses privately for comparison.
4. Check manual lock and two-minute idle lock. A wrong PIN must fail; repeated failures must increase delay. Restarting the app may reset that delay, as documented.
5. Change PIN and reopen the app. New PIN works, old PIN fails. Preserve the backup throughout.
6. Restore using the device word-entry flow. All five addresses must match. Wrong word/checksum must fail without replacing the existing wallet.
7. Test on the same device and, separately if available, recovery from words on another device. Copying the encrypted SD record to another device must not decrypt it.

## 2. Confirmation and transport

Use the local demo through the installed extension. Verify complete sender/recipient/contract, network, action, amount, nonce and maximum fee on the Flipper.

- Prehold OK before starting a request: no approval until released and freshly held.
- Skip a required review page: no approval.
- Hold for less than two seconds: no signature; release resets the hold.
- Finish all pages and hold OK freshly for two seconds: signature verifies independently.
- Back/reject, 180-second expiry, unplug, new HELLO, malformed frame, reordered fragments and a competing request: old approval cannot be used.
- During wallet operation, qFlipper's serial remote control must be unavailable. Attempt Bluetooth reconnection and verify isolation. Software-source input events must cancel and never count as physical input; GPIO must also be pressed.
- Repeat at least 100 measured sign/deny/cancel cycles, including maximum-size supported messages. Record outcomes, minimum free heap after each group, crashes and resets. Require at least 16 KiB free heap and inspect task stack high-water marks. The existing 100 locked STATUS probes do not satisfy this test.
- Exit normally. USB and previous Bluetooth configuration recover. qFlipper reconnects. No seed/PIN remains available through protocol or diagnostics.

## 3. Storage faults

Only use disposable test data with an offline recovery backup. Work inside this application's SD directory, never other apps' files.

Test missing record, wrong PIN, altered ciphertext/header/tag, truncated files and unavailable SD. Fail closed. Inject interruption at pending write, sync/readback, primary-to-previous rename, promotion, verification and cleanup, including PIN changes. Recovery must produce exactly the old or new authenticated wallet, never partial data. An interrupted PIN change can require trying the old or new PIN. Missing primary may fall back to previous; an existing corrupt primary must not be silently replaced. SD copies cannot provide secure deletion or rollback resistance.

## 4. Browser and networks

- Chrome WebHID: select exactly one wallet device, grant access explicitly, connect site, reject then approve address sharing. Unapproved origins see no account. Revoke and restart extension; verify persistence/isolation. Keep another EIP6963 wallet installed to check coexistence. Repeat in Brave before claiming Brave support.
- Sepolia and Arbitrum Sepolia: faucet-funded ETH, ERC20 transfer/finite approve/zero revoke, ERC721 and ERC1155 single transfers; verify receipt success and exact resulting balances/ownership. Check both successful and rejected flows.
- Base transactions remain blocked until FEE_POLICY is explicitly resolved. Do not close the three-network gate based on local contract tests.
- Deploy the fixture contracts with a separate disposable test deployer. Review code hashes and EIP2612 name/version/domain before adding immutable firmware metadata. Preserve each receipt in `contracts/deployments.json`; reruns must not duplicate unknown transactions.
- Permit: verify signature, submit with correct nonce/deadline, and verify allowance change. Reuse, wrong chain/owner/domain, expired deadline, unknown contract and unlimited value must fail. The current production registry is empty.
- SIWE: compare the device clock to UTC, sign the exact displayed message and independently verify signer/domain/nonce/time in the demo and a real relying-party flow. Expired, wrong-origin and unsupported messages must fail.

## 5. Release

Record review findings and fix all critical defects. Download the actual release candidate artifacts, verify SHA256, reinstall on the tested firmware and repeat setup/signing/recovery. Update each `docs/acceptance.json` gate only with concrete evidence. `scripts/check-release.py` checks evidence presence; it is not an audit or a substitute for review. No accepted beta should be published while a mandatory gate is open.
