/**
 * @brief     file group implementation
 * @author    Thomas Atwood (tatwood.net)
 * @date      2011
 * @copyright unlicense / public domain
 ****************************************************************************/
#include <taaasset/assetpack.h>
#include <taa/error.h>
#include <taa/hash.h>
#include <taa/path.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

//****************************************************************************
static uint32_t taa_assetpack_search(
    const taa_assetpack_file* files,
    uint32_t numfiles,
    uint32_t typehash,
    uint32_t filehash)
{
    uint32_t lo = 0;
    uint32_t hi = numfiles;
    while(lo < hi)
    {
        uint32_t i = lo + ((hi-lo) >> 1);
        const taa_assetpack_file* f = files + i;
        if(f->filehash < filehash)
        {
            lo = i + 1;
        }
        else if(f->filehash == filehash && f->typehash < typehash)
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
static void taa_assetpack_insert(
    taa_assetpack_loose* pack,
    uint32_t typehash,
    uint32_t filehash,
    const char* path,
    uint32_t size)
{
    enum { FILE_CHUNK = 32 };
    enum { PATH_CHUNK = 1024 };
    uint32_t i;
    uint32_t n;
    uint32_t cap;
    uint32_t ncap;
    taa_assetpack_file* file;

    n = pack->numfiles;
    cap = (n + (FILE_CHUNK-1)) & ~(FILE_CHUNK-1);
    ncap = (n + (FILE_CHUNK)) & ~(FILE_CHUNK-1);
    if(ncap > cap)
    {
        pack->files = (taa_assetpack_file*) realloc(
            pack->files,
            ncap * sizeof(*pack->files));
    }

    i = taa_assetpack_search(pack->files,pack->numfiles,typehash,filehash);
    file = pack->files + i;
    if(i != n)
    {
        if((filehash != file->filehash || typehash != file->typehash))
        {
            taa_assetpack_file* fileitr = pack->files + n;
            taa_assetpack_file* fileend = file;
            while(fileitr != fileend)
            {
                *fileitr = *(fileitr - 1);
                --fileitr;
            }
        }
        else
        {
            taa_CHECK_ERROR1(0, "duplicate file id %x", filehash);
        }
    }
    file->typehash = typehash;
    file->filehash = filehash;
    file->offset = pack->pathbufsize;
    file->size = size;

    n = strlen(path) + 1;
    cap = (pack->pathbufsize + (PATH_CHUNK-1)) & ~(PATH_CHUNK-1);
    ncap = (pack->pathbufsize + n + (PATH_CHUNK-1)) & ~(PATH_CHUNK-1);
    if(ncap > cap)
    {
        pack->pathbuf = (char*) realloc(
            pack->pathbuf,
            ncap * sizeof(*pack->pathbuf));                
    }
    memcpy(pack->pathbuf + file->offset, path, n); 
    pack->pathbufsize += n;

    ++pack->numfiles;
}

//****************************************************************************
static void taa_assetpack_createloose(
    const char* name,
    const char* dir,
    taa_assetpack* packout)
{
    taa_dir sysdir;
    memset(packout, 0, sizeof(*packout));
    packout->base.type = taa_ASSETPACK_LOOSE;
    packout->base.namehash = taa_assetpack_hashpackname(name);
    if(taa_opendir(dir, &sysdir) == 0)
    {
        const char* fname = taa_readdir(&sysdir);
        while(fname != NULL)
        {
            if(strcmp(fname,".") && strcmp(fname, ".."))
            {
                taa_filestat stat;
                char fpath[taa_PATH_SIZE];

                taa_path_set(fpath, sizeof(fpath), dir);
                taa_path_append(fpath, sizeof(fpath), fname);

                if(taa_getfilestat(fpath, &stat) == 0)
                {
                    if(stat.mode == taa_FILE_IFREG)
                    {
                        uint32_t typehash=taa_assetpack_hashfiletype(fname);
                        uint32_t filehash=taa_assetpack_hashfilename(fname);
                        taa_assetpack_insert(
                            &packout->loose,
                            typehash,
                            filehash,
                            fpath,
                            stat.size);
                    }
                }
            }
            fname = taa_readdir(&sysdir);
        }
        taa_closedir(&sysdir);
    }
}

//****************************************************************************
void taa_assetpack_createlist(
    taa_assetpack_list* listout)
{
    memset(listout, 0, sizeof(*listout));
}

//****************************************************************************
void taa_assetpack_destroylist(
    taa_assetpack_list* list)
{
    taa_assetpack* packitr = list->packs;
    taa_assetpack* packend = packitr + list->numpacks;
    while(packitr != packend)
    {
        free(packitr->base.files);
        switch(packitr->base.type)
        {
        case taa_ASSETPACK_LOOSE:
            free(packitr->loose.pathbuf);
            break;
        case taa_ASSETPACK_PACKED:
            fclose(packitr->packed.fp);
            break;
        }
        ++packitr;
    }
    free(list->packs);
}

//****************************************************************************
int32_t taa_assetpack_find(
    const taa_assetpack* pack,
    uint32_t typehash,
    uint32_t filehash)
{
    int32_t result = -1;
    uint32_t i = taa_assetpack_search(
        pack->base.files,
        pack->base.numfiles,
        typehash,
        filehash);
    if(i < pack->base.numfiles)
    {
        const taa_assetpack_file* f = pack->base.files + i;
        if(f->filehash == filehash && f->typehash == typehash)
        {
            result = i;
        }
    }
    return result;
}

//****************************************************************************
uint32_t taa_assetpack_hashfilename(
    const char* name)
{
    char fmtname[taa_PATH_SIZE];
    char* dstitr = fmtname;
    char* dstend = fmtname + sizeof(fmtname);
    const char* srcitr;
    const char* srcend;
    srcitr = strrchr(name, taa_PATH_SLASH);
    srcitr = (srcitr != NULL) ? srcitr + 1 : name;
    srcend = strchr(srcitr, '.');
    srcend = (srcend != NULL) ? srcend + 1 : srcitr + strlen(srcitr) + 1;
    while(dstitr != dstend && srcitr != srcend)
    {
        *dstitr = tolower(*srcitr);
        ++dstitr;
        ++srcitr;
    }
    --dstitr;
    *dstitr = '\0';
    return taa_hash_pjw(fmtname);
}

//****************************************************************************
uint32_t taa_assetpack_hashfiletype(
    const char* ext)
{
    char fmtext[taa_PATH_SIZE];
    char* dstitr = fmtext;
    char* dstend = fmtext + sizeof(fmtext);
    const char* srcitr;
    const char* srcend;
    srcitr = strrchr(ext, '.');
    srcitr = (srcitr != NULL) ? srcitr + 1 : ext;
    srcend = srcitr + strlen(srcitr) + 1;
    while(dstitr != dstend && srcitr != srcend)
    {
        *dstitr = tolower(*srcitr);
        ++dstitr;
        ++srcitr;
    }
    --dstitr;
    *dstitr = '\0';
    return taa_hash_pjw(fmtext);
}

//****************************************************************************
uint32_t taa_assetpack_hashpackname(
    const char* name)
{
    char fmtname[taa_PATH_SIZE];
    char* dstitr = fmtname;
    char* dstend = fmtname + sizeof(fmtname);
    const char* srcitr = name;
    const char* srcend = srcitr + strlen(srcitr) + 1;
    while(dstitr != dstend && srcitr != srcend)
    {
        *dstitr = tolower(*srcitr);
        ++dstitr;
        ++srcitr;
    }
    --dstitr;
    *dstitr = '\0';
    return taa_hash_pjw(fmtname);
}

//****************************************************************************
void taa_assetpack_scan(
    taa_assetpack_list* list,
    const char* path)
{
    taa_assetpack* p = list->packs;
    uint32_t n = list->numpacks;
    taa_dir dir;
    if(taa_opendir(path, &dir) == 0)
    {
        const char* entry = taa_readdir(&dir);
        while(entry != NULL)
        {
            char entrypath[taa_PATH_SIZE];
            taa_filestat fstat;

            taa_path_set(entrypath, sizeof(entrypath), path);
            taa_path_append(entrypath, sizeof(entrypath), entry);
            if(taa_getfilestat(entrypath, &fstat) == 0)
            {
                if(fstat.mode == taa_FILE_IFDIR)
                {
                    p = (taa_assetpack*) realloc(p, (n+1)*sizeof(*p));
                    taa_assetpack_createloose(
                        entry,
                        entrypath,
                        p + n);
                    ++n;
                }
            }
            entry = taa_readdir(&dir);
        }
        taa_closedir(&dir);
    }
    list->packs = p;
    list->numpacks = n;
}
