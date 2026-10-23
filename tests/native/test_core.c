#include "bip32.h"
#include "bip39.h"
#include "pbkdf2.h"
#include "protocol.h"
#include "wallet.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static bool wrap(bool enc, const uint8_t iv[16], const uint8_t in[64], uint8_t out[64], void *ctx) {
    (void)enc;
    (void)iv;
    (void)ctx;
    memcpy(out, in, 64);
    return true;
}
int main(void) {
    FwKeys k = {.unlocked = true};
    uint8_t out[20];
    assert(fw_address(&k, 0, out) == FW_OK);
    char s[41];
    fw_hex(out, 20, s);
    printf("BIP44 zero entropy address: %s\n", s);
    const char *m = mnemonic_from_data(k.entropy, 32);
    assert(m && strstr(m, "art"));
    uint8_t entropy[32];
    assert(fw_mnemonic_restore(m, entropy) == FW_OK);
    assert(fw_equal(entropy, k.entropy, 32));
    uint8_t record[128];
    assert(fw_vault_seal(k.entropy, "12345678", wrap, NULL, record) == FW_OK);
    assert(fw_vault_open(record, "12345678", wrap, NULL, entropy) == FW_OK);
    assert(fw_vault_open(record, "87654321", wrap, NULL, entropy) != FW_OK);
    const size_t positions[] = {0, 4, 8, 12, 28, 44, 56, 64, 80, 96, 112, 127};
    for (size_t i = 0; i < sizeof(positions) / sizeof(positions[0]); i++) {
        record[positions[i]] ^= 1;
        memset(entropy, 0xaa, 32);
        assert(fw_vault_open(record, "12345678", wrap, NULL, entropy) != FW_OK);
        for (size_t j = 0; j < 32; j++)
            assert(entropy[j] == 0);
        record[positions[i]] ^= 1;
    }
    assert(!fw_pin_valid("1234567") && !fw_pin_valid("1234567890123") && !fw_pin_valid("password"));
    uint8_t seed[16];
    for (size_t i = 0; i < 16; i++)
        seed[i] = i;
    HDNode root = {0};
    assert(hdnode_from_seed(seed, 16, "secp256k1", &root));
    char vector[65];
    fw_hex(root.chain_code, 32, vector);
    assert(!strcmp(vector, "873dff81c02f525623fd1fe5167eac3a55a049de3d314bb42ee227ffed37d508"));
    uint8_t derived[32];
    pbkdf2_hmac_sha256((const uint8_t *)"password", 8, (const uint8_t *)"salt", 4, 1, derived, 32);
    fw_hex(derived, 32, vector);
    assert(!strcmp(vector, "120fb6cffcf8b32c43e7225256c4f837a86548c92ccc35480805987cb70be17b"));
    assert(fw_chain_allowed(84532) && !fw_chain_allowed(1));
    FwReview r = {.count = 2, .kind = 2, .chain = 11155111};
    FwApproval a;
    fw_approval_begin(&a, &r, 1, 1, 0);
    fw_approval_page(&a, 0);
    fw_approval_page(&a, 1);
    assert(!fw_approval_button(&a, true, true, 3000, 1, 1));
    assert(!fw_approval_button(&a, true, false, 3001, 1, 1));
    assert(!fw_approval_button(&a, true, true, 3002, 1, 1));
    assert(fw_approval_button(&a, true, true, 5002, 1, 1));
    assert(!fw_approval_button(&a, true, true, 8000, 1, 1));
    fw_approval_begin(&a, &r, 1, 2, 0);
    fw_approval_page(&a, 0);
    fw_approval_page(&a, 1);
    assert(!fw_approval_button(&a, false, false, 10, 1, 2));
    assert(!a.active);
    FwTransport t;
    fw_transport_reset(&t, 9);
    uint8_t frame[64], msg[45] = {0};
    fw_frame_encode(frame, 3, 9, 1, 45, 0, msg);
    assert(fw_frame_receive(&t, frame, 0) == FW_OK && !t.complete);
    fw_frame_encode(frame, 3, 9, 1, 45, 1, msg);
    assert(fw_frame_receive(&t, frame, 1) == FW_OK && t.complete);
    assert(fw_frame_receive(&t, frame, 2) != FW_OK && !t.active);
    fw_transport_reset(&t, 9);
    fw_frame_encode(frame, 1, 0, 1, 0, 0, msg);
    frame[63] = 1;
    assert(fw_frame_receive(&t, frame, 3) == FW_ERR_SEQUENCE);
    fw_frame_encode(frame, 6, 9, 2, 1, 0, msg);
    frame[63] = 1;
    assert(fw_frame_receive(&t, frame, 4) == FW_ERR_SEQUENCE);
    frame[63] = 0;
    assert(fw_frame_receive(&t, frame, 5) == FW_ERR_CANCELLED);
    assert(fw_frame_receive(&t, frame, 6) == FW_ERR_SEQUENCE);
    /* Fresh deadlines after long work and wraparound use current ticks. */
    fw_approval_begin(&a, &r, 9, 3, 0xfffffff0u);
    fw_approval_page(&a, 0);
    fw_approval_page(&a, 1);
    assert(!fw_approval_button(&a, true, false, 0xfffffff1u, 9, 3));
    assert(!fw_approval_button(&a, true, true, 0xfffffff2u, 9, 3));
    assert(fw_approval_button(&a, true, true, 0x7c2u, 9, 3));
    fw_lock(&k);
    assert(!k.unlocked);
    puts("core tests passed");
    return 0;
}
