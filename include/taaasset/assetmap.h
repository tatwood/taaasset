/**
 * @brief     map of assets to file information header
 * @author    Thomas Atwood (tatwood.net)
 * @date      2011
 * @copyright unlicense / public domain
 ****************************************************************************/
#ifndef TAAASSET_ASSETMAP_H_
#define TAAASSET_ASSETMAP_H_

#include "assetpack.h"

typedef struct taa_assetmap_key_s taa_assetmap_key;
typedef struct taa_assetmap_value_s taa_assetmap_value;
typedef struct taa_assetmap_s taa_assetmap;

struct taa_assetmap_key_s
{
    uint32_t packhash;
    uint32_t filehash;
};

struct taa_assetmap_value_s
{
    const taa_assetpack* pack;
    uint32_t file;
};

struct taa_assetmap_s
{
    taa_assetmap_key* keys;
    taa_assetmap_value* values;
    uint32_t size;
};

//****************************************************************************
// functions

taa_EXTERN_C void taa_assetmap_create(
    taa_assetmap* mapout);

taa_EXTERN_C void taa_assetmap_destroy(
    taa_assetmap* map);

taa_EXTERN_C int32_t taa_assetmap_find(
    const taa_assetmap* map,
    uint32_t packhash,
    uint32_t filehash);

taa_EXTERN_C void taa_assetmap_register(
    taa_assetmap* map,
    const taa_assetpack_list* assetpacks,
    uint32_t typehash);

#endif // TAAASSET_ASSETMAP_H_
