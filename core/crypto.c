#include "bip32.h"
#include "bip39.h"
#include "ecdsa.h"
#include "memzero.h"
#include "sha3.h"
#include "wallet.h"
#include <string.h>
void fw_lock(FwKeys *k) { memzero(k, sizeof(*k)); }
bool fw_equal(const uint8_t *a, const uint8_t *b, size_t n) {
    uint8_t v = 0;
    for (size_t i = 0; i < n; i++)
        v |= a[i] ^ b[i];
    return v == 0;
}
void fw_hex(const uint8_t *in, size_t n, char *out) {
    const char *h = "0123456789abcdef";
    for (size_t i = 0; i < n; i++) {
        out[2 * i] = h[in[i] >> 4];
        out[2 * i + 1] = h[in[i] & 15];
    }
    out[2 * n] = 0;
}
bool fw_unhex(const char *in, size_t n, uint8_t *out) {
    if (n % 2)
        return false;
    for (size_t i = 0; i < n; i++) {
        unsigned c = (unsigned char)in[i];
        unsigned v = c >= '0' && c <= '9'   ? c - '0'
                     : c >= 'a' && c <= 'f' ? c - 'a' + 10
                     : c >= 'A' && c <= 'F' ? c - 'A' + 10
                                            : 16;
        if (v > 15)
            return false;
        if (i % 2)
            out[i / 2] |= v;
        else
            out[i / 2] = v << 4;
    }
    return true;
}
void fw_hash(const uint8_t *in, size_t n, uint8_t out[32]) { keccak_256(in, n, out); }
bool fw_chain_allowed(uint32_t c) { return c == 11155111 || c == 84532 || c == 421614; }
const char *fw_chain_name(uint32_t c) {
    return c == 11155111 ? "Sepolia"
           : c == 84532  ? "Base Sepolia"
           : c == 421614 ? "Arbitrum Sepolia"
                         : "REJECTED";
}
static bool derive(const FwKeys *k, uint8_t index, HDNode *node) {
    uint8_t seed[64];
    bool ok = false;
    if (!k->unlocked || index >= FW_ACCOUNTS)
        return false;
    const char *m = mnemonic_from_data(k->entropy, 32);
    if (!m)
        return false;
    mnemonic_to_seed(m, "", seed, NULL);
    mnemonic_clear();
    if (hdnode_from_seed(seed, sizeof(seed), "secp256k1", node)) {
        const uint32_t path[] = {0x8000002c, 0x8000003c, 0x80000000, 0, index};
        ok = true;
        for (size_t i = 0; i < 5; i++)
            if (!hdnode_private_ckd(node, path[i])) {
                ok = false;
                break;
            }
    }
    memzero(seed, sizeof(seed));
    return ok;
}
FwError fw_address(const FwKeys *k, uint8_t account, uint8_t out[20]) {
    HDNode node = {0};
    FwError e = FW_ERR_LOCKED;
    if (derive(k, account, &node)) {
        e = hdnode_get_ethereum_pubkeyhash(&node, out) ? FW_OK : FW_ERR_CRYPTO;
    }
    memzero(&node, sizeof(node));
    return e;
}
FwError fw_sign(const FwKeys *k, const FwReview *r, uint8_t out[65]) {
    HDNode node = {0};
    FwError e = FW_ERR_LOCKED;
    uint8_t rec = 0;
    if (!fw_chain_allowed(r->chain) || !r->count || r->kind < 2 || r->kind > 4)
        return FW_ERR_POLICY;
    if (derive(k, r->account, &node)) {
        e = ecdsa_sign_digest(node.curve->params, node.private_key, r->digest, out, &rec, NULL) ==
                        0 &&
                    rec < 2
                ? FW_OK
                : FW_ERR_CRYPTO;
        if (e == FW_OK)
            out[64] = rec;
        else
            memzero(out, 65);
    }
    memzero(&node, sizeof(node));
    return e;
}
FwError fw_mnemonic_restore(const char *m, uint8_t out[32]) {
    uint8_t bits[33] = {0};
    size_t n = 0;
    if (mnemonic_check(m))
        n = mnemonic_to_bits(m, bits);
    if (n == 264)
        memcpy(out, bits, 32);
    else
        memzero(out, 32);
    memzero(bits, sizeof(bits));
    mnemonic_clear();
    return n == 264 ? FW_OK : FW_ERR_FORMAT;
}
bool fw_page(FwReview *r, const char *t, const char *s) {
    if (r->count >= FW_MAX_PAGES || strlen(t) >= sizeof(r->pages[0].title) || strlen(s) > 80)
        return false;
    FwPage *p = &r->pages[r->count++];
    strcpy(p->title, t);
    strcpy(p->text, s);
    return true;
}
