# Dependencies and licensing

Original application/core/extension/test code is MIT. The selected crypto files remain unmodified from Trezor firmware commit30d30b37751c2d8e75da5746e18864948095f5db, with their copyright notices and license texts. The selected `.c` files implement BIP32/BIP39, secp256k1, hashing, KDF and ChaCha20-Poly1305; supporting headers are included without enabling every algorithm.

- `vendor/trezor-crypto/LICENSE`: Trezor crypto license.
- `vendor/trezor-crypto/chacha20poly1305/LICENSE`: retained component notice.
- Other included component headers retain their original notices.
- `vendor/trezor-crypto.sha256.json`: integrity inventory; changes require a pin/review update.
- `dependencies.json`: source/SDK provenance.
- `package-lock.json`: exact JS packages and registry integrity hashes.
- `assets/fonts/IBMPlexMono-{Regular,Medium,SemiBold}.ttf`: IBM Plex Mono for headings and controls, including Cyrillic, pinned in `dependencies.json` to Google Fonts commit `01d5d9f05246fb7090a8403b81257535cb1a088b`. SIL Open Font License 1.1 is retained in `assets/fonts/OFL.txt` and copied as `FONT_LICENSE.txt` in the extension and demo builds. The fonts are bundled locally; no external font service is contacted.

Crypto compile settings disable BIP32/BIP39 caches and 25519 derivation, enable Ethereum/Keccak and use size-optimized portable curve code. A local Furi adapter supplies CSPRNG bytes, a fail-stop fault handler and `explicit_bzero`, which Momentum87.1 does not export. Cryptographic algorithms are not replaced with homegrown versions.

The SDK and uFBT remain separately licensed external GPL build/firmware projects; they are not copied into this MIT repository. The USB implementation uses their public interfaces and standard HID descriptors. The FAP is designed for this firmware and not represented as a relicensing of it.

The extension uses React, viem and their transitive dependencies. Packaging collects production license files into `THIRD_PARTY_LICENSES.txt`. OpenZeppelin5.6.1 supplies test contract standards. The `tmp` dependency is pinned via override0.2.7 to resolve the advisory affecting0.2.6; `npm audit` must be rerun before a release.

Primary references: [WebHID in extensions](https://developer.chrome.com/docs/extensions/how-to/web-platform/webhid), [EIP1193](https://eips.ethereum.org/EIPS/eip-1193), [EIP6963](https://eips.ethereum.org/EIPS/eip-6963), [SIWE](https://eips.ethereum.org/EIPS/eip-4361), [EIP2612](https://eips.ethereum.org/EIPS/eip-2612), [Flipper crypto limitations](https://developer.flipper.net/flipperzero/doxygen/furi__hal__crypto_8h.html).
