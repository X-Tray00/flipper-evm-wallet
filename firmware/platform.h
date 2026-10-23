#pragma once
#include "../core/wallet.h"
#include <storage/storage.h>
bool wallet_wrap(bool encrypt, const uint8_t iv[16], const uint8_t in[64], uint8_t out[64],
                 void *ctx);
bool wallet_record_exists(Storage *storage);
FwError wallet_record_read(Storage *storage, uint8_t out[128]);
FwError wallet_record_write(Storage *storage, const uint8_t data[128]);
