#include "protocol.h"
#include "memzero.h"
#include <string.h>
uint32_t fw_read32(const uint8_t *p) {
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
void fw_write32(uint8_t *p, uint32_t v) {
    for (size_t i = 0; i < 4; i++)
        p[i] = v >> (8 * i);
}
static uint16_t u16(const uint8_t *p) { return p[0] | (uint16_t)p[1] << 8; }
void fw_transport_reset(FwTransport *t, uint32_t s) {
    memzero(t, sizeof(*t));
    t->session = s;
}
void fw_transport_cancel(FwTransport *t) {
    uint32_t s = t->session, last = t->last_request;
    memzero(t, sizeof(*t));
    t->session = s;
    t->last_request = last;
}
FwError fw_frame_receive(FwTransport *t, const uint8_t f[64], uint32_t now) {
    if (f[0] != 'F' || f[1] != 'W' || f[2] != 1 || f[3] < 1 || f[3] > 7 || f[16] || f[17] ||
        f[18] || f[19])
        goto invalid;
    uint32_t s = fw_read32(f + 4), id = fw_read32(f + 8);
    uint16_t total = u16(f + 12), seq = u16(f + 14);
    if (f[3] == 1) { /* HELLO is a one-frame, empty reset; caller allocates fresh
                        random session. */
        if (s || !id || total || seq)
            goto invalid;
        for (size_t i = 20; i < 64; i++)
            if (f[i])
                goto invalid;
        fw_transport_cancel(t);
        t->request = id;
        t->command = 1;
        t->complete = true;
        return FW_OK;
    }
    if (!t->session || s != t->session || !id || !total || total > FW_MAX_PAYLOAD)
        goto invalid;
    if (t->active && (uint32_t)(now - t->started) >= FW_REVIEW_MS) {
        fw_transport_cancel(t);
        return FW_ERR_TIMEOUT;
    }
    if (f[3] == 6) {
        if (total != 1 || seq || f[20] != 0 || id <= t->last_request)
            goto invalid;
        for (size_t i = 21; i < 64; i++)
            if (f[i])
                goto invalid;
        fw_transport_cancel(t);
        t->last_request = id;
        return FW_ERR_CANCELLED;
    }
    if (!t->active) {
        if (seq || id <= t->last_request)
            goto invalid;
        t->active = true;
        t->request = id;
        t->last_request = id;
        t->size = total;
        t->command = f[3];
        t->started = now;
    }
    if (t->complete || t->request != id || t->command != f[3] || t->size != total ||
        t->sequence != seq)
        goto invalid;
    size_t take = t->size - t->received;
    if (take > FW_FRAME_DATA)
        take = FW_FRAME_DATA;
    for (size_t i = 20 + take; i < 64; i++)
        if (f[i])
            goto invalid;
    memcpy(t->data + t->received, f + 20, take);
    t->received += take;
    t->sequence++;
    t->complete = t->received == t->size;
    return FW_OK;
invalid:
    fw_transport_cancel(t);
    return FW_ERR_SEQUENCE;
}
void fw_frame_encode(uint8_t out[64], uint8_t cmd, uint32_t s, uint32_t id, uint16_t total,
                     uint16_t seq, const uint8_t *data) {
    memset(out, 0, 64);
    out[0] = 'F';
    out[1] = 'W';
    out[2] = 1;
    out[3] = cmd;
    fw_write32(out + 4, s);
    fw_write32(out + 8, id);
    out[12] = total;
    out[13] = total >> 8;
    out[14] = seq;
    out[15] = seq >> 8;
    size_t off = (size_t)seq * FW_FRAME_DATA;
    if (off < total) {
        size_t n = total - off;
        if (n > FW_FRAME_DATA)
            n = FW_FRAME_DATA;
        memcpy(out + 20, data + off, n);
    }
}
void fw_approval_cancel(FwApproval *a) { memzero(a, sizeof(*a)); }
void fw_approval_begin(FwApproval *a, const FwReview *r, uint32_t s, uint32_t id, uint32_t now) {
    fw_approval_cancel(a);
    a->review = *r;
    a->session = s;
    a->request = id;
    a->started = now;
    a->active = true;
}
void fw_approval_page(FwApproval *a, uint8_t p) {
    if (a->active && p < a->review.count) {
        a->seen |= (uint64_t)1 << p;
        a->holding = false;
        a->released = false;
    }
}
bool fw_approval_button(FwApproval *a, bool hardware, bool down, uint32_t now, uint32_t s,
                        uint32_t id) {
    if (!a->active)
        return false;
    if (!hardware || s != a->session || id != a->request ||
        (uint32_t)(now - a->started) >= FW_REVIEW_MS) {
        fw_approval_cancel(a);
        return false;
    }
    if (!down) {
        a->released = true;
        a->holding = false;
        return false;
    }
    uint64_t all = ((uint64_t)1 << a->review.count) - 1;
    if (!a->released || !a->review.count || a->seen != all)
        return false;
    if (!a->holding) {
        a->holding = true;
        a->hold_start = now;
        return false;
    }
    if ((uint32_t)(now - a->hold_start) < FW_HOLD_MS)
        return false;
    a->active = false;
    a->holding = false;
    return true;
}
