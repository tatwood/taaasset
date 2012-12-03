/**
 * @brief     asset file storage in directory structure header
 * @author    Thomas Atwood (tatwood.net)
 * @date      2011
 * @copyright unlicense / public domain
 ****************************************************************************/
#ifndef taa_ASSETDIR_H_
#define taa_ASSETDIR_H_

#include "asset.h"

//****************************************************************************
// typedefs

typedef struct taa_asset_dir_storage_s taa_asset_dir_storage;

//****************************************************************************
// functions

/**
 * @brief creates directory manager instance for servicing storage requests
 * @param maxrequests maximum number of requests to service simultaneously
 * @param mgr_out pointer to output handle
 */
taa_ASSET_LINKAGE void taa_asset_create_dir_storage(
    uint32_t maxrequests,
    taa_asset_dir_storage** mgr_out);

taa_ASSET_LINKAGE void taa_asset_destroy_dir_storage(
    taa_asset_dir_storage* mgr);

/**
 * @brief creates a storage group for the files in the specified path
 */
taa_ASSET_LINKAGE taa_asset_group* taa_asset_scan_dir(
    taa_asset_dir_storage* mgr,
    const char* name,
    const char* path);

#endif // taa_ASSETDIR_H_

