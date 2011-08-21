/**
 * @brief     asynchronous asset file loader implementation
 * @author    Thomas Atwood (tatwood.net)
 * @date      2011
 * @copyright unlicense / public domain
 ****************************************************************************/
#include <taaasset/assetstream.h>
#include <taa/semaphore.h>
#include <taa/thread.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct taa_assetstream_data_s taa_assetstream_data;
typedef struct taa_assetstream_req_s taa_assetstream_req;
typedef struct taa_assetstream_reqchunk_s taa_assetstream_reqchunk;

struct taa_assetstream_data_s
{
    void* buf;
    uint32_t size;
    volatile int32_t locked;
};

struct taa_assetstream_req_s
{
    const taa_assetpack* pack;
    uint32_t file;
    taa_assetstream_callback callback;
    void* userdata;
    const void* buf;
    uint32_t size;
    int32_t result;
    taa_assetstream_req* next;
};

struct taa_assetstream_reqchunk_s
{
    taa_assetstream_req buf[128];
    taa_assetstream_reqchunk* next;
};

struct taa_assetstream_s
{
    taa_assetstream_req* loadlist;
    taa_assetstream_req* callbacklist; 
    taa_assetstream_req* pool;
    taa_assetstream_reqchunk* chunks;
    taa_assetstream_data data[2];
    taa_thread thread;
    taa_semaphore sem;
    volatile int32_t quit;
};

//****************************************************************************
static void taa_assetstream_allocreq(
    taa_assetstream* stream)
{
    taa_assetstream_req* itr;
    taa_assetstream_req* end;
    taa_assetstream_req* prev;
    taa_assetstream_reqchunk* chunk;
    chunk = (taa_assetstream_reqchunk*) malloc(sizeof(*chunk));
    itr = chunk->buf;
    end = itr + (sizeof(chunk->buf)/sizeof(*chunk->buf)) - 1;
    while(itr != end)
    {
        itr->next = itr + 1;
        ++itr;
    }
    end->next = stream->pool;
    do
    {
        prev = end->next;
        end->next = taa_ATOMIC_CMPXCHGPTR(&stream->pool,end->next,chunk->buf);
    }
    while(end->next != prev);
    chunk->next = stream->chunks;
    stream->chunks = chunk;
}

//****************************************************************************
static taa_assetstream_req* taa_assetstream_popreq(
    taa_assetstream_req** list)
{
    taa_assetstream_req* req = *list;
    taa_assetstream_req* prev;
    do
    {
        prev = req;
        req = taa_ATOMIC_CMPXCHGPTR(list, req, NULL);
    }
    while(req != prev);
    return req;
}

//****************************************************************************
static void taa_assetstream_pushreq(
    taa_assetstream_req** list,
    taa_assetstream_req* req)
{
    taa_assetstream_req* next;
    req->next = *list;
    do
    {
        next = req->next;
        req->next = taa_ATOMIC_CMPXCHGPTR(list, req->next, req);
    }
    while(req->next != next);
}


//****************************************************************************
static int32_t taa_assetstream_thread(
    void* args)
{
    taa_assetstream* stream = (taa_assetstream*) args;
    taa_assetstream_req* req = NULL;
    taa_assetstream_data* data = stream->data;
    taa_assetstream_data* dataend = data+sizeof(stream->data)/sizeof(*data);
    data->locked = 1;
    while(!stream->quit)
    {
        if(req == NULL)
        { 
            req = taa_assetstream_popreq(&stream->loadlist);
        }
        if(data == NULL)
        {
            taa_assetstream_data* dataitr = stream->data;
            while(dataitr != dataend)
            {
                if(!dataitr->locked)
                {
                    data = dataitr;
                    data->locked = 1;
                    break;
                }
                ++dataitr;
            }
        }
        if(req != NULL && data != NULL)
        {
            taa_assetstream_req* next = req->next;
            const taa_assetpack* pack = req->pack;
            const taa_assetpack_file* file = pack->base.files + req->file;
            req->buf = NULL;
            req->size = 0;
            req->result = -1;
            if(req->pack->base.type == taa_ASSETPACK_LOOSE)
            {
                const taa_assetpack_loose* loose = &pack->loose;
                FILE* fp = fopen(loose->pathbuf + file->offset, "rb");
                if(fp != NULL)
                {
                    uint32_t size = file->size;
                    if(size > data->size)
                    {
                        data->size = size;
                        data->buf = realloc(data->buf, size);
                    }
                    if(fread(data->buf, 1, size, fp) == size)
                    {
                        req->buf = data->buf;
                        req->size = size;
                        req->result = 0;
                        data = NULL;
                    }
                    fclose(fp);
                }
            }
            else
            {
                const taa_assetpack_packed* packed = &pack->packed;
                uint32_t size = file->size;
                fseek(packed->fp, file->offset, SEEK_SET);
                if(size > data->size)
                {
                    data->size = size;
                    data->buf = realloc(data->buf, size);
                }
                if(fread(data->buf, 1, size, packed->fp) == size)
                {
                    req->buf = data->buf;
                    req->size = size;
                    req->result = 0;
                    data = NULL;
                }
            }
            taa_assetstream_pushreq(&stream->callbacklist, req);
            req = next;
        }
        else
        {
            // if there was nothing to process, wait for a post
            taa_semaphore_wait(&stream->sem);
        }
    }
    // unlock the data that was reserved for the next request
    if(data != NULL)
    {
        data->locked = 0;
    }
    // make sure all aborted requests are put into callback list
    do
    {
        while(req != NULL)
        {
            taa_assetstream_req* next = req->next;
            req->buf = NULL;
            req->size = 0;
            req->result = -1;
            taa_assetstream_pushreq(&stream->callbacklist, req);
            req = next;
        }
        if(req == NULL)
        { 
            req = taa_assetstream_popreq(&stream->loadlist);
        }
    }
    while(req != NULL);
    return 0;
}

//****************************************************************************
void taa_assetstream_create(
    taa_assetstream** streamout)
{
    taa_assetstream* stream = (taa_assetstream*)calloc(1,sizeof(*stream));
    taa_semaphore_create(&stream->sem);
    taa_thread_create(taa_assetstream_thread, stream, &stream->thread);
    taa_assetstream_allocreq(stream);
    *streamout = stream;
}

//****************************************************************************
void taa_assetstream_destroy(
    taa_assetstream* stream)
{
    taa_assetstream_data* data = stream->data;
    taa_assetstream_data* dataend = data+sizeof(stream->data)/sizeof(*data);
    taa_assetstream_reqchunk* chunk = stream->chunks;
    // tell the thread to quit
    stream->quit = 1;
    taa_semaphore_post(&stream->sem);
    taa_thread_join(&stream->thread);
    // make sure any remaining requests get handled
    taa_assetstream_update(stream);
    // clean up
    while(data != dataend)
    {
        assert(data->locked == 0);
        free(data->buf);
        ++data;
    }
    while(chunk != NULL)
    {
        taa_assetstream_reqchunk* next = chunk->next;
        free(chunk);
        chunk = next;
    }
    taa_semaphore_destroy(&stream->sem);
    free(stream);
}

//****************************************************************************
void taa_assetstream_load(
    taa_assetstream* stream,
    const taa_assetpack* pack,
    uint32_t file,
    taa_assetstream_callback callback,
    void* userdata)
{
    // acquire a request node
    taa_assetstream_req* req = stream->pool;
    taa_assetstream_req* prev;
    do
    {
        if(req == NULL)
        {
            taa_assetstream_allocreq(stream);
            // this is the only thread that will remove from pool
            req = stream->pool;
        }
        prev = req;
        req = taa_ATOMIC_CMPXCHGPTR(&stream->pool, req, req->next);
    }
    while(prev != req);
    // set its members
    req->pack = pack;
    req->file = file;
    req->callback = callback;
    req->userdata = userdata;
    // push it to the load list
    taa_assetstream_pushreq(&stream->loadlist, req);
    // post a signal to the load thread
    taa_semaphore_post(&stream->sem);
}

//****************************************************************************
void taa_assetstream_release(
    taa_assetstream* stream,
    const void* buf)
{
    taa_assetstream_data* data = stream->data;
    taa_assetstream_data* dataend = data+sizeof(stream->data)/sizeof(*data);
    while(data != dataend)
    {
        if(data->buf == buf)
        {
            assert(data->locked == 1);
            data->locked = 0;
            // make sure the load thread knows it's available
            taa_semaphore_post(&stream->sem);
            break;
        }
        ++data;
    }
    assert(data != dataend);
}

//****************************************************************************
void taa_assetstream_update(
    taa_assetstream* stream)
{
    taa_assetstream_req* req;
    req = taa_assetstream_popreq(&stream->callbacklist);
    while(req != NULL)
    {
        taa_assetstream_req* next = req->next;
        // perform callback
        req->callback(req->buf, req->size, req->result, req->userdata);
        // put request back in pool
        req->buf = NULL;
        taa_assetstream_pushreq(&stream->pool, req);
        req = next;
    }
}
