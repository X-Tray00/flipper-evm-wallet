#pragma once
#include "wallet.h"
typedef struct {
    const uint8_t *p;
    size_t n, encoded;
    bool list;
} FwRlp;
bool fw_rlp(const uint8_t *data, size_t n, FwRlp *out);
bool fw_rlp_uint(const FwRlp *r, size_t max_bytes, uint64_t *out);
