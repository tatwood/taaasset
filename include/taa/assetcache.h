/**
 * @brief     fixed sized cache of in memory assets implementation
 * @author    Thomas Atwood (tatwood.net)
 * @date      2011
 * @copyright unlicense / public domain
 ****************************************************************************/
#ifndef taa_ASSETCACHE_H_
#define taa_ASSETCACHE_H_

#include "asset.h"
#include <taa/system.h>

//****************************************************************************
// typedefs

/**
 * @brief fixed size cache of in memory assets
 * @details The purpose of the cache is to manage the mapping and unmapping of
 * a fixed number of asset resources so that they can be assigned and 
 * reassigned to asset keys on an as needed basis. It has no knowledge of the
 * format of the data within it or of the keys associated with the assets.
 */
typedef struct taa_asset_cache_s taa_asset_cache;

//****************************************************************************
// functions

/**
 * @param size the maximum pinned entries the cache may support at one time
 */
void taa_asset_create_cache(
    size_t size,
    taa_asset_cache** cache_out);

void taa_asset_destroy_cache(
    taa_asset_cache* cache);

/**
 * @brief attempts to find an unused cache entry and locks it if available
 * @param cache the cache instance
 * @param asset_out address to receive the asset data for the cache entry if
 *        an entry is succssfully pinned
 * @return the index of the pinned entry is returned on success, -1 otherwise
 */
int taa_asset_pin_cache(
    taa_asset_cache* cache,
    taa_asset** asset_out);

/**
 * @brief attempts to relock a previous cache assignment
 * @details if the owner of the cache unpinned an entry that it previously
 * had assigned to a resource, this function allows it to attempt to retrieve
 * that same entry again, if it is not currently pinned.
 * @param entry the index of the entry
 * @param asset_out address to receive the asset data for the cache entry if
 *        it is succssfully repinned
 * @return entry if it was not reassigned, -1 otherwise
 */
int taa_asset_repin_cache(
    taa_asset_cache* cache,
    int entry,
    taa_asset** asset_out);

/**
 * @brief associates a cache entry index with asset data
 * @details This function allows asset data to be associated with a cache
 * entry, which will be retrieved whenever the entry becomes pinned. The data
 * will remain associated with the entry until set entry is performed 
 * again for the same entry index. The initial asset data associated with a 
 * cache entry is NULL. If this function is not used to set the data for an
 * entry, pin operations will result in a NULL asset output value.
 */
void taa_asset_set_cache_entry(
    taa_asset_cache* cache,
    int entry,
    taa_asset* asset);

/**
 * @brief instructs the cache that an entry can be reassigned to another asset
 */
void taa_asset_unpin_cache(
    taa_asset_cache* cache,
    int entry);

#endif // taa_ASSETCACHE_H_
