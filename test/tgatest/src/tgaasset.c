#include "tgaasset.h"
#include "tga.h"
#include <taa/assetcache.h>
#include <taa/assetmap.h>
#include <taa/log.h>
#include <taa/spinlock.h>
#include <assert.h>
#include <stdlib.h>

//****************************************************************************

struct tgaasset_s
{
    taa_texture2d texture;
    tgaasset_mgr* mgr;
    int32_t cacheentry;
    taa_asset_map_value* mapval;
    taa_asset_state state;
    int32_t refcount;
};

struct tgaasset_mgr_s
{
    taa_asset_storage* storage;
    taa_workqueue* workqueue;
    int lock;
    taa_asset_cache* cache;
    taa_asset_map* map;
    tgaasset* assets;
    uint32_t cachesize;
};

//****************************************************************************
// to be executed on the render thread
static void tgaasset_parse(
    const void* data,
    size_t datasize,
    void* userdata)
{
    tgaasset* asset = (tgaasset*) userdata;
    int32_t err = 0;
    tga_header tga;
    const void* image;
    taa_texformat fmt;    
    // parse the asset
    if(datasize >= 18)
    {
        size_t imageoff;
        tga_read((const unsigned char*) data, &tga, &imageoff);
        image = ((const unsigned char*) data) + imageoff;
        if(tga.imagetype!=tga_TYPE_TRUECOLOR && tga.imagetype!=tga_TYPE_GREY)
        {
            err = -1; // only support truecolor uncompressed or grey scale
        }
        if(tga.colourmaptype != 0 || tga.colourmaplength != 0)
        {
            err = -1; // do not support colour maps
        }
        if((tga.descriptor & 0xC0) != 0)
        {
            err = -1; // do not support interleaved data
        }        
        if(imageoff + tga.width * tga.height * tga.bitsperpixel/8 > datasize)
        {
            err = -1; // check for buffer overflow
        }
    }
    else
    {
        err = -1;
    }
    if(err == 0)
    {
        switch(tga.bitsperpixel)
        {
        case  8: fmt = taa_TEXFORMAT_LUM8;  break;
        case 24: fmt = taa_TEXFORMAT_BGR8;  break;
        case 32: fmt = taa_TEXFORMAT_BGRA8; break;
        default: err = -1;
        }
    }
    if(err == 0)
    {
        taa_texture2d_bind(asset->texture);
        taa_texture2d_image(0, fmt, tga.width, tga.height, image);
        taa_texture2d_bind(0);
        asset->state = taa_ASSET_LOADED;
    }
    else
    {
        asset->state = taa_ASSET_ERROR;
    }
    // release the load reference
    tgaasset_release(asset);
}

//****************************************************************************
static void tgaasset_create(
    tgaasset_mgr* mgr,
    tgaasset* asset)
{
    asset->mgr = mgr;
    asset->state = taa_ASSET_UNLOADED;
    asset->mapval = NULL;
    asset->cacheentry = -1;
    asset->refcount = 0;
    taa_texture2d_create(&asset->texture);
    taa_texture2d_bind(asset->texture);
    taa_texture2d_setparameter(taa_TEXPARAM_MAX_LEVEL, 0);
    taa_texture2d_setparameter(taa_TEXPARAM_MAG_FILTER,taa_TEXFILTER_NEAREST);
    taa_texture2d_setparameter(taa_TEXPARAM_MIN_FILTER,taa_TEXFILTER_NEAREST);
    taa_texture2d_setparameter(taa_TEXPARAM_WRAP_S,taa_TEXWRAP_CLAMP);
    taa_texture2d_setparameter(taa_TEXPARAM_WRAP_T,taa_TEXWRAP_CLAMP);
}

//****************************************************************************
static void tgaasset_destroy(
    tgaasset* asset)
{
    taa_texture2d_destroy(asset->texture);
}

//****************************************************************************
tgaasset* tgaasset_acquire(
    tgaasset_mgr* mgr,
    const taa_asset_key key)
{
    tgaasset* asset = NULL;
    taa_asset_map_value* mapval;
    taa_SPINLOCK_LOCK(&mgr->lock);
    mapval = taa_asset_find(mgr->map, key);
    if(mapval != NULL)
    {
        asset = (tgaasset*) mapval->asset;
        // if the map value has a data reference, need to verify that the data
        // still belongs to the map value and hasn't been reassigned.
        if(asset != NULL && asset->mapval == mapval)
        {
            if(asset->refcount == 0)
            {
                // if a handle to the asset data exists, but it has been
                // unpinned, need to attempt to re-pin it.
                assert(asset->cacheentry >= 0);
                if(taa_asset_repin_cache(mgr->cache,asset->cacheentry,NULL)<0)
                {
                    // because the asset data was verified to still reference
                    // mapval, this shouldn't ever fail
                    assert(0);
                    asset = NULL;
                }
            }
            if(asset != NULL)
            {
                ++asset->refcount; // add refcount for fetch
            }
            taa_SPINLOCK_UNLOCK(&mgr->lock);
        }
        else
        {
            int32_t hcache;
            taa_asset* hasset;
            hcache = taa_asset_pin_cache(mgr->cache, &hasset);
            if(hcache >= 0)
            {
                asset = (tgaasset*) hasset;
                asset->cacheentry = hcache;
            }
            else
            {
                // cache is full, overflow instance needs to be created
                mapval->asset = NULL;
                asset = (tgaasset*) malloc(sizeof(*asset));
                tgaasset_create(mgr, asset);
            }
            mapval->asset = (taa_asset*) asset;
            // the asset needs to be loaded
            asset->mapval = mapval;
            asset->state = taa_ASSET_LOADING;
            ++asset->refcount; // add refcount for fetch
            ++asset->refcount; // add additionl refcount for load
            // unlock before making request to prevent deadlocks
            taa_SPINLOCK_UNLOCK(&mgr->lock);
            taa_asset_request_file(
                mgr->storage,
                mapval->group,
                mapval->file,
                mgr->workqueue,
                tgaasset_parse,
                asset);
        }
    }
    else
    {
        taa_SPINLOCK_UNLOCK(&mgr->lock);
    }
    return asset;
}

//****************************************************************************
void tgaasset_create_mgr(
    taa_asset_storage* storage,
    taa_workqueue* wq,
    uint32_t totalcapacity,
    uint32_t cachesize,
    tgaasset_mgr** mgr_out)
{
    tgaasset_mgr* mgr;
    tgaasset* asset;
    tgaasset* assetend;
    uintptr_t offset;
    int32_t i;
    // determine buffer size and pointer offsets
    offset = 0;
    mgr = (tgaasset_mgr*) offset;
    offset = (uintptr_t) (mgr+1);
    asset = (tgaasset*) taa_ALIGN_PTR(offset, 8);
    offset = (uintptr_t) (asset + cachesize);
    // allocate the buffer and adjust pointers
    offset = (uintptr_t) malloc(offset);
    mgr = (tgaasset_mgr*) (((uintptr_t) mgr) + offset);
    asset = (tgaasset*) (((uintptr_t) asset) + offset);
    assetend = asset + cachesize;
    // initialize asset manager struct
    mgr->storage = storage;
    mgr->workqueue = wq;
    mgr->lock = 0;
    taa_asset_create_cache(cachesize, &mgr->cache);
    taa_asset_create_map(totalcapacity, &mgr->map);
    mgr->assets = asset;
    mgr->cachesize = cachesize;
    // initialize asset cache data
    i = 0;
    while(asset != assetend)
    {
        tgaasset_create(mgr, asset);
        taa_asset_set_cache_entry(mgr->cache, i, (taa_asset*) asset);
        ++asset;
        ++i;
    }
    // set out parameter
    *mgr_out = mgr;
}

//****************************************************************************
void tgaasset_destroy_mgr(
    tgaasset_mgr* mgr)
{
    tgaasset* asset = mgr->assets;
    tgaasset* assetend = asset + mgr->cachesize;
    // clean up asset cache data
    while(asset != assetend)
    {
        tgaasset_destroy(asset);
        ++asset;
    }
    // clean up struct members
    taa_asset_destroy_map(mgr->map);
    taa_asset_destroy_cache(mgr->cache);
    // free buffer
    free(mgr);
}

//****************************************************************************
taa_asset_state tgaasset_poll(
    tgaasset_mgr* mgr,
    tgaasset* asset,
    taa_texture2d* texture_out)
{
    taa_asset_state result = taa_ASSET_ERROR;
    if(asset != NULL)
    {
        result = asset->state;
        if(result == taa_ASSET_LOADED)
        {
            *texture_out = asset->texture;
        }
    }
    return result;
}

//****************************************************************************
void tgaasset_register_storage(
    tgaasset_mgr* mgr,
    taa_asset_group* group)
{
    uint32_t typekey = taa_asset_gen_typekey("tga");
    taa_asset_register_group(mgr->map, group, typekey);
}

//****************************************************************************
void tgaasset_release(
    tgaasset* asset)
{
    tgaasset_mgr* mgr = asset->mgr;
    assert(asset->refcount != 0);
    if(taa_ATOMIC_DEC_32(&asset->refcount) == 0)
    {
        int needfree = 0;
        taa_SPINLOCK_LOCK(&mgr->lock);
        if(asset->refcount == 0)
        {
            if(asset->cacheentry >= 0)
            {
                taa_asset_unpin_cache(mgr->cache, asset->cacheentry);
            }
            else
            {
                asset->mapval->asset = NULL;
                needfree = 1;
            }
        }
        taa_SPINLOCK_UNLOCK(&mgr->lock);
        // don't call system functions while locked
        if(needfree)
        {
            // free overflow instance
            tgaasset_destroy(asset);
            free(asset);
        }
    }
}
