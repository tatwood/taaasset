/**
 * @brief     map of assets to file information implementation
 * @author    Thomas Atwood (tatwood.net)
 * @date      2011
 * @copyright unlicense / public domain
 ****************************************************************************/
#include <taa/assetmap.h>
#include <taa/log.h>
#include <taa/path.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

struct taa_asset_map_s
{
    taa_asset_key* keys;
    taa_asset_map_value* values;
    uint32_t size;
    uint32_t capacity;
};

//****************************************************************************
static unsigned int taa_asset_map_search(
    const taa_asset_key* keys,
    unsigned int numkeys,
    uint32_t groupkey,
    uint32_t filekey)
{
    unsigned int lo = 0;
    unsigned int hi = numkeys;
    while(lo < hi)
    {
        unsigned int i = lo + ((hi-lo) >> 1);
        const taa_asset_key* k = keys + i;
        if(k->parts.group < groupkey)
        {
            lo = i + 1;
        }
        else if(k->parts.group == groupkey && k->parts.file < filekey)
        {
            lo = i + 1;
        }
        else
        {
            hi = i;
        }
    }
    return lo;
}

//****************************************************************************
void taa_asset_create_map(
    uint32_t capacity,
    taa_asset_map** map_out)
{
    taa_asset_map* map;
    map = (taa_asset_map*) calloc(1, sizeof(*map));
    map->keys = (taa_asset_key*) malloc(capacity * sizeof(*map->keys));
    map->values = (taa_asset_map_value*) malloc(capacity*sizeof(*map->values));
    map->capacity = capacity;
    *map_out = map;
}

//****************************************************************************
void taa_asset_destroy_map(
    taa_asset_map* map)
{
    free(map->keys);
    free(map->values);
    free(map);
};

//****************************************************************************
taa_asset_map_value* taa_asset_find(
    const taa_asset_map* map,
    const taa_asset_key key)
{
    taa_asset_map_value* result = NULL;
    uint32_t i = taa_asset_map_search(
        map->keys,
        map->size,
        key.parts.group,
        key.parts.file);
    if(i < map->size)
    {
        if(map->keys[i].all == key.all)
        {
            result = map->values + i;
        }
    }
    return result;
}

//****************************************************************************
void taa_asset_register_group(
    taa_asset_map* map,
    taa_asset_group* group,
    uint32_t typekey)
{
    uint32_t groupkey = group->key;
    uint32_t n;
    uint32_t total;
    taa_asset_key* keyitr;
    taa_asset_key* keydst;
    taa_asset_map_value* valitr;
    taa_asset_map_value* valdst;
    taa_asset_file* fileitr;
    taa_asset_file* fileend;
    // count the relevant number of files in the group
    fileitr = group->files;
    fileend = fileitr+group->numfiles;
    n = 0;
    while(fileitr != fileend)
    {
        if(fileitr->typekey == typekey)
        {
            ++n;
        }
        ++fileitr;
    }
    total = map->size + n;
    // ensure buffers have enough capacity
    if(total > map->capacity)
    {
        uint32_t sz;
        uint32_t ncap = (total + 15) & ~15; // round to nearest 16
        sz = ncap * sizeof(*map->keys);
        map->keys = (taa_asset_key*) realloc(map->keys, sz);
        sz = ncap * sizeof(*map->values);
        map->values=(taa_asset_map_value*) realloc(map->values, sz);
        map->capacity = ncap;
        taa_LOG_WARN("asset map over capacity. resized to: %u", ncap);
    }
    // make room to insert the nodes at the correct position
    keyitr = map->keys + map->size;
    keydst = map->keys + total - 1;
    valitr = map->values + map->size;
    valdst = map->values + total - 1;
    while(keyitr != map->keys && keyitr->parts.group > groupkey)
    {
        *keydst = *keyitr;
        *valdst = *valitr;
        --keyitr;
        --keydst;
        --valitr;
        --valdst;
    }
    // loop through the files again and insert the data
    fileitr = group->files;
    fileend = fileitr+group->numfiles;
    n = 0;
    while(fileitr != fileend)
    {
        if(fileitr->typekey == typekey)
        {
            taa_asset_key* key;
            taa_asset_map_value* val;
            uint32_t i;
            i = taa_asset_map_search(keyitr,n,groupkey,fileitr->filekey);
            // TODO: check for hash conflicts?
            // make room for the file
            key = keyitr + i;
            val = valitr + i;
            keydst = keyitr + n;
            valdst = valitr + n;
            while(keydst > key)
            {
                *keydst = keydst[-1];
                *valdst = valdst[-1];
                --keydst;
                --valdst;
            }
            // insert the value
            key->parts.group = groupkey;
            key->parts.file = fileitr->filekey;
            val->group = group;
            val->file = fileitr;
            val->asset = NULL;
            ++n;
        }
        ++fileitr;
    }
    map->size += n;
    assert(map->size == total);
}
