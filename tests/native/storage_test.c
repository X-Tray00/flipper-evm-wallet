#include "store.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef struct {
    uint8_t data[3][128];
    bool exists[3];
    int fail, step;
    bool corrupt;
} Disk;
static bool step(Disk *d) { return d->fail < 0 || d->step++ < d->fail; }
static bool exists(void *c, FwStorePath p) { return ((Disk *)c)->exists[p]; }
static bool read_(void *c, FwStorePath p, uint8_t out[128]) {
    Disk *d = c;
    if (!step(d) || !d->exists[p])
        return false;
    memcpy(out, d->data[p], 128);
    return true;
}
static bool write_(void *c, FwStorePath p, const uint8_t in[128]) {
    Disk *d = c;
    if (!step(d))
        return false;
    memcpy(d->data[p], in, 128);
    d->exists[p] = true;
    if (d->corrupt)
        d->data[p][7] ^= 1;
    return true;
}
static bool rename_(void *c, FwStorePath a, FwStorePath b) {
    Disk *d = c;
    if (!step(d) || !d->exists[a] || d->exists[b])
        return false;
    memcpy(d->data[b], d->data[a], 128);
    d->exists[b] = true;
    d->exists[a] = false;
    return true;
}
static bool remove_(void *c, FwStorePath p) {
    Disk *d = c;
    if (!step(d))
        return false;
    d->exists[p] = false;
    return true;
}
int main(void) {
    uint8_t old[128], next[128], out[128];
    memset(old, 0x11, 128);
    memset(next, 0x22, 128);
    for (int cut = 0; cut < 10; cut++) {
        Disk d = {.fail = cut};
        d.exists[0] = true;
        memcpy(d.data[0], old, 128);
        FwStoreOps fs = {&d, exists, read_, write_, rename_, remove_};
        fw_store_commit(&fs, next);
        d.fail = -1;
        assert(fw_store_read(&fs, out) == FW_OK);
        assert(!memcmp(out, old, 128) || !memcmp(out, next, 128));
    }
    Disk d = {.fail = -1, .corrupt = true};
    d.exists[0] = true;
    memcpy(d.data[0], old, 128);
    FwStoreOps fs = {&d, exists, read_, write_, rename_, remove_};
    assert(fw_store_commit(&fs, next) != FW_OK);
    assert(fw_store_read(&fs, out) == FW_OK && !memcmp(out, old, 128));
    d.corrupt = false;
    assert(fw_store_commit(&fs, next) == FW_OK);
    assert(!d.exists[2]);
    assert(fw_store_read(&fs, out) == FW_OK && !memcmp(out, next, 128));
    puts("Storage interruption and corrupt-write model passed (not a physical SD test)");
    return 0;
}
