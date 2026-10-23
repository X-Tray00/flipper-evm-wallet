/* Host-only differential harness. Every key here uses PUBLIC zero entropy.
 * This program is never included in a FAP or extension. */
#include "wallet.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static bool identity(bool e, const uint8_t iv[16], const uint8_t in[64], uint8_t out[64],
                     void *ctx) {
    (void)e;
    (void)iv;
    (void)ctx;
    memcpy(out, in, 64);
    return true;
}
int main(int argc, char **argv) {
    if (argc < 2)
        return 2;
    FwKeys keys = {.unlocked = true};
    uint8_t addr[20] = {0};
    FwReview r = {0};
    uint8_t data[8192], sig[65];
    char h[257];
    if (!strcmp(argv[1], "vault")) {
        uint8_t record[128];
        if (fw_vault_seal(keys.entropy, "12345678", identity, NULL, record) != FW_OK)
            return 3;
        fw_hex(record, 128, h);
        puts(h);
        return 0;
    }
    if (!strcmp(argv[1], "address")) {
        if (argc != 3)
            return 2;
        for (unsigned i = 0; i < 32; i++)
            keys.entropy[i] = 0;
        FwError e = fw_address(&keys, atoi(argv[2]), addr);
        fw_hex(addr, 20, h);
        printf("{\"error\":%d,\"address\":\"0x%s\"}\n", e, h);
        return 0;
    }
    if (argc < 3 || strlen(argv[2]) > 16384 || !fw_unhex(argv[2], strlen(argv[2]), data))
        return 2;
    if (fw_address(&keys, 0, addr) != FW_OK)
        return 3;
    size_t n = strlen(argv[2]) / 2;
    FwError e = FW_ERR_FORMAT;
    if (!strcmp(argv[1], "tx"))
        e = fw_review_tx(data, n, addr, &r);
    if (!strcmp(argv[1], "siwe") && argc == 5)
        e = fw_review_siwe(data, n, addr, argv[3], strtoull(argv[4], NULL, 10), &r);
    if (!strcmp(argv[1], "permit") && argc == 4)
        e = fw_review_permit(data, n, addr, strtoull(argv[3], NULL, 10), &r);
    printf("{\"error\":%d", e);
    if (e == FW_OK) {
        fw_hex(r.digest, 32, h);
        printf(",\"hash\":\"0x%s\"", h);
        e = fw_sign(&keys, &r, sig);
        if (e != FW_OK)
            return 3;
        fw_hex(sig, 65, h);
        printf(",\"signature\":\"0x%s\",\"pages\":[", h);
        for (size_t i = 0; i < r.count; i++) {
            if (i)
                printf(",");
            printf("{\"title\":\"%s\",\"textHex\":\"", r.pages[i].title);
            char text[193];
            fw_hex((uint8_t *)r.pages[i].text, strlen(r.pages[i].text), text);
            printf("%s\"}", text);
        }
        printf("]");
    }
    puts("}");
    return 0;
}
