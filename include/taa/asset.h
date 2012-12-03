/**
 * @brief     base asset header
 * @author    Thomas Atwood (tatwood.net)
 * @date      2011
 * @copyright unlicense / public domain
 ****************************************************************************/
#ifndef taa_ASSET_H_
#define taa_ASSET_H_

#include <taa/workqueue.h>

#ifndef taa_ASSET_LINKAGE
#define taa_ASSET_LINKAGE
#endif

//****************************************************************************
// enums

enum taa_asset_state_e
{
    taa_ASSET_UNLOADED,
    taa_ASSET_LOADING,
    taa_ASSET_LOADED,
    taa_ASSET_ERROR
};

//****************************************************************************
// typedefs

typedef enum taa_asset_state_e taa_asset_state;

// data types

typedef struct taa_asset_file_s taa_asset_file;
typedef struct taa_asset_file_request_s taa_asset_file_request;
typedef struct taa_asset_group_s taa_asset_group;
typedef union taa_asset_key_u taa_asset_key;

// opaque types

typedef struct taa_asset_s taa_asset;
typedef struct taa_asset_storage_s taa_asset_storage;

/**
 * @brief function pointer responsible for parsing the loaded storage data
 * @details This function is placed in the work queue associated with the
 *          file request. It should translate and copy the provided data
 *          buffer into the native in-memory representation. The data buffer
 *          is temporary and will expire after this function returns.
 * @param buf the file data loaded from storage
 * @param size the size of the file data in bytes
 * @param userdata the user context data that was provided when the request
 *        was made.
 */
typedef void (*taa_asset_parse_func)(
    const void* buf,
    size_t size,
    void* userdata);

/**
 * @brief function pointer responsible for reading data from storage
 * @details This function is implemented by storage plugins to read the
 *          requested files from persistent storage. It will be executed on
 *          the storage thread and should block until all the requests have
 *           been fulfilled.
 * @param storage the storage instance that contains the requested files
 * @param requests a linked list of file requests to be fulfilled
 */
typedef void (*taa_asset_load_func)(
    taa_asset_group* group,
    taa_asset_file_request* requests);

//****************************************************************************
// structs

struct taa_asset_file_s
{
    const char* name;
    uint32_t typekey;
    uint32_t filekey;
    uint32_t size;
    uintptr_t handle;
};

struct taa_asset_file_request_s
{
    taa_asset_file* file;
    taa_workqueue* workqueue;
    taa_asset_parse_func parsefunc;
    void* userdata;
    taa_asset_file_request* next;
};

struct taa_asset_group_s
{
    const char* name;
    uint32_t key;
    uint32_t numfiles;
    taa_asset_file* files;
    taa_asset_load_func loadfunc;
};

union taa_asset_key_u
{
    struct
    {
        uint32_t group;
        uint32_t file;
    } parts;
    uint64_t all;
};

//****************************************************************************
// key generation functions

taa_ASSET_LINKAGE taa_asset_key taa_asset_gen_key(
    const char* group,
    const char* file);

taa_ASSET_LINKAGE uint32_t taa_asset_gen_filekey(
    const char* name);

taa_ASSET_LINKAGE uint32_t taa_asset_gen_groupkey(
    const char* name);

taa_ASSET_LINKAGE uint32_t taa_asset_gen_typekey(
    const char* ext);

//****************************************************************************
// storage functions

/**
 * @param groupcapacity The maximum number of groups that can be
 *        actively requested at one time.
 * @param reqcapacity The maximum number of file instances that can be
 *        actively requested at one time.
 * @param storage_out out parameter that will be set to the storage handle
 */
taa_ASSET_LINKAGE void taa_asset_create_storage(
    uint32_t storagecapacity,
    uint32_t reqcapacity,
    taa_asset_storage** storage_out);

taa_ASSET_LINKAGE void taa_asset_destroy_storage(
    taa_asset_storage* storage);

/**
 * @param storage handle to the asset storage instance
 * @param group the file container
 * @param file the file to load
 * @param wq the parse callback will be pushed to this workqueue
 * @param parsefunc callback function to call to parse the loaded file
 * @param userdata context data that will be provided to parse function
 */
taa_ASSET_LINKAGE void taa_asset_request_file(
    taa_asset_storage* storage,
    taa_asset_group* group,
    taa_asset_file* file,
    taa_workqueue* wq, 
    taa_asset_parse_func parsefunc,
    void* userdata);

taa_ASSET_LINKAGE void taa_asset_stop_storage_thread(
    taa_asset_storage* storage);

#endif // taa_ASSET_H_
