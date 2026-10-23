#include "selftest.h"
#include "../vendor/trezor-crypto/memzero.h"
#include "platform.h"
#include "probe_vectors.h"
#include <furi.h>
#include <string.h>
/* Called only by the read-only `probe` launch mode. It cannot load the SD wallet,
 * accept a host hash, approve a request, or return a signature to the host. */
uint8_t wallet_selftest(uint32_t *min_heap) {
    FwKeys *keys = malloc(sizeof(FwKeys));
    memset(keys, 0, sizeof(*keys));
    keys->unlocked = true;
    FwReview *review = malloc(sizeof(FwReview));
    uint8_t flags = 0, addr[20], signature[65], record[128], entropy[32];
    uint32_t heap = memmgr_get_free_heap();
    if (heap < *min_heap)
        *min_heap = heap;
    bool good = true;
    for (uint8_t i = 0; i < 5; i++)
        if (fw_address(keys, i, addr) != FW_OK || !fw_equal(addr, probe_addresses[i], 20))
            good = false;
    if (good)
        flags |= 1;
    if (fw_vault_seal(keys->entropy, "12345678", wallet_wrap, NULL, record) == FW_OK &&
        fw_vault_open(record, "12345678", wallet_wrap, NULL, entropy) == FW_OK &&
        fw_equal(entropy, keys->entropy, 32)) {
        flags |= 2;
        record[127] ^= 1;
        if (fw_vault_open(record, "12345678", wallet_wrap, NULL, entropy) != FW_OK)
            flags |= 4;
    }
    if (fw_review_tx(probe_transaction, sizeof(probe_transaction), probe_addresses[0], review) ==
            FW_OK &&
        fw_sign(keys, review, signature) == FW_OK && fw_equal(signature, probe_signature, 65))
        flags |= 8;
    fw_lock(keys);
    memzero(review, sizeof(*review));
    memzero(signature, sizeof(signature));
    memzero(record, sizeof(record));
    memzero(entropy, sizeof(entropy));
    free(review);
    free(keys);
    return flags;
}
