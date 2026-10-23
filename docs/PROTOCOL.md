# USB protocol v1

Vendor usage page `0xff71`, usage1. Development VID/PID `0483:5741`; not a claim of allocated commercial identifiers. One HID input and one output report, 64 bytes, report ID0. The host API's leading report-ID byte is not part of these 64 bytes. No CDC or FIDO interface is exposed during normal wallet operation.

## Frame

| Offset | Bytes | Encoding |
| --- | --- | --- |
| 0 | 2 | ASCII `FW` |
| 2 | 1 | Version1 |
| 3 | 1 | Command; responses set bit7 |
| 4 | 4 | Session ID, little endian |
| 8 | 4 | Request ID, little endian |
| 12 | 2 | Complete payload size, little endian, <=8192 |
| 14 | 2 | Fragment sequence, little endian, starts0 |
| 16 | 4 | Reserved, zero |
| 20 | 44 | Payload bytes followed by zero padding |

HELLO is one empty frame with session0 and nonzero request ID. Device generates a new random nonzero session. Within a session, non-HELLO IDs increase monotonically; do not wrap them. One request may be assembling or under review. Altered metadata, out-of-order fragments, oversize, concurrent traffic and stale session cancel. Timeout is180s, host waiting bound185s. No request authenticates the computer cryptographically.

| Command | ID | Request body | Success data after status byte |
| --- | --- | --- | --- |
| HELLO |1| empty | protocol1, minor0, capability revision0, min free heap u32 |
| ADDRESS |2| envelope, no typed body |20 address bytes after physical confirmation |
| TRANSACTION |3| envelope, unsigned type2 canonical RLP |r32,s32,recovery parity1 |
| SIWE |4| envelope, original ASCII SIWE bytes |r32,s32,recovery parity1 |
| PERMIT |5| envelope, fixed136-byte object |r32,s32,recovery parity1 |
| CANCEL |6| single zero byte | cancellation error5 |
| STATUS |7| single zero byte | unlocked1, observed min heap u32; probe appends timing and test flags |

Envelope: account index u8(0..4), origin byte length u8(1..127), printable ASCII origin, then typed body. There is no setup, PIN, seed export, mnemonic entry or raw-hash signing command. Origins are visibly labelled as host-reported. Device data cannot prove which browser initiated a request.

Every response payload begins with an error byte:0 success,1 format,2 policy,3 locked,4 busy,5 cancelled,6 timeout,7 sequence,8 crypto,9 storage. Failed responses have no signature/address data. Do not treat a partial response as success.

Permit body: chain LE32 at0, contract20 at4, owner20 at24, spender20 at44, value BE256 at64, nonce BE256 at96, deadline BE64 at128. Domain name/version come from the firmware registry. The owner must equal the selected derived address. Value must fit128bits and deadline must be after device UTC time. The normal firmware registry is empty until verified deployments exist.

## Review invariants

The review owns a copied digest and display fields. Pages contain at most80 printable characters and are displayed in complete20-character chunks. A page counts as visited only when drawn. Page changes reset hold/release state. Confirmation requires all pages, a released button, then two seconds continuously physically held OK. Software-origin input cancels review. Before response, disconnect, timeout and queued competing USB traffic are checked again.

The `probe` launch mode cannot load a stored wallet, prompt for a PIN or service signing requests. It uses public zero-entropy fixtures internally for crypto self-checks and exposes only result flags/timings/memory. It exits after30s of USB service, restores the previous interface and has no automatic approval path for user wallets.
