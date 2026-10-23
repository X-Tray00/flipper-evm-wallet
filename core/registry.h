#pragma once
#include <stddef.h>
#include <stdint.h>
typedef struct {
    uint32_t chain;
    uint8_t address[20];
    const char *name;
    const char *version;
} FwPermitContract;
/* Populated only with reviewed, deployed test contracts. Empty means reject all
 * Permit. */
extern const FwPermitContract fw_permit_contracts[];
extern const size_t fw_permit_contract_count;
