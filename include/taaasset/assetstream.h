/**
 * @brief     asynchronous asset file loader header
 * @author    Thomas Atwood (tatwood.net)
 * @date      2011
 * @copyright unlicense / public domain
 ****************************************************************************/
#ifndef TAAASSET_ASSETSTREAM_H_
#define TAAASSET_ASSETSTREAM_H_

#include "assetpack.h"

//****************************************************************************
// typedefs

typedef struct taa_assetstream_s taa_assetstream;

typedef void (*taa_assetstream_callback)(
    const void* buf,
    uint32_t size,
    int32_t result,
    void* userdata);

//****************************************************************************
// functions

taa_EXTERN_C void taa_assetstream_create(
    taa_assetstream** streamout);

taa_EXTERN_C void taa_assetstream_destroy(
    taa_assetstream* stream);

taa_EXTERN_C void taa_assetstream_load(
    taa_assetstream* stream,
    const taa_assetpack* pack,
    uint32_t fileindex,
    taa_assetstream_callback callback,
    void* userdata);

void taa_assetstream_release(
    taa_assetstream* stream,
    const void* buf);

taa_EXTERN_C void taa_assetstream_update(
    taa_assetstream* stream);

#endif // TAAASSET_ASSETSTREAM_H_
