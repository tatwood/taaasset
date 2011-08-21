/**
 * @brief     cache of in memory assets with fixed slot count implementation
 * @author    Thomas Atwood (tatwood.net)
 * @date      2011
 * @copyright unlicense / public domain
 ****************************************************************************/
#include <taaasset/assetcache.h>
#include <stdlib.h>

typedef struct taa_assetcache_slot_s taa_assetcache_slot;

struct taa_assetcache_slot_s
{
    int32_t assetindex;
    taa_assetcache_slot* garbageprev;
    taa_assetcache_slot* garbagenext;
};

struct taa_assetcache_s
{
    taa_assetcache_slot* slots;
    int32_t* assetmap;
    taa_assetcache_slot garbage;
    uint32_t mapsize;
    uint32_t numslots;
};

//****************************************************************************
static void taa_assetcache_pushgarbage(
    taa_assetcache_slot* anchor,
    taa_assetcache_slot* slot)
{
    slot->garbageprev = anchor->garbageprev;
    slot->garbagenext = anchor;
    anchor->garbageprev->garbagenext = slot;
    anchor->garbageprev = slot;
}

//****************************************************************************
static void taa_assetcache_popgarbage(
    taa_assetcache_slot* slot)
{
    slot->garbageprev->garbagenext = slot->garbagenext;
    slot->garbagenext->garbageprev = slot->garbageprev;
    slot->garbageprev = NULL;
    slot->garbagenext = NULL;
}

//****************************************************************************
int32_t taa_assetcache_acquireslot(
    taa_assetcache* cache,
    uint32_t assetindex)
{
    int32_t result = -1;
    int32_t prevslot = cache->assetmap[assetindex];
    if(prevslot >= 0)
    {
        // the asset already has data associated with it
        // continue to use it and remove it from the garbage list
        taa_assetcache_slot* pslot = cache->slots + prevslot;
        if(pslot->garbagenext != NULL)
        {
            taa_assetcache_popgarbage(pslot);
        }
        result = prevslot;
    }
    else
    {
        // the asset does not have any data associated with it,
        // try to grab some from the garbage queue
        taa_assetcache_slot* pslot = cache->garbage.garbagenext;
        if(pslot != &cache->garbage)
        {
            // if there was an available slot, claim it
            int32_t i = (int32_t) (pslot - cache->slots);
            if(pslot->assetindex >= 0)
            {
                // break any association with previous asset
                cache->assetmap[pslot->assetindex] = -1;
            }
            pslot->assetindex = assetindex;
            taa_assetcache_popgarbage(pslot);
            result = i;
        }
    }
    return result;
}

//****************************************************************************
void taa_assetcache_create(
    uint32_t numslots,
    taa_assetcache** cacheout)
{
    taa_assetcache* cache;
    taa_assetcache_slot* slotitr;
    taa_assetcache_slot* slotend;
    void* buf;
    uint32_t size;

    size = 
        sizeof(*cache) +
        numslots * sizeof(*cache->slots);
    buf = malloc(size);

    cache = (taa_assetcache*) buf;
    buf = cache + 1;
    cache->slots = (taa_assetcache_slot*) buf;
    cache->numslots = numslots;

    cache->assetmap = NULL;
    cache->garbage.assetindex = -1;
    cache->garbage.garbageprev = &cache->garbage;
    cache->garbage.garbagenext = &cache->garbage;
    cache->mapsize = 0;

    slotitr = cache->slots;
    slotend = slotitr + numslots;
    while(slotitr != slotend)
    {
        slotitr->assetindex = -1;
        taa_assetcache_pushgarbage(&cache->garbage, slotitr);
        ++slotitr;
    }

    *cacheout = cache;
}

//****************************************************************************
void taa_assetcache_destroy(
    taa_assetcache* cache)
{
    free(cache->assetmap);
    free(cache);
}

//****************************************************************************
void taa_assetcache_releaseslot(
    taa_assetcache* cache,
    uint32_t slot)
{
    taa_assetcache_slot* pslot = cache->slots + slot;
    taa_assetcache_pushgarbage(&cache->garbage, pslot);
}

//****************************************************************************
void taa_assetcache_resizemap(
    taa_assetcache* cache,
    uint32_t mapsize)
{
    int32_t* m = cache->assetmap;
    int32_t* mapitr;
    int32_t* mapend;

    cache->assetmap = (int32_t*) realloc(m, mapsize*sizeof(*m));
    cache->mapsize = mapsize;

    mapitr = cache->assetmap;
    mapend = mapitr + mapsize;
    while(mapitr != mapend)
    {
        *mapitr = -1;
        ++mapitr;
    }
}
