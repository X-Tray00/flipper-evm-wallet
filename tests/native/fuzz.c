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
    puts("50000 deterministic parser mutations + approval boundaries passed");
    return 0;
}
