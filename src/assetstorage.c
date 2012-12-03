/**
 * @brief     asset file storage implementation
 * @author    Thomas Atwood (tatwood.net)
 * @date      2011
 * @copyright unlicense / public domain
 ****************************************************************************/
#include <taa/asset.h>
#include <taa/log.h>
#include <taa/path.h>
#include <taa/semaphore.h>
#include <taa/spinlock.h>
#include <taa/thread.h>
#include <stdlib.h>

typedef struct taa_asset_storage_node_s taa_asset_storage_node;

struct taa_asset_storage_node_s
{
    taa_asset_group* group;
    taa_asset_file_request* requests;
    taa_asset_storage_node* next;
};

struct taa_asset_storage_s
{
    taa_thread thread;
    taa_semaphore sem;
    int32_t quit;
    uint32_t lock;
    taa_asset_storage_node* nodes;
    taa_asset_storage_node* pool;
    taa_asset_file_request* requestpool;
    void* end;
};

//****************************************************************************
static taa_thread_result taa_THREAD_CALLCONV taa_asset_storage_thread(
    void* userdata)
{
    taa_asset_storage* storage = (taa_asset_storage*) userdata;
    taa_asset_group* group = NULL;
    // loop until instructed to quit
    while(!storage->quit)
    {
        // loop until there's no more work to do
        while(!storage->quit)
        {
            // loop through all of the requested group queues to find
            // the next file list to process. if additional requests exist
            // for the same group that was previously processed, that
            // node will be reprocessed, otherwise the group at the front of
            // the queue is selected.
            taa_asset_file_request* req;
            taa_asset_file_request* freelist;
            taa_asset_storage_node* node;
            taa_asset_storage_node* itr;
            taa_asset_storage_node** ref;
            taa_asset_storage_node** prevref;
            // lock
            taa_SPINLOCK_LOCK(&storage->lock);
            node = storage->nodes;
            ref = prevref = &storage->nodes;
            itr = node;
            while(itr != NULL)
            {
                if(itr->group == group)
                {
                    // if a node exists matching the previously processed
                    // group, use that node
                    node = itr;
                    ref = prevref;
                    break;
                }
                prevref = &itr->next;
                itr = itr->next;
            }
            if(node == NULL)
            {
                // nothing to do
                taa_SPINLOCK_UNLOCK(&storage->lock);
                break;
            }
            // remove the node from the list
            *ref = node->next;
            taa_SPINLOCK_UNLOCK(&storage->lock);
            // process the file requests; this may take a while
            // the lock MUST be released at this point
            group = node->group;
            req = node->requests;
            group->loadfunc(group, req);
            // lock
            taa_SPINLOCK_LOCK(&storage->lock);
            freelist = NULL;
            while(req != NULL)
            {
                // release the file requests
                taa_asset_file_request* next = req->next;
                if(((void*) req) > userdata && ((void*) req) < storage->end)
                {
                    req->next = storage->requestpool;
                    storage->requestpool = req;
                }
                else
                {
                    // put overflow pointer in free list. to prevent stalls,
                    // do not call free until lock is released
                    req->next = freelist;
                    freelist = req;
                }
                req = next;
            }
            if(((void*) node) > userdata && ((void*) node) < storage->end)
            {
                // put the node back in the pool
                node->next = storage->pool;
                storage->pool = node;
                node = NULL;
            }
            taa_SPINLOCK_UNLOCK(&storage->lock);
            if(node != NULL)
            {
                // free overflow node after lock is released
                free(node);
                taa_LOG_DEBUG("asset storage freed overflow node req");              
            }
            // free all the overflow requests while lock is released
            while(freelist != NULL)
            {
                taa_asset_file_request* next = freelist->next;
                free(freelist);
                taa_LOG_DEBUG("asset storage freed overflow file req");
                freelist = next;
            }
            // be polite and yield after releasing the lock
            taa_sched_yield();
        }
        // if there's nothing to do, sleep until there is
        taa_semaphore_wait(&storage->sem);
    }
    return 0;
}

//****************************************************************************
void taa_asset_create_storage(
    uint32_t storagecapacity,
    uint32_t reqcapacity,
    taa_asset_storage** storage_out)
{
    uintptr_t offset = 0;
    taa_asset_storage* storage;
    taa_asset_storage_node* node;
    taa_asset_storage_node* nodeend;
    taa_asset_file_request* req;
    taa_asset_file_request* reqend;
    // determine buffer size and pointer offsets
    storage = (taa_asset_storage*) offset;
    offset = (uintptr_t) (storage + 1);
    node = (taa_asset_storage_node*) taa_ALIGN_PTR(offset, 8);
    offset = (uintptr_t) (node + storagecapacity);
    req = (taa_asset_file_request*) taa_ALIGN_PTR(offset, 8);
    offset = (uintptr_t) (req + reqcapacity);
    // allocate the buffer and adjust pointers
    offset = (uintptr_t) calloc(1, offset);
    storage = (taa_asset_storage*) (((uintptr_t) storage) + offset);
    node = (taa_asset_storage_node*) (((uintptr_t) node) + offset);
    req = (taa_asset_file_request*) (((uintptr_t) req) + offset);
    nodeend = node + storagecapacity;
    reqend = req + reqcapacity;
    // initialize assetstorage struct
    storage->pool = node;
    storage->requestpool = req;
    storage->end = reqend;
    // initialize node pool
    while(node != nodeend-1)
    {
        node->next = node + 1;
        ++node;
    }
    // initialize request pool
    while(req != reqend-1)
    {
        req->next = req + 1;
        ++req;
    }
    // initialize thread
    taa_semaphore_create(&storage->sem);
    taa_thread_create(taa_asset_storage_thread, storage, &storage->thread);
    // set out parameter
    *storage_out = storage;
}

//****************************************************************************
void taa_asset_destroy_storage(
    taa_asset_storage* storage)
{
    taa_asset_storage_node* node;
    // stop the storage thread
    if(!storage->quit)
    {
        taa_asset_stop_storage_thread(storage);
    }
    taa_semaphore_destroy(&storage->sem);
    node = storage->nodes;
    // loop through outstanding requests and make sure no overflow mem leaks
    while(node != NULL)
    {
        taa_asset_storage_node* next = node->next;
        taa_asset_file_request* req = node->requests;
        while(req != NULL)
        {
            taa_asset_file_request* reqnext = req->next;
            if(((void*)req) < ((void*)storage) || ((void*)req) > storage->end)
            {
                free(req);
            }
            req = reqnext;
        }
        if(((void*)node) < ((void*)storage)|| ((void*)node) > storage->end)
        {
            free(node);
        }
        node = next;
    }
    // free the buffer
    free(storage);
}

//****************************************************************************
void taa_asset_request_file(
    taa_asset_storage* storage,
    taa_asset_group* group,
    taa_asset_file* file,
    taa_workqueue* wq,
    taa_asset_parse_func parsefunc,
    void* userdata)
{
    taa_asset_storage_node* node;
    taa_asset_storage_node* itr;
    taa_asset_file_request* req;
    // lock
    taa_SPINLOCK_LOCK(&storage->lock);
    // get a file request struct
    // note that since the lock may be released here, the request node fetch
    // must be performed before node is found or added to manager to avoid
    // race conditions when it is relocked.
    if(storage->requestpool != NULL)
    {
        req = storage->requestpool;
        storage->requestpool = req->next;
    }
    else
    {
        // unlock before calling system functions to avoid stalls.
        taa_SPINLOCK_UNLOCK(&storage->lock);
        taa_LOG_WARN("asset storage file pool empty, alloced overflow");
        req = (taa_asset_file_request*) malloc(sizeof(*req));
        taa_SPINLOCK_LOCK(&storage->lock);
        // not holding pointers to data that may have changed, so safe
        // to proceed
    }
    // try to find an active request node for the specified storage instance
    node = storage->nodes;
    itr = node;
    while(itr != NULL)
    {
        if(itr->group == group)
        {
            // found it!
            node = itr;
            break;
        }
        itr = itr->next;
    }
    if(node == NULL)
    {
        // no pending requests for the storage instance, need to create one
        if(storage->pool != NULL)
        {
            node = storage->pool;
            storage->pool = node->next;
        }
        else
        {
            // unlock before calling system functions to avoid stalls.
            taa_SPINLOCK_UNLOCK(&storage->lock);
            taa_LOG_WARN("asset storage node pool empty, alloced overflow");
            node = (taa_asset_storage_node*) malloc(sizeof(*node));
            taa_SPINLOCK_LOCK(&storage->lock);
            // not holding pointers to data that may have changed,
            // so safe to proceed
        }
        node->group = group;
        node->requests = NULL;
        node->next = storage->nodes;
        storage->nodes = node;
    }
    req->file = file;
    req->workqueue = wq;
    req->parsefunc = parsefunc;
    req->userdata = userdata;
    req->next = node->requests;
    node->requests = req;
    taa_SPINLOCK_UNLOCK(&storage->lock);
    taa_semaphore_post(&storage->sem);
}

//****************************************************************************
void taa_asset_stop_storage_thread(
    taa_asset_storage* storage)
{
    storage->quit = 1;
    taa_semaphore_post(&storage->sem);
    taa_thread_join(storage->thread);
}
