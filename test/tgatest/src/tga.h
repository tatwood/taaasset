#ifndef TGA_H_
#define TGA_H_

#include <stddef.h>

//****************************************************************************
// macros

#define tga_GET_VFLIP(pheader_) \
    (((pheader_)->descriptor & (1 << 5)) >> 5)
    
#define tga_SET_VFLIP(pheader_, vflip_) \
    ((pheader_)->descriptor |= (vflip_ & 1) << 5)
    
#define tga_GET_ALPHA(pheader_) \
    ((pheader_)->descriptor & 15)
    
#define tga_SET_ALPHA(pheader_, alphabits_) \
    ((pheader_)->descriptor |= (alphabits_) & 15)
    
//****************************************************************************
// enums

enum
{
    tga_HEADER_SIZE = 18
};

enum tga_imagetype_e
{
    tga_TYPE_TRUECOLOR = 2,
    tga_TYPE_GREY      = 3
};

//****************************************************************************
// typedefs

typedef struct tga_header_s tga_header;

//****************************************************************************
// structs

struct tga_header_s
{
   unsigned char  idsize;
   unsigned char  colourmaptype;
   unsigned char  imagetype;
   unsigned short colourmaporigin;
   unsigned short colourmaplength;
   unsigned char  colourmapbits;
   unsigned short x_origin;
   unsigned short y_origin;
   unsigned short width;
   unsigned short height;
   unsigned char  bitsperpixel;
   unsigned char  descriptor;
};

//****************************************************************************
// functions

/**
 * @brief initializes tga header struct based on specified values
 */
void tga_init(
    unsigned short width,
    unsigned short height,
    unsigned char bitsperpixel,
    unsigned char vflip,
    tga_header* header_out);

/**
 * @brief deserializes tga header struct from data buffer
 * @param data the serialized source buffer
 * @param header_out tga address that will be filled with deserialized header
 * @param image_out address that will be filled with offset of image data from
 *        beginning of source data
 */
void tga_read(
    const unsigned char hbuf[tga_HEADER_SIZE],
    tga_header* header_out,
    size_t* image_out);

/**
 * @brief serializes tga header struct to data buffer
 * @param data the serialized source buffer
 * @param header_out tga address that will be filled with deserialized header
 * @param image_out address that will be filled with offset of image data from
 *        beginning of source data
 * @return 0 on success, -1 on error
 */
void tga_write(
    const tga_header* header,
    unsigned char hbuf_out[tga_HEADER_SIZE]);

#endif // TGA_H_
