/**
 * @brief     fixed sized cache of in memory assets implementation
 * @author    Thomas Atwood (tatwood.net)
 * @date      2011
 * @copyright unlicense / public domain
 ****************************************************************************/
#include <taa/assetcache.h>
#include <assert.h>
#include <stdlib.h>

typedef struct taa_asset_cache_node_s taa_asset_cache_node;

//****************************************************************************
// structs

struct taa_asset_cache_node_s
{
    taa_asset* asset;
    taa_asset_cache_node* prev;
    taa_asset_cache_node* next;
};

struct taa_asset_cache_s
{
    taa_asset_cache_node* nodes;
    size_t size;
    taa_asset_cache_node pool;
};

//****************************************************************************
static void taa_asset_cache_push_pool(
    taa_asset_cache_node* anchor,
    taa_asset_cache_node* node)
{
    node->prev = anchor->prev;
    node->next = anchor;
    anchor->prev->next = node;
    anchor->prev = node;
}

//****************************************************************************
static void taa_asset_cache_pop_pool(
    taa_asset_cache_node* node)
{
    node->prev->next = node->next;
    node->next->prev = node->prev;
    node->prev = NULL;
    node->next = NULL;
}

//****************************************************************************
void taa_asset_create_cache(
    size_t size,
    taa_asset_cache** cache_out)
{
    taa_asset_cache_node* nodeitr;
    taa_asset_cache_node* nodeend;
    taa_asset_cache* cache;
    uintptr_t offset;
    offset = 0;
    // calculate buffer size and pointer offsets
    cache = (taa_asset_cache*) taa_ALIGN_PTR(offset, 8);
    offset = (uintptr_t) (cache + 1);
    nodeitr = (taa_asset_cache_node*) taa_ALIGN_PTR(offset, 8);
    offset = (uintptr_t) (nodeitr + size);
    // allocate buffer and set pointer values
    offset = (uintptr_t) calloc(offset, 1);
    cache = (taa_asset_cache*) (((uintptr_t) cache) + offset);
    nodeitr = (taa_asset_cache_node*) (((uintptr_t) nodeitr) + offset);
    nodeend = nodeitr + size;
    // init struct data
    cache->nodes = nodeitr;
    cache->size = size;
    cache->pool.prev = &cache->pool;
    cache->pool.next = &cache->pool;
    while(nodeitr != nodeend)
    {
        taa_asset_cache_push_pool(&cache->pool, nodeitr);
        ++nodeitr;
    }
    // set out param
    *cache_out = cache;
}

//****************************************************************************
void taa_asset_destroy_cache(
    taa_asset_cache* cache)
{
    free(cache);
}

//****************************************************************************
int taa_asset_pin_cache(
    taa_asset_cache* cache,
    taa_asset** asset_out)
{
    int entry = -1;
    // try to grab a entry from the pool queue
    taa_asset_cache_node* node = cache->pool.next;
    if(node != &cache->pool)
    {
        // if there was an available entry, claim it
        entry = (int) (ptrdiff_t) (node - cache->nodes);
        taa_asset_cache_pop_pool(node);
        if(asset_out != NULL)
        {
            *asset_out = node->asset;
        }
    }
    return entry;
}

//****************************************************************************
int taa_asset_repin_cache(
    taa_asset_cache* cache,
    int entry,
    taa_asset** asset_out)
{
    taa_asset_cache_node* node = cache->nodes + entry;
    assert(((size_t) entry) < cache->size);
    // if the entry is in the pool, reclaim it
    if(node->next != NULL)
    {
        taa_asset_cache_pop_pool(node);
        if(asset_out != NULL)
        {
            *asset_out = node->asset;
        }
    }
    else
    {
        entry = -1;
    }
    return entry;
}

//****************************************************************************
void taa_asset_set_cache_entry(
    taa_asset_cache* cache,
    int entry,
    taa_asset* asset)
{
    assert(((size_t) entry) < cache->size);
    cache->nodes[entry].asset = asset;
}

//****************************************************************************
void taa_asset_unpin_cache(
    taa_asset_cache* cache,
    int entry)
{
    taa_asset_cache_node* node = cache->nodes + entry;
    taa_asset_cache_push_pool(&cache->pool, node);
}
