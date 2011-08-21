/**
 * @brief     cache of in memory assets with fixed slot count header
 * @author    Thomas Atwood (tatwood.net)
 * @date      2011
 * @copyright unlicense / public domain
 ****************************************************************************/
#ifndef TAAASSET_ASSETCACHE_H_
#define TAAASSET_ASSETCACHE_H_

#include <taa/system.h>

//****************************************************************************
// typedefs

typedef struct taa_assetcache_s taa_assetcache;

//****************************************************************************
// functions

/**
 * attempts to assign a slot index to the asset
 * @return the slot index if it was able to acquire the slot, -1 otherwise
 */
taa_EXTERN_C int32_t taa_assetcache_acquireslot(
    taa_assetcache* cache,
    uint32_t assetindex);

/**
 * @param numassets the total number of assets to manage
 * @param numslots the maximum slots that may be acquired at any given time
 */
taa_EXTERN_C void taa_assetcache_create(
    uint32_t numslots,
    taa_assetcache** cacheout);

taa_EXTERN_C void taa_assetcache_destroy(
    taa_assetcache* cache);

/**
 * instructs the cache that the slot can be reassigned to another asset
 */
taa_EXTERN_C void taa_assetcache_releaseslot(
    taa_assetcache* cache,
    uint32_t slot);

taa_EXTERN_C void taa_assetcache_resizemap(
    taa_assetcache* cache,
    uint32_t mapsize);

#endif // TAAASSET_ASSETCACHE_H_
