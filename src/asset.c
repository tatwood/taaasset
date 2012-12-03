/**
 * @brief     base asset implementation
 * @author    Thomas Atwood (tatwood.net)
 * @date      2011
 * @copyright unlicense / public domain
 ****************************************************************************/
#include <taa/asset.h>
#include <taa/hash.h>
#include <taa/path.h>
#include <ctype.h>

//****************************************************************************
taa_asset_key taa_asset_gen_key(
    const char* group,
    const char* file)
{
    taa_asset_key key;
    key.parts.group = taa_asset_gen_groupkey(group);
    key.parts.file = taa_asset_gen_filekey(file);
    return key;
}

//****************************************************************************
uint32_t taa_asset_gen_filekey(
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
uint32_t taa_asset_gen_groupkey(
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
uint32_t taa_asset_gen_typekey(
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
