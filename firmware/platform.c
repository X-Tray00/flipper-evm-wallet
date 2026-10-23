#include "platform.h"
#include "../core/store.h"
#include "../vendor/trezor-crypto/memzero.h"
#include <furi_hal.h>
#include <string.h>
#define DIRECTORY EXT_PATH("apps_data/flipper_evm_wallet")
#define RECORD DIRECTORY "/vault.bin"
#define TEMP DIRECTORY "/vault.pending"
#define BACKUP DIRECTORY "/vault.previous"
void random_buffer(uint8_t *out, size_t n) { furi_hal_random_fill_buf(out, n); }
void tc_fault_handler(const char *msg) {
    (void)msg;
    furi_crash("Wallet crypto fault");
}
bool wallet_wrap(bool encrypt, const uint8_t iv[16], const uint8_t in[64], uint8_t out[64],
                 void *ctx) {
    (void)ctx;
    const uint8_t slot = FURI_HAL_CRYPTO_ENCLAVE_UNIQUE_KEY_SLOT;
    if (!furi_hal_crypto_enclave_ensure_key(slot) || !furi_hal_crypto_enclave_load_key(slot, iv))
        return false;
    bool ok = encrypt ? furi_hal_crypto_encrypt(in, out, 64) : furi_hal_crypto_decrypt(in, out, 64);
    bool unloaded = furi_hal_crypto_enclave_unload_key(slot);
    return ok && unloaded;
}
bool wallet_record_exists(Storage *s) {
    return storage_file_exists(s, RECORD) || storage_file_exists(s, BACKUP) ||
           storage_file_exists(s, TEMP);
}
static bool read_path(Storage *s, const char *path, uint8_t out[128]) {
    File *f = storage_file_alloc(s);
    bool ok = storage_file_open(f, path, FSAM_READ, FSOM_OPEN_EXISTING) &&
              storage_file_size(f) == 128 && storage_file_read(f, out, 128) == 128;
    storage_file_close(f);
    storage_file_free(f);
    return ok;
}
static const char *paths[] = {RECORD, TEMP, BACKUP};
static bool fs_exists(void *ctx, FwStorePath path) { return storage_file_exists(ctx, paths[path]); }
static bool fs_read(void *ctx, FwStorePath path, uint8_t out[128]) {
    return read_path(ctx, paths[path], out);
}
static bool fs_write(void *ctx, FwStorePath path, const uint8_t data[128]) {
    File *f = storage_file_alloc(ctx);
    bool ok = storage_file_open(f, paths[path], FSAM_WRITE, FSOM_CREATE_ALWAYS) &&
              storage_file_write(f, data, 128) == 128 && storage_file_sync(f);
    storage_file_close(f);
    storage_file_free(f);
    return ok;
}
static bool fs_rename(void *ctx, FwStorePath a, FwStorePath b) {
    return storage_common_rename(ctx, paths[a], paths[b]) == FSE_OK;
}
static bool fs_remove(void *ctx, FwStorePath path) {
    return storage_common_remove(ctx, paths[path]) == FSE_OK;
}
static FwStoreOps ops(Storage *s) {
    return (FwStoreOps){s, fs_exists, fs_read, fs_write, fs_rename, fs_remove};
}
FwError wallet_record_read(Storage *s, uint8_t out[128]) {
    FwStoreOps fs = ops(s);
    return fw_store_read(&fs, out);
}
FwError wallet_record_write(Storage *s, const uint8_t data[128]) {
    storage_common_mkdir(s, DIRECTORY);
    FwStoreOps fs = ops(s);
    return fw_store_commit(&fs, data);
}
/* Newlib's explicit_bzero is not exported by Momentum 87.1. */
void explicit_bzero(void *p, size_t n) {
    volatile uint8_t *v = p;
    while (n--)
        *v++ = 0;
}
