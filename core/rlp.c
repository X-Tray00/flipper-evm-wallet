#include "rlp.h"
bool fw_rlp(const uint8_t *d, size_t n, FwRlp *o) {
    if (!n)
        return false;
    size_t head = 1, len = 0;
    bool list = false;
    if (d[0] < 0x80) {
        o->p = d;
        o->n = 1;
        o->encoded = 1;
        o->list = false;
        return true;
    }
    if (d[0] <= 0xb7)
        len = d[0] - 0x80;
    else if (d[0] >= 0xc0 && d[0] <= 0xf7) {
        list = true;
        len = d[0] - 0xc0;
    } else {
        list = d[0] >= 0xf8;
        size_t bytes = d[0] - (list ? 0xf7 : 0xb7);
        if (bytes > 2 || bytes >= n || d[1] == 0)
            return false;
        head += bytes;
        for (size_t i = 1; i <= bytes; i++)
            len = (len << 8) | d[i];
        if (len < 56)
            return false;
    }
    if (head > n || len > n - head || (!list && len == 1 && d[head] < 0x80))
        return false;
    o->p = d + head;
    o->n = len;
    o->encoded = head + len;
    o->list = list;
    return true;
}
bool fw_rlp_uint(const FwRlp *r, size_t max, uint64_t *out) {
    if (r->list || r->n > max || r->n > 8 || (r->n && r->p[0] == 0))
        return false;
    uint64_t v = 0;
    for (size_t i = 0; i < r->n; i++)
        v = (v << 8) | r->p[i];
    *out = v;
    return true;
}
