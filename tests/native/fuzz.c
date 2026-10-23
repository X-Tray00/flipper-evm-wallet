#include "../../firmware/probe_vectors.h"
#include "protocol.h"
#include "wallet.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint32_t seed = 0x582c01;
static uint32_t next(void) {
    seed ^= seed << 13;
    seed ^= seed >> 17;
    seed ^= seed << 5;
    return seed;
}
int main(void) {
    FwTransport t;
    FwReview r;
    uint8_t data[8192], sender[20] = {0};
    fw_transport_reset(&t, 7);
    for (unsigned trial = 0; trial < 50000; trial++) {
        size_t n = next() % sizeof(data);
        for (size_t i = 0; i < n; i++)
            data[i] = next();
        fw_review_tx(data, n, sender, &r);
        fw_review_siwe(data, n, sender, "https://example.com", 1792753200, &r);
        fw_review_permit(data, n, sender, 1792753200, &r);
        if (n >= 64)
            fw_frame_receive(&t, data, next());
    }
    /* Mutate valid seeds to reach deep fields, not only format prefixes. */
    uint8_t seeds[4][1536] = {{0}};
    size_t sizes[4] = {sizeof(probe_transaction), 136, 0, 64};
    memcpy(seeds[0], probe_transaction, sizes[0]);
    fw_write32(seeds[1], 11155111);
    memset(seeds[1] + 4, 0x11, 20);
    memcpy(seeds[1] + 24, probe_addresses[0], 20);
    memset(seeds[1] + 44, 0x22, 20);
    seeds[1][95] = 1;
    memset(seeds[1] + 128, 0x77, 8);
    char addr[41];
    uint8_t checksum[32];
    fw_hex(probe_addresses[0], 20, addr);
    fw_hash((uint8_t *)addr, 40, checksum);
    for (size_t i = 0; i < 40; i++) {
        uint8_t nibble = i % 2 ? checksum[i / 2] & 15 : checksum[i / 2] >> 4;
        if (addr[i] >= 'a' && addr[i] <= 'f' && nibble >= 8)
            addr[i] -= 32;
    }
    sizes[2] =
        snprintf((char *)seeds[2], sizeof(seeds[2]),
                 "example.com wants you to sign in with your Ethereum account:\n0x%s\n\nTest\n\n"
                 "URI: https://example.com\nVersion: 1\nChain ID: 11155111\nNonce: abcdef1234\n"
                 "Issued At: 2026-10-23T10:00:00Z",
                 addr);
    uint8_t status = 0;
    fw_frame_encode(seeds[3], 7, 7, 1, 1, 0, &status);
    assert(fw_review_tx(seeds[0], sizes[0], probe_addresses[0], &r) == FW_OK);
    assert(fw_review_permit(seeds[1], sizes[1], probe_addresses[0], 1792753200, &r) == FW_OK);
    assert(fw_review_siwe(seeds[2], sizes[2], probe_addresses[0], "https://example.com", 1792753200,
                          &r) == FW_OK);
    for (unsigned trial = 0; trial < 20000; trial++) {
        unsigned which = trial % 4;
        size_t n = sizes[which];
        memcpy(data, seeds[which], n);
        if (trial % 3 == 0)
            n = next() % (n + 1);
        else
            for (unsigned j = 0; j < 1 + trial % 4; j++)
                data[next() % n] ^= 1u << (next() % 8);
        if (which == 0)
            fw_review_tx(data, n, probe_addresses[0], &r);
        else if (which == 1)
            fw_review_permit(data, n, probe_addresses[0], 1792753200, &r);
        else if (which == 2)
            fw_review_siwe(data, n, probe_addresses[0], "https://example.com", 1792753200, &r);
        else if (n == 64) {
            fw_transport_reset(&t, 7);
            fw_frame_receive(&t, data, next());
        }
        assert(r.count <= FW_MAX_PAGES);
    }
    /* Exercise each physical-confirmation boundary with deterministic timing. */
    for (unsigned pages = 1; pages <= 40; pages++) {
        memset(&r, 0, sizeof(r));
        r.count = pages;
        FwApproval a;
        fw_approval_begin(&a, &r, 5, 6, 0);
        for (unsigned p = 0; p < pages; p++) {
            fw_approval_page(&a, p);
            assert(!fw_approval_button(&a, true, true, 3000, 5, 6));
        }
        assert(!fw_approval_button(&a, true, false, 3001, 5, 6));
        assert(!fw_approval_button(&a, true, true, 3002, 5, 6));
        assert(!fw_approval_button(&a, true, true, 5001, 5, 6));
        assert(fw_approval_button(&a, true, true, 5002, 5, 6));
        assert(!fw_approval_button(&a, true, true, 9000, 5, 6));
        fw_approval_begin(&a, &r, 5, 6, 0);
        assert(!fw_approval_button(&a, true, false, 180000, 5, 6));
        assert(!a.active);
        fw_approval_begin(&a, &r, 5, 6, 0);
        assert(!fw_approval_button(&a, true, false, 1, 5, 7));
        assert(!a.active);
    }
    puts("70000 random and structured parser mutations + approval boundaries passed");
    return 0;
}
