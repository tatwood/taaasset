/**
 * @brief     map of assets to file information header
 * @author    Thomas Atwood (tatwood.net)
 * @date      2011
 * @copyright unlicense / public domain
 ****************************************************************************/
#ifndef taa_ASSETMAP_H_
#define taa_ASSETMAP_H_

#include "asset.h"

//****************************************************************************
// typedefs

typedef struct taa_asset_map_value_s taa_asset_map_value;
typedef struct taa_asset_map_s taa_asset_map;

//****************************************************************************
// structs

struct taa_asset_map_value_s
{
    taa_asset_group* group;
    taa_asset_file* file;
    taa_asset* asset;
};

//****************************************************************************
// functions

taa_ASSET_LINKAGE void taa_asset_create_map(
    uint32_t capacity,
    taa_asset_map** map_out);

taa_ASSET_LINKAGE void taa_asset_destroy_map(
    taa_asset_map* map);

taa_ASSET_LINKAGE taa_asset_map_value* taa_asset_find(
    const taa_asset_map* map,
    const taa_asset_key key);

/**
 * @details iterates the storage group and inserts all files matching the
 *          specified type key into the map
 */
taa_ASSET_LINKAGE void taa_asset_register_group(
    taa_asset_map* map,
    taa_asset_group* group,
    uint32_t typekey);

#endif // taa_ASSETMAP_H_
