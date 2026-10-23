#include "rlp.h"
#include "wallet.h"
#include <stdio.h>
#include <string.h>
static bool zero(const uint8_t *p, size_t n) {
    uint8_t v = 0;
    for (size_t i = 0; i < n; i++)
        v |= p[i];
    return !v;
}
static bool address_word(const uint8_t *p) { return zero(p, 12); }
static bool page_address(FwReview *r, const char *label, const uint8_t *p) {
    char s[43] = "0x";
    fw_hex(p, 20, s + 2);
    return fw_page(r, label, s);
}
static bool page_uint(FwReview *r, const char *label, const uint8_t *p, size_t n) {
    uint8_t work[32] = {0};
    char dec[79] = {0};
    size_t used = 0;
    if (n > 32)
        return false;
    memcpy(work + 32 - n, p, n);
    do {
        unsigned rem = 0;
        for (size_t i = 0; i < 32; i++) {
            unsigned v = rem * 256 + work[i];
            work[i] = v / 10;
            rem = v % 10;
        }
        dec[used++] = '0' + rem;
    } while (!zero(work, 32));
    for (size_t i = 0; i < used / 2; i++) {
        char c = dec[i];
        dec[i] = dec[used - i - 1];
        dec[used - i - 1] = c;
    }
    return fw_page(r, label, dec);
}
static bool approve_bounded(const uint8_t *p) { /* Values >= 2^128 are excluded, including
                                                   all conventional unlimited sentinels. */
    return zero(p, 16);
}
FwError fw_review_tx(const uint8_t *data, size_t n, const uint8_t sender[20], FwReview *r) {
    FwRlp list, f[9];
    size_t pos = 0;
    uint64_t chain, nonce, priority, fee, gas;
    memset(r, 0, sizeof(*r));
    r->kind = 2;
    if (n < 2 || n > FW_MAX_PAYLOAD || data[0] != 2 || !fw_rlp(data + 1, n - 1, &list) ||
        !list.list || list.encoded != n - 1)
        return FW_ERR_FORMAT;
    for (size_t i = 0; i < 9; i++) {
        if (!fw_rlp(list.p + pos, list.n - pos, &f[i]))
            return FW_ERR_FORMAT;
        pos += f[i].encoded;
    }
    if (pos != list.n || !fw_rlp_uint(&f[0], 4, &chain) || !fw_chain_allowed(chain))
        return FW_ERR_POLICY;
    if (!fw_rlp_uint(&f[1], 8, &nonce) || !fw_rlp_uint(&f[2], 8, &priority) ||
        !fw_rlp_uint(&f[3], 8, &fee) || !fw_rlp_uint(&f[4], 4, &gas) || priority > fee ||
        gas < 21000)
        return FW_ERR_FORMAT;
    if (f[5].list || f[5].n != 20 || f[6].list || f[6].n > 32 || (f[6].n && f[6].p[0] == 0) ||
        f[7].list || !f[8].list || f[8].n)
        return FW_ERR_POLICY;
    /* Base EIP-1559 does not cap its separate L1 fee. Until the product
     * explicitly accepts estimates, refuse instead of claiming a total cap. */
    if (chain == 84532)
        return FW_ERR_POLICY;
    r->chain = chain;
    fw_page(r, "Test network", fw_chain_name(chain));
    page_address(r, "From", sender);
    const uint8_t *d = f[7].p;
    size_t dn = f[7].n;
    if (!dn) {
        fw_page(r, "Action", "Send native ETH");
        page_address(r, "To", f[5].p);
        page_uint(r, "Amount (wei)", f[6].p, f[6].n);
    } else {
        if (f[6].n)
            return FW_ERR_POLICY;
        fw_page(r, "Contract call",
                "Only this call is checked. Contract behavior is NOT guaranteed.");
        page_address(r, "Contract", f[5].p);
        if (dn == 68 && (!memcmp(d, "\xa9\x05\x9c\xbb", 4) || !memcmp(d, "\x09\x5e\xa7\xb3", 4))) {
            if (!address_word(d + 4))
                return FW_ERR_FORMAT;
            bool approve = d[0] == 0x09;
            if (approve && !approve_bounded(d + 36))
                return FW_ERR_POLICY;
            fw_page(r, "Action",
                    approve ? (zero(d + 36, 32) ? "Revoke ERC20 allowance" : "Set ERC20 allowance")
                            : "ERC20 transfer");
            page_address(r, approve ? "Spender" : "Recipient", d + 16);
            page_uint(r, "Raw token units", d + 36, 32);
            fw_page(r, "Token metadata",
                    "Name and decimals unverified. Verify contract and raw units.");
        } else if ((dn == 100 &&
                    (!memcmp(d, "\x23\xb8\x72\xdd", 4) || !memcmp(d, "\x42\x84\x2e\x0e", 4))) ||
                   (dn == 164 && !memcmp(d, "\xb8\x8d\x4f\xde", 4))) {
            if (!address_word(d + 4) || !address_word(d + 36) || memcmp(d + 16, sender, 20))
                return FW_ERR_POLICY;
            if (dn == 164 && (!zero(d + 100, 31) || d[131] != 128 || !zero(d + 132, 32)))
                return FW_ERR_FORMAT;
            fw_page(r, "Action", d[0] == 0x23 ? "NFT transferFrom" : "NFT safeTransferFrom");
            page_address(r, "Recipient", d + 48);
            page_uint(r, "Token ID", d + 68, 32);
            fw_page(r, "Interpretation",
                    "ERC721 call. A contract using the same selector may behave "
                    "differently.");
        } else if (dn == 196 && !memcmp(d, "\xf2\x42\x43\x2a", 4)) {
            if (!address_word(d + 4) || !address_word(d + 36) || memcmp(d + 16, sender, 20) ||
                !zero(d + 132, 31) || d[163] != 160 || !zero(d + 164, 32))
                return FW_ERR_FORMAT;
            fw_page(r, "Action", "ERC1155 single transfer");
            page_address(r, "Recipient", d + 48);
            page_uint(r, "Token ID", d + 68, 32);
            page_uint(r, "Quantity", d + 100, 32);
        } else
            return FW_ERR_POLICY;
    }
    /* Exact 96-bit maximum: uint32 gas * uint64 maxFeePerGas. */
    uint8_t product[12] = {0};
    uint64_t carry = 0;
    for (int i = 7; i >= 0; i--) {
        carry += ((fee >> (8 * (7 - i))) & 255) * gas;
        product[i + 4] = (uint8_t)carry;
        carry >>= 8;
    }
    for (int i = 3; i >= 0; i--) {
        product[i] = (uint8_t)carry;
        carry >>= 8;
    }
    page_uint(r, "Max total fee (wei)", product, 12);
    if (chain == 421614)
        fw_page(r, "Arbitrum fee",
                "Gas limit includes execution and L1 posting gas. Both share this fee cap.");
    char s[50];
    snprintf(s, sizeof(s), "%llu", (unsigned long long)nonce);
    fw_page(r, "Nonce", s);
    snprintf(s, sizeof(s), "%llu", (unsigned long long)gas);
    fw_page(r, "Gas limit", s);
    fw_hash(data, n, r->digest);
    return FW_OK;
}
