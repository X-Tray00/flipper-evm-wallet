#include "protocol.h"
#include "registry.h"
#include "sha3.h"
#include "wallet.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static bool chunks(FwReview *r, const char *label, const char *s) {
    size_t n = strlen(s);
    do {
        char text[81] = {0};
        size_t take = n > 80 ? 80 : n;
        memcpy(text, s, take);
        if (!fw_page(r, label, text))
            return false;
        s += take;
        n -= take;
    } while (n);
    return true;
}
static bool decimal(const char *s, uint64_t *v) {
    *v = 0;
    if (!*s || (*s == '0' && s[1]))
        return false;
    for (; *s; s++) {
        if (*s < '0' || *s > '9' || *v > (UINT64_MAX - (unsigned)(*s - '0')) / 10)
            return false;
        *v = *v * 10 + (*s - '0');
    }
    return true;
}
static bool timestamp(const char *s, uint64_t *out) {
    size_t length = strlen(s);
    if (length < 20 || length > 30 || s[4] != '-' || s[7] != '-' || s[10] != 'T' || s[13] != ':' ||
        s[16] != ':' || s[length - 1] != 'Z')
        return false;
    if (length != 20) {
        if (length < 22 || s[19] != '.')
            return false;
        for (size_t i = 20; i < length - 1; i++)
            if (s[i] < '0' || s[i] > '9')
                return false;
    }
    int pos[] = {0, 1, 2, 3, 5, 6, 8, 9, 11, 12, 14, 15, 17, 18};
    for (size_t i = 0; i < 14; i++)
        if (s[pos[i]] < '0' || s[pos[i]] > '9')
            return false;
    int y = atoi(s), m = atoi(s + 5), d = atoi(s + 8), h = atoi(s + 11), mi = atoi(s + 14),
        se = atoi(s + 17);
    const int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    bool leap = (y % 4 == 0 && (y % 100 != 0 || y % 400 == 0));
    if (y < 2020 || y > 2100 || m < 1 || m > 12 || d < 1 || d > days[m - 1] + (m == 2 && leap) ||
        h > 23 || mi > 59 || se > 59)
        return false;
    uint64_t total = 0;
    for (int year = 1970; year < y; year++)
        total += 365 + (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
    for (int mon = 1; mon < m; mon++)
        total += days[mon - 1] + (mon == 2 && leap);
    total += d - 1;
    *out = ((total * 24 + h) * 60 + mi) * 60 + se;
    return true;
}
static bool authority(const char *origin, char out[128]) {
    const char *p = NULL;
    if (!strncmp(origin, "https://", 8))
        p = origin + 8;
    else if (!strncmp(origin, "http://localhost:", 17) || !strncmp(origin, "http://127.0.0.1:", 17))
        p = origin + 7;
    else
        return false;
    size_t n = strlen(p);
    if (!n || n >= 128)
        return false;
    for (size_t i = 0; i < n; i++)
        if (!isalnum((unsigned char)p[i]) && p[i] != '.' && p[i] != '-' && p[i] != ':')
            return false;
    memcpy(out, p, n + 1);
    return true;
}
FwError fw_review_siwe(const uint8_t *data, size_t n, const uint8_t sender[20], const char *origin,
                       uint64_t now, FwReview *r) {
    char buf[1537], host[128];
    char *lines[24];
    size_t count = 0;
    memset(r, 0, sizeof(*r));
    r->kind = 3;
    if (n < 100 || n > 1536 || !authority(origin, host))
        return FW_ERR_FORMAT;
    for (size_t i = 0; i < n; i++)
        if (data[i] != '\n' && (data[i] < 32 || data[i] > 126))
            return FW_ERR_FORMAT;
    memcpy(buf, data, n);
    buf[n] = 0;
    lines[count++] = buf;
    for (size_t i = 0; i < n; i++)
        if (buf[i] == '\n') {
            buf[i] = 0;
            if (count >= 24)
                return FW_ERR_FORMAT;
            lines[count++] = buf + i + 1;
        }
    char expected[192];
    snprintf(expected, sizeof(expected),
             "%s wants you to sign in with your Ethereum account:", host);
    if (count < 9 || strcmp(lines[0], expected) || strlen(lines[1]) != 42 ||
        strncmp(lines[1], "0x", 2) || lines[2][0])
        return FW_ERR_FORMAT;
    uint8_t addr[20];
    if (!fw_unhex(lines[1] + 2, 40, addr) || memcmp(addr, sender, 20))
        return FW_ERR_POLICY;
    char lower[41];
    uint8_t checksum[32];
    fw_hex(addr, 20, lower);
    fw_hash((uint8_t *)lower, 40, checksum);
    for (size_t j = 0; j < 40; j++)
        if (lower[j] >= 'a' && lower[j] <= 'f') {
            uint8_t nibble = (j % 2) ? checksum[j / 2] & 15 : checksum[j / 2] >> 4;
            char wanted = nibble >= 8 ? lower[j] - 32 : lower[j];
            if (lines[1][j + 2] != wanted)
                return FW_ERR_FORMAT;
        }
    size_t i = 3;
    const char *statement = "";
    if (!*lines[i]) {
        i++;
    } else {
        statement = lines[i++];
        if (strlen(statement) > 160 || i >= count || *lines[i++])
            return FW_ERR_FORMAT;
    }
    if (i + 5 > count)
        return FW_ERR_FORMAT;
    const char *uri = lines[i++];
    if (strncmp(uri, "URI: ", 5))
        return FW_ERR_FORMAT;
    uri += 5;
    size_t on = strlen(origin);
    if (strncmp(uri, origin, on) || (uri[on] && uri[on] != '/'))
        return FW_ERR_POLICY;
    if (strcmp(lines[i++], "Version: 1"))
        return FW_ERR_POLICY;
    uint64_t chain, issued, expires = 0;
    if (strncmp(lines[i], "Chain ID: ", 10) || !decimal(lines[i++] + 10, &chain) ||
        chain > UINT32_MAX || !fw_chain_allowed(chain))
        return FW_ERR_POLICY;
    const char *nonce = lines[i++];
    if (strncmp(nonce, "Nonce: ", 7) || strlen(nonce + 7) < 8 || strlen(nonce + 7) > 64)
        return FW_ERR_FORMAT;
    for (const char *p = nonce + 7; *p; p++)
        if (!isalnum((unsigned char)*p))
            return FW_ERR_FORMAT;
    if (strncmp(lines[i], "Issued At: ", 11) || !timestamp(lines[i++] + 11, &issued) ||
        issued > now + 300)
        return FW_ERR_POLICY;
    if (i < count && !strncmp(lines[i], "Expiration Time: ", 17)) {
        if (!timestamp(lines[i++] + 17, &expires) || expires <= now || expires <= issued)
            return FW_ERR_POLICY;
    }
    if (i < count && !strncmp(lines[i], "Not Before: ", 12)) {
        uint64_t before;
        if (!timestamp(lines[i++] + 12, &before) || before > now || before < issued ||
            (expires && before >= expires))
            return FW_ERR_POLICY;
    }
    /* Optional resources/request ID are rejected until a bounded renderer
     * supports them. */
    if (i != count)
        return FW_ERR_POLICY;
    r->chain = chain;
    fw_page(r, "Action", "Sign in with Ethereum");
    fw_page(r, "Test network", fw_chain_name(chain));
    fw_page(r, "Host origin", "Reported by computer. Not independently authenticated by Flipper.");
    if (!chunks(r, "Origin", origin) || !chunks(r, "Account", lines[1]) ||
        !chunks(r, "Statement", statement) || !chunks(r, "URI", uri) ||
        !chunks(r, "Nonce", nonce + 7))
        return FW_ERR_POLICY;
    for (size_t j = 0; j < count; j++)
        if (!strncmp(lines[j], "Issued At: ", 11) || !strncmp(lines[j], "Expiration Time: ", 17) ||
            !strncmp(lines[j], "Not Before: ", 12))
            if (!chunks(r, "Validity (UTC)", lines[j]))
                return FW_ERR_POLICY;
    if (!expires)
        fw_page(r, "Expiry", "No expiration in message");
    SHA3_CTX ctx;
    char prefix[64];
    int pn = snprintf(prefix, sizeof(prefix),
                      "\x19"
                      "Ethereum Signed Message:\n%zu",
                      n);
    sha3_256_Init(&ctx);
    sha3_Update(&ctx, (uint8_t *)prefix, pn);
    sha3_Update(&ctx, data, n);
    keccak_Final(&ctx, r->digest);
    return FW_OK;
}
static void hash_string(const char *s, uint8_t out[32]) {
    fw_hash((const uint8_t *)s, strlen(s), out);
}
FwError fw_review_permit(const uint8_t *d, size_t n, const uint8_t sender[20], uint64_t now,
                         FwReview *r) {
    memset(r, 0, sizeof(*r));
    r->kind = 4;
    if (n != 136)
        return FW_ERR_FORMAT;
    uint32_t chain = fw_read32(d);
    if (!fw_chain_allowed(chain) || memcmp(d + 24, sender, 20))
        return FW_ERR_POLICY;
    const FwPermitContract *c = NULL;
    for (size_t i = 0; i < fw_permit_contract_count; i++)
        if (fw_permit_contracts[i].chain == chain &&
            !memcmp(fw_permit_contracts[i].address, d + 4, 20)) {
            c = &fw_permit_contracts[i];
            break;
        }
    if (!c)
        return FW_ERR_POLICY;
    for (size_t i = 64; i < 80; i++)
        if (d[i])
            return FW_ERR_POLICY;
    uint64_t deadline = 0;
    for (size_t i = 128; i < 136; i++)
        deadline = deadline << 8 | d[i];
    if (deadline <= now)
        return FW_ERR_POLICY;
    uint8_t domain[160] = {0}, permit[192] = {0}, digest[66] = {0x19, 0x01};
    hash_string("EIP712Domain(string name,string version,uint256 chainId,address "
                "verifyingContract)",
                domain);
    hash_string(c->name, domain + 32);
    hash_string(c->version, domain + 64);
    for (size_t i = 0; i < 4; i++) {
        domain[127 - i] = chain >> (8 * i);
    }
    memcpy(domain + 140, d + 4, 20);
    hash_string("Permit(address owner,address spender,uint256 value,uint256 "
                "nonce,uint256 deadline)",
                permit);
    memcpy(permit + 44, sender, 20);
    memcpy(permit + 76, d + 44, 20);
    memcpy(permit + 96, d + 64, 32);
    memcpy(permit + 128, d + 96, 32);
    memcpy(permit + 184, d + 128, 8);
    fw_hash(domain, sizeof(domain), digest + 2);
    fw_hash(permit, sizeof(permit), digest + 34);
    fw_hash(digest, sizeof(digest), r->digest);
    r->chain = chain;
    fw_page(r, "Action", "EIP2612 Permit");
    fw_page(r, "Test network", fw_chain_name(chain));
    fw_page(r, "Contract name", c->name);
    char text[67];
    fw_hex(d + 4, 20, text);
    fw_page(r, "Contract (hex)", text);
    fw_hex(sender, 20, text);
    fw_page(r, "Owner (hex)", text);
    fw_hex(d + 44, 20, text);
    fw_page(r, "Spender (hex)", text);
    fw_hex(d + 64, 32, text);
    fw_page(r, "Raw units (hex)", text);
    fw_hex(d + 96, 32, text);
    fw_page(r, "Nonce (hex)", text);
    snprintf(text, sizeof(text), "%llu", (unsigned long long)deadline);
    fw_page(r, "Deadline (Unix UTC)", text);
    return FW_OK;
}
