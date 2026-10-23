#include "store.h"
#include "memzero.h"
FwError fw_store_read(const FwStoreOps *fs, uint8_t out[128]) {
    FwStorePath path = fs->exists(fs->ctx, FwStorePrimary) ? FwStorePrimary : FwStorePrevious;
    return fs->read(fs->ctx, path, out) ? FW_OK : FW_ERR_STORAGE;
}
FwError fw_store_commit(const FwStoreOps *fs, const uint8_t record[128]) {
    uint8_t check[128] = {0};
    FwError result = FW_ERR_STORAGE;
    if (!fs->write_sync(fs->ctx, FwStorePending, record) ||
        !fs->read(fs->ctx, FwStorePending, check) || !fw_equal(check, record, 128))
        goto done;
    if (fs->exists(fs->ctx, FwStorePrimary)) {
        if (fs->exists(fs->ctx, FwStorePrevious) && !fs->remove(fs->ctx, FwStorePrevious))
            goto done;
        if (!fs->rename(fs->ctx, FwStorePrimary, FwStorePrevious))
            goto done;
    }
    if (!fs->rename(fs->ctx, FwStorePending, FwStorePrimary))
        goto done;
    if (!fs->read(fs->ctx, FwStorePrimary, check) || !fw_equal(check, record, 128))
        goto done;
    /* New record is committed. Failure to remove a previous record is reported,
     * preserving the current record and never silently rolling back. */
    if (fs->exists(fs->ctx, FwStorePrevious) && !fs->remove(fs->ctx, FwStorePrevious))
        goto done;
    result = FW_OK;
done:
    memzero(check, sizeof(check));
    return result;
}
