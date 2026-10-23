#pragma once
#include "wallet.h"
typedef enum { FwStorePrimary, FwStorePending, FwStorePrevious } FwStorePath;
typedef struct {
    void *ctx;
    bool (*exists)(void *, FwStorePath);
    bool (*read)(void *, FwStorePath, uint8_t[128]);
    bool (*write_sync)(void *, FwStorePath, const uint8_t[128]);
    bool (*rename)(void *, FwStorePath, FwStorePath);
    bool (*remove)(void *, FwStorePath);
} FwStoreOps;
FwError fw_store_read(const FwStoreOps *fs, uint8_t out[128]);
FwError fw_store_commit(const FwStoreOps *fs, const uint8_t record[128]);
