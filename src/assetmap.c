/**
 * @brief     map of assets to file information implementation
 * @author    Thomas Atwood (tatwood.net)
 * @date      2011
 * @copyright unlicense / public domain
 ****************************************************************************/
#include <taaasset/assetmap.h>
#include <taa/error.h>
#include <taa/hash.h>
#include <taa/path.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

//****************************************************************************
static uint32_t taa_assetmap_search(
    const taa_assetmap_key* keys,
    uint32_t numkeys,
    uint32_t packhash,
    uint32_t filehash)
{
    uint32_t lo = 0;
    uint32_t hi = numkeys;
    while(lo < hi)
    {
        uint32_t i = lo + ((hi-lo) >> 1);
        const taa_assetmap_key* k = keys + i;
        if(k->packhash < packhash)
        {
            lo = i + 1;
        }
        else if(k->packhash == packhash && k->filehash < filehash)
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
void taa_assetmap_create(
    taa_assetmap* mapout)
{
    memset(mapout, 0, sizeof(*mapout));
}

//****************************************************************************
void taa_assetmap_destroy(
    taa_assetmap* map)
{
    free(map->keys);
    free(map->values);
};

//****************************************************************************
int32_t taa_assetmap_find(
    const taa_assetmap* map,
    uint32_t packhash,
    uint32_t filehash)
{
    int32_t result = -1;
    uint32_t i = taa_assetmap_search(
        map->keys,
        map->size,
        packhash,
        filehash);
    if(i < map->size)
    {
        const taa_assetmap_key* k = map->keys + i;
        if(k->packhash == packhash && k->filehash == filehash)
        {
            result = i;
        }
    }
    return result;
}

//****************************************************************************
void taa_assetmap_register(
    taa_assetmap* map,
    const taa_assetpack_list* assetpacks,
    uint32_t typehash)
{
    uint32_t n = 0;
    const taa_assetpack* packitr = assetpacks->packs;
    const taa_assetpack* packend = packitr + assetpacks->numpacks;
    const taa_assetpack* prevpack = NULL;
    // count the relevant number of files in every pack
    while(packitr != packend)
    {
        const taa_assetpack_file* fileitr = packitr->base.files;
        const taa_assetpack_file* fileend = fileitr+packitr->base.numfiles;
        while(fileitr != fileend)
        {
            if(fileitr->typehash == typehash)
            {
                ++n;
            }
            ++fileitr;
        }
        ++packitr;
    }
    // allocate buffers
    map->keys = (taa_assetmap_key*) malloc(n * sizeof(*map->keys));
    map->values = (taa_assetmap_value*) malloc(n*sizeof(*map->values));
    map->size = 0;
    // loop through everything again and fill out the data
    while(1)
    {
        uint32_t hash = ~0;
        taa_assetmap_key* keyitr = map->keys + map->size;
        taa_assetmap_value* valueitr = map->values + map->size;
        const taa_assetpack* pack = NULL;
        const taa_assetpack_file* fileitr;
        const taa_assetpack_file* fileend;
        // the files are already sorted by hash within the asset packs
        // but the packs themselves need to be sorted
        packitr = assetpacks->packs;
        if(prevpack == NULL)
        {
            while(packitr != packend)
            {
                uint32_t thash = packitr->base.namehash;
                if (thash <= hash)
                {
                    hash = thash;
                    pack = packitr;
                }
                ++packitr;
            }
        }
        else
        {
            uint32_t prevhash = prevpack->base.namehash;
            while(packitr != packend)
            {
                uint32_t thash = packitr->base.namehash;
                if (thash > prevhash && thash <= hash)
                {
                    hash = thash;
                    pack = packitr;
                }
                ++packitr;
            }
        }
        if(pack == NULL)
        {
            break;
        }
        // after the next pack is selected, create entries for its files
        fileitr = pack->base.files;
        fileend = fileitr + pack->base.numfiles;
        while(fileitr != fileend)
        {
            if(fileitr->typehash == typehash)
            {
                keyitr->packhash = pack->base.namehash;
                keyitr->filehash = fileitr->filehash;
                valueitr->pack = pack;
                valueitr->file = (uint32_t) (fileitr - pack->base.files);
                ++keyitr;
                ++valueitr;
                ++map->size;
            }
            ++fileitr;
        }
        prevpack = pack;
    }
    assert(map->size == n);
}
