#ifndef TGAASSET_H_
#define TGAASSET_H_

#include <taa/gl.h>
#include <taa/asset.h>

typedef struct tgaasset_s tgaasset;
typedef struct tgaasset_mgr_s tgaasset_mgr;

tgaasset* tgaasset_acquire(
    tgaasset_mgr* mgr,
    const taa_asset_key key);

void tgaasset_create_mgr(
    taa_asset_storage* storage,
    taa_workqueue* wq,
    uint32_t totalcapacity,
    uint32_t cachesize,
    tgaasset_mgr** mgr_out);

void tgaasset_destroy_mgr(
    tgaasset_mgr* mgr);

taa_asset_state tgaasset_poll(
    tgaasset_mgr* mgr,
    tgaasset* asset,
    taa_texture2d* texture_out);

void tgaasset_register_storage(
    tgaasset_mgr* mgr,
    taa_asset_group* group);

void tgaasset_release(
    tgaasset* asset);

#endif // TGAASSET_H_
