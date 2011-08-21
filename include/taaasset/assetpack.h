/**
 * @brief     file sgroup header
 * @author    Thomas Atwood (tatwood.net)
 * @date      2011
 * @copyright unlicense / public domain
 ****************************************************************************/
#ifndef TAAASSET_ASSETPACK_H_
#define TAAASSET_ASSETPACK_H_

#include <taa/system.h>
#include <stdio.h>

//****************************************************************************
// typedefs

typedef enum taa_assetpack_type_e taa_assetpack_type;

typedef struct taa_assetpack_file_s taa_assetpack_file;
typedef struct taa_assetpack_base_s taa_assetpack_base;
typedef struct taa_assetpack_loose_s taa_assetpack_loose;
typedef struct taa_assetpack_packed_s taa_assetpack_packed;
typedef union taa_assetpack_u taa_assetpack;
typedef struct taa_assetpack_list_s taa_assetpack_list;

//****************************************************************************
// enums

enum taa_assetpack_type_e
{
    taa_ASSETPACK_LOOSE,
    taa_ASSETPACK_PACKED
};

//****************************************************************************
// structs

struct taa_assetpack_file_s
{
    uint32_t typehash;
    uint32_t filehash;
    /// if packed file, offset within pack. if loose, offset of path in buffer
    uint32_t offset;
    uint32_t size;
};

struct taa_assetpack_base_s
{
    taa_assetpack_type type;
    uint32_t namehash;
    taa_assetpack_file* files;
    uint32_t numfiles;
};

struct taa_assetpack_loose_s
{
    taa_assetpack_type type;
    uint32_t namehash;
    taa_assetpack_file* files;
    uint32_t numfiles;
    uint32_t pathbufsize;
    char* pathbuf;
};

struct taa_assetpack_packed_s
{
    taa_assetpack_type type;
    uint32_t namehash;
    taa_assetpack_file* files;
    uint32_t numfiles;
    FILE* fp;
};

union taa_assetpack_u
{
    taa_assetpack_base base;
    taa_assetpack_loose loose;
    taa_assetpack_packed packed;
};

struct taa_assetpack_list_s
{
    taa_assetpack* packs;
    uint32_t numpacks;
};

//****************************************************************************
// functions

taa_EXTERN_C void taa_assetpack_createlist(
    taa_assetpack_list* list);

taa_EXTERN_C void taa_assetpack_destroylist(
    taa_assetpack_list* list);

taa_EXTERN_C int32_t taa_assetpack_find(
    const taa_assetpack* pack,
    uint32_t typehash,
    uint32_t filehash);

taa_EXTERN_C uint32_t taa_assetpack_hashfilename(
    const char* name);

taa_EXTERN_C uint32_t taa_assetpack_hashfiletype(
    const char* ext);

taa_EXTERN_C uint32_t taa_assetpack_hashpackname(
    const char* name);

taa_EXTERN_C void taa_assetpack_scan(
    taa_assetpack_list* list,
    const char* path);

#endif // TAAASSET_ASSETPACK_H_
