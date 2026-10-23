#pragma once
#include "wallet.h"
#define FW_FRAME_SIZE 64
#define FW_FRAME_DATA 44
#define FW_WIRE_VERSION 1
/* Commands: HELLO(1), ADDRESS(2), TX(3), SIWE(4), PERMIT(5), CANCEL(6),
 * STATUS(7). Responses command|0x80; first payload byte = FwError. */
typedef struct {
    uint32_t session, request, last_request, started;
    uint16_t size, received, sequence;
    uint8_t command;
    bool active, complete;
    uint8_t data[FW_MAX_PAYLOAD];
} FwTransport;
typedef struct {
    FwReview review;
    uint32_t session, request, started, hold_start;
    uint64_t seen;
    bool active, released, holding;
} FwApproval;
uint32_t fw_read32(const uint8_t *p);
void fw_write32(uint8_t *p, uint32_t v);
void fw_transport_reset(FwTransport *t, uint32_t session);
void fw_transport_cancel(FwTransport *t);
FwError fw_frame_receive(FwTransport *t, const uint8_t frame[64], uint32_t now);
void fw_frame_encode(uint8_t out[64], uint8_t command, uint32_t session, uint32_t request,
                     uint16_t total, uint16_t sequence, const uint8_t *data);
void fw_approval_begin(FwApproval *a, const FwReview *r, uint32_t session, uint32_t request,
                       uint32_t now);
void fw_approval_cancel(FwApproval *a);
void fw_approval_page(FwApproval *a, uint8_t page);
bool fw_approval_button(FwApproval *a, bool hardware_source, bool gpio_down, uint32_t now,
                        uint32_t session, uint32_t request);
