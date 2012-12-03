/**
 * @brief     asset file storage in directory structure implementation
 * @author    Thomas Atwood (tatwood.net)
 * @date      2011
 * @copyright unlicense / public domain
 ****************************************************************************/
#include <taa/assetdir.h>
#include <taa/path.h>
#include <taa/semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct taa_assetdir_buf_s taa_assetdir_buf;
typedef struct taa_assetdir_strings_s taa_assetdir_strings;
typedef struct taa_assetdir_s taa_assetdir;

struct taa_assetdir_buf_s
{
    void* data;
    taa_semaphore* sem;
    uint32_t size;
    uint32_t capacity;
    taa_asset_parse_func parsefunc;
    void* userdata;
};

struct taa_assetdir_strings_s
{
    int32_t offset;
    taa_assetdir_strings* next;
    char buf[2048];
};

struct taa_assetdir_s
{
    taa_asset_group group;
    taa_asset_dir_storage* mgr;
    taa_assetdir* next;
};

struct taa_asset_dir_storage_s
{
    taa_semaphore sem;
    uint32_t numbuffers;
    taa_assetdir_buf* buffers;
    taa_assetdir* dirs;
    taa_assetdir_strings* stringbuf;
};

//****************************************************************************
// copies contents of a string to a duplicate allocated from the string table
static const char* taa_assetdir_strdup(
    taa_asset_dir_storage* mgr,
    const char* src)
{
    taa_assetdir_strings* strbuf = mgr->stringbuf;
    uint32_t sz = strlen(src) + 1;
    char* dst;
    while(strbuf != NULL)
    {
        // try to find space for the string in an existing string table
        if(strbuf->offset + sz <= sizeof(strbuf->buf))
        {
            break;
        }
        strbuf = strbuf->next;
    }
    if(strbuf == NULL)
    {
        // if no string tables exist with enough space, allocate one
        strbuf = (taa_assetdir_strings*) malloc(sizeof(*strbuf));
        strbuf->offset = 0;
        strbuf->next = mgr->stringbuf;
        mgr->stringbuf = strbuf;
    }
    dst = strbuf->buf + strbuf->offset;
    strbuf->offset += sz;
    memcpy(dst, src, sz);
    return dst;
}

//****************************************************************************
// called on one of the workqueue threads to execute the parse function
static void taa_assetdir_parse(
    void* userdata)
{
    taa_assetdir_buf* buf = (taa_assetdir_buf*) userdata;
    // execute the specified function to parse it
    buf->parsefunc(buf->data, buf->size, buf->userdata);
    // instruct the storage thread that we're done with the buffer
    buf->parsefunc = NULL;
    taa_semaphore_post(buf->sem);
}

//****************************************************************************
// called on the storage thread to load the contents of a set of files
static void taa_assetdir_load(
    taa_asset_group* group,
    taa_asset_file_request* requests)
{
    taa_assetdir* dir = (taa_assetdir*) group;
    taa_asset_dir_storage* mgr = dir->mgr;
    taa_asset_file_request* req = requests;
    while(req != NULL)
    {
        taa_asset_file* file = req->file;
        taa_assetdir_buf* buf = NULL;
        uint32_t sz = file->size;
        uint32_t cap = 0;
        FILE* fp;
        // find a buffer to read the data into
        while(1)
        {
            taa_assetdir_buf* bufitr = mgr->buffers;
            taa_assetdir_buf* bufend = bufitr + mgr->numbuffers;
            while(bufitr != bufend)
            {
                // if the buffer is not in use
                if(bufitr->parsefunc == NULL)
                {
                    // if previous buffer is too small and new one is bigger
                    // select the new one
                    if(buf == NULL || (cap < sz && bufitr->capacity > cap))
                    {
                        buf = bufitr;
                        cap =buf->capacity;
                    }
                }
                ++bufitr;
            }
            // if a buffer was found, stop searching
            if(buf != NULL)
            {
                break;
            }
            // if all the buffers are in use, wait for a signal
            taa_semaphore_wait(&mgr->sem);
        }
        // if the selected buffer is too small, resize it
        if(cap < sz)
        {
            // fit to the nearest 65k
            enum { MEM_CHUNK = 65536 };
            cap = (sz+(MEM_CHUNK-1)) & ~(MEM_CHUNK-1);
            buf->data = realloc(buf->data, cap);
            buf->capacity = cap;
        }
        // attempt to load the file
        fp = fopen((const char*) file->handle, "rb");
        if(fp != NULL)
        {
            if(fread(buf->data, 1, sz, fp) != sz)
            {
                sz = 0;
            }
            fclose(fp);
        }
        else
        {
            sz = 0;
        }
        // queue the data to be processed on another thread
        buf->sem = &mgr->sem;
        buf->size = sz;
        buf->parsefunc = req->parsefunc;
        buf->userdata = req->userdata;
        taa_workqueue_push(req->workqueue, taa_assetdir_parse,buf);
        req = req->next;
    }
}

//****************************************************************************
void taa_asset_create_dir_storage(
    uint32_t maxrequests,
    taa_asset_dir_storage** mgr_out)
{
    taa_asset_dir_storage* mgr;
    taa_assetdir_buf* buf;
    uintptr_t offset;
    // determine buffer size and pointer offsets
    offset = 0;
    mgr = (taa_asset_dir_storage*) offset;
    offset = (uintptr_t) (mgr + 1);
    buf = (taa_assetdir_buf*) taa_ALIGN_PTR(offset, 8);
    offset = (uintptr_t) (buf + maxrequests);
    // allocate the buffer and adjust pointers
    offset = (uintptr_t) calloc(1, offset);
    mgr = (taa_asset_dir_storage*) (((uintptr_t) mgr) + offset);
    buf = (taa_assetdir_buf*) (((uintptr_t) buf) + offset);
    // initialize manager struct
    taa_semaphore_create(&mgr->sem);
    mgr->buffers = buf;
    mgr->numbuffers = maxrequests;
    *mgr_out = mgr;
}

//****************************************************************************
void taa_asset_destroy_dir_storage(
    taa_asset_dir_storage* mgr)
{
    taa_assetdir* dir = mgr->dirs;
    taa_assetdir_buf* buf = mgr->buffers;
    taa_assetdir_buf* bufend = buf + mgr->numbuffers;
    taa_assetdir_strings* strbuf = mgr->stringbuf;
    taa_semaphore_destroy(&mgr->sem);
    while(dir != NULL)
    {
        taa_assetdir* next = dir->next;
        free(dir);
        dir = next;
    }
    while(buf != bufend)
    {
        free(buf->data);
        ++buf;
    }
    while(strbuf != NULL)
    {
        taa_assetdir_strings* next = strbuf->next;
        free(strbuf);
        strbuf = next;
    }
    free(mgr);
}

//****************************************************************************
taa_asset_group* taa_asset_scan_dir(
    taa_asset_dir_storage* mgr,
    const char* name,
    const char* path)
{
    taa_asset_group* group = NULL;
    uint32_t n = 0;
    taa_dir sysdir;
    // loop through and count the files in the directory
    if(taa_opendir(path, &sysdir) == 0)
    {
        const char* fname = taa_readdir(sysdir);
        while(fname != NULL)
        {
            if(strcmp(fname,".") && strcmp(fname, ".."))
            {
                struct taa_stat stat;
                char fpath[taa_PATH_SIZE];
                // get the full path to the file
                taa_path_set(fpath, sizeof(fpath), path);
                taa_path_append(fpath, sizeof(fpath), fname);
                if(taa_stat(fpath, &stat) == 0)
                {
                    if(stat.st_mode == taa_S_IFREG)
                    {
                        ++n;
                    }
                }
            }
            fname = taa_readdir(sysdir);
        }
        taa_closedir(sysdir);
    }
    // add the files
    if(n > 0 && (taa_opendir(path, &sysdir) == 0))
    {
        uintptr_t offset;
        taa_assetdir* stordir;
        taa_asset_file* file;
        taa_asset_file* fileend;
        const char* fname;
        // determine buffer size and pointer offsets
        offset = 0;
        stordir = (taa_assetdir*) offset;
        offset = (uintptr_t) (stordir + 1);
        file = (taa_asset_file*) offset;
        offset = (uintptr_t) (file + n);
        // allocate the buffer and adjust pointers
        offset = (uintptr_t) malloc(offset);
        stordir = (taa_assetdir*) (((uintptr_t) stordir) + offset);
        file = (taa_asset_file*) (((uintptr_t) file) + offset);
        group = &stordir->group;
        fileend = file + n;
        // initialize directory struct and add to manager
        stordir->mgr = mgr;
        stordir->next = mgr->dirs;
        mgr->dirs = stordir;
        // initialize storage struct
        group->name = taa_assetdir_strdup(mgr, name);
        group->key = taa_asset_gen_groupkey(name);
        group->numfiles = 0;
        group->files = file;
        group->loadfunc = taa_assetdir_load;
        // add files
        fname = taa_readdir(sysdir);
        while(file != fileend && fname != NULL)
        {
            if(strcmp(fname,".") && strcmp(fname, ".."))
            {
                struct taa_stat stat;
                char fpath[taa_PATH_SIZE];
                // get the full path to the file
                taa_path_set(fpath, sizeof(fpath), path);
                taa_path_append(fpath, sizeof(fpath), fname);
                if(taa_stat(fpath, &stat) == 0)
                {
                    if(stat.st_mode == taa_S_IFREG)
                    {
                        uintptr_t h;
                        h = (uintptr_t) taa_assetdir_strdup(mgr,fpath);
                        file->name = taa_assetdir_strdup(mgr, fname);
                        file->typekey=taa_asset_gen_typekey(fname);
                        file->filekey=taa_asset_gen_filekey(fname);
                        file->size = stat.st_size;
                        file->handle = h;
                        // TODO: check for hash conflicts?
                        ++file;
                    }
                }
            }
            fname = taa_readdir(sysdir);
        }
        taa_closedir(sysdir);
        group->numfiles = file - group->files;
    }
    return group;
}
