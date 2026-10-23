#include "chacha20poly1305/rfc7539.h"
#include "memzero.h"
#include "pbkdf2.h"
#include "rand.h"
#include "wallet.h"
#include <string.h>
/* Header 64 bytes: magic/version/kdf/salt(16)/outerIV(16)/nonce(12)/reserved.
 * Wrapped 64 bytes: encrypted entropy + padding(48), tag(16).
 * Header is authenticated as AEAD associated data. Outer AES uses device
 * slot11.
 */
static const uint8_t magic[8] = {'F', 'E', 'W', 'V', 1, 0, 0, 0};
bool fw_pin_valid(const char *pin) {
    size_t n = strlen(pin);
    if (n < 8 || n > 12)
        return false;
    for (size_t i = 0; i < n; i++)
        if (pin[i] < '0' || pin[i] > '9')
            return false;
    return true;
}
static bool header_valid(const uint8_t *r) {
    return fw_equal(r, magic, 8) && r[8] == 0xa0 && r[9] == 0x86 && r[10] == 1 && r[11] == 0 &&
           r[56] == 0 && r[57] == 0 && r[58] == 0 && r[59] == 0 && r[60] == 0 && r[61] == 0 &&
           r[62] == 0 && r[63] == 0;
}
static void key_for(const char *pin, const uint8_t *salt, uint8_t key[32]) {
    pbkdf2_hmac_sha256((const uint8_t *)pin, (int)strlen(pin), salt, 16, FW_KDF_ROUNDS, key, 32);
}
FwError fw_vault_seal(const uint8_t entropy[32], const char *pin, FwWrap wrap, void *ctx,
                      uint8_t out[128]) {
    uint8_t key[32] = {0}, inner[64] = {0}, plain[48] = {0};
    chacha20poly1305_ctx aead;
    FwError e = FW_ERR_CRYPTO;
    if (!fw_pin_valid(pin) || !wrap)
        return FW_ERR_FORMAT;
    memset(out, 0, 128);
    memcpy(out, magic, 8);
    out[8] = 0xa0;
    out[9] = 0x86;
    out[10] = 1;
    random_buffer(out + 12, 44);
    key_for(pin, out + 12, key);
    rfc7539_init(&aead, key, out + 44);
    rfc7539_auth(&aead, out, 64);
    memcpy(plain, entropy, 32);
    random_buffer(plain + 32, 16);
    chacha20poly1305_encrypt(&aead, plain, inner, 48);
    rfc7539_finish(&aead, 64, 48, inner + 48);
    if (wrap(true, out + 28, inner, out + 64, ctx))
        e = FW_OK;
    else
        memzero(out, 128);
    memzero(key, sizeof(key));
    memzero(inner, sizeof(inner));
    memzero(plain, sizeof(plain));
    memzero(&aead, sizeof(aead));
    return e;
}
FwError fw_vault_open(const uint8_t record[128], const char *pin, FwWrap wrap, void *ctx,
                      uint8_t entropy[32]) {
    uint8_t key[32] = {0}, inner[64] = {0}, plain[48] = {0}, tag[16] = {0};
    chacha20poly1305_ctx aead = {0};
    FwError e = FW_ERR_CRYPTO;
    memzero(entropy, 32);
    if (!fw_pin_valid(pin) || !wrap || !header_valid(record))
        return FW_ERR_FORMAT;
    if (!wrap(false, record + 28, record + 64, inner, ctx))
        goto done;
    key_for(pin, record + 12, key);
    rfc7539_init(&aead, key, record + 44);
    rfc7539_auth(&aead, record, 64);
    chacha20poly1305_decrypt(&aead, inner, plain, 48);
    rfc7539_finish(&aead, 64, 48, tag);
    if (fw_equal(tag, inner + 48, 16)) {
        memcpy(entropy, plain, 32);
        e = FW_OK;
    }
done:
    memzero(key, sizeof(key));
    memzero(inner, sizeof(inner));
    memzero(plain, sizeof(plain));
    memzero(tag, sizeof(tag));
    memzero(&aead, sizeof(aead));
    return e;
}
