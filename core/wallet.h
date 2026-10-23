#ifndef FEW_WALLET_H
#define FEW_WALLET_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define FW_MAX_PAYLOAD 8192
#define FW_MAX_PAGES 40
#define FW_PAGE_TEXT 97
#define FW_REVIEW_MS 180000u
#define FW_LOCK_MS 120000u
#define FW_HOLD_MS 2000u
#define FW_ACCOUNTS 5
#define FW_VAULT_SIZE 128
#define FW_KDF_ROUNDS 100000u

typedef enum {
    FW_OK = 0,
    FW_ERR_FORMAT = 1,
    FW_ERR_POLICY = 2,
    FW_ERR_LOCKED = 3,
    FW_ERR_BUSY = 4,
    FW_ERR_CANCELLED = 5,
    FW_ERR_TIMEOUT = 6,
    FW_ERR_SEQUENCE = 7,
    FW_ERR_CRYPTO = 8,
    FW_ERR_STORAGE = 9
} FwError;
typedef struct {
    char title[24];
    char text[FW_PAGE_TEXT];
} FwPage;
typedef struct {
    FwPage pages[FW_MAX_PAGES];
    uint8_t count, account, kind;
    uint32_t chain;
    uint8_t digest[32];
} FwReview;
typedef struct {
    uint8_t entropy[32];
    bool unlocked;
    uint32_t last_activity;
} FwKeys;

typedef bool (*FwWrap)(bool encrypt, const uint8_t iv[16], const uint8_t in[64], uint8_t out[64],
                       void *ctx);
void fw_lock(FwKeys *k);
bool fw_pin_valid(const char *pin);
FwError fw_vault_seal(const uint8_t entropy[32], const char *pin, FwWrap wrap, void *ctx,
                      uint8_t out[FW_VAULT_SIZE]);
FwError fw_vault_open(const uint8_t record[FW_VAULT_SIZE], const char *pin, FwWrap wrap, void *ctx,
                      uint8_t entropy[32]);
FwError fw_address(const FwKeys *k, uint8_t account, uint8_t out[20]);
FwError fw_sign(const FwKeys *k, const FwReview *reviewed, uint8_t signature[65]);
FwError fw_mnemonic_restore(const char *words, uint8_t entropy[32]);
bool fw_chain_allowed(uint32_t chain);
const char *fw_chain_name(uint32_t chain);
void fw_hex(const uint8_t *in, size_t n, char *out);
bool fw_unhex(const char *in, size_t chars, uint8_t *out);
bool fw_equal(const uint8_t *a, const uint8_t *b, size_t n);
void fw_hash(const uint8_t *in, size_t n, uint8_t out[32]);
bool fw_page(FwReview *r, const char *title, const char *text);
FwError fw_review_tx(const uint8_t *data, size_t n, const uint8_t sender[20], FwReview *review);
FwError fw_review_siwe(const uint8_t *data, size_t n, const uint8_t sender[20], const char *origin,
                       uint64_t now, FwReview *review);
FwError fw_review_permit(const uint8_t *data, size_t n, const uint8_t sender[20], uint64_t now,
                         FwReview *review);
#endif
