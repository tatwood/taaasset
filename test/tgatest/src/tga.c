#include "tga.h"
#include <stdlib.h>
#include <string.h>

//*****************************************************************************
void tga_init(
    unsigned short width,
    unsigned short height,
    unsigned char bitsperpixel,
    unsigned char vflip,
    tga_header* header_out)
{
    header_out->idsize         = 0;
    header_out->colourmaptype  = 0;
    header_out->imagetype      = (bitsperpixel != 8) ? 2 : 3;
    header_out->colourmaporigin= 0;
    header_out->colourmaplength= 0;
    header_out->colourmapbits  = 0;
    header_out->x_origin       = 0;
    header_out->y_origin       = 0;
    header_out->width          = width;
    header_out->height         = height;
    header_out->bitsperpixel   = bitsperpixel;
    header_out->descriptor     = 0;
    tga_SET_VFLIP(header_out, vflip);
    tga_SET_ALPHA(header_out, (bitsperpixel == 32) ? 8 : 0);
}

//*****************************************************************************
void tga_read(
    const unsigned char hbuf[tga_HEADER_SIZE],
    tga_header* header_out,
    size_t* image_out)
{
    header_out->idsize         = hbuf[ 0];
    header_out->colourmaptype  = hbuf[ 1];
    header_out->imagetype      = hbuf[ 2];
    header_out->colourmaporigin= hbuf[ 3] | (((unsigned short)hbuf[ 4]) << 8);
    header_out->colourmaplength= hbuf[ 5] | (((unsigned short)hbuf[ 6]) << 8);
    header_out->colourmapbits  = hbuf[ 7];
    header_out->x_origin       = hbuf[ 8] | (((unsigned short)hbuf[ 9]) << 8);
    header_out->y_origin       = hbuf[10] | (((unsigned short)hbuf[11]) << 8);
    header_out->width          = hbuf[12] | (((unsigned short)hbuf[13]) << 8);
    header_out->height         = hbuf[14] | (((unsigned short)hbuf[15]) << 8);
    header_out->bitsperpixel   = hbuf[16];
    header_out->descriptor     = hbuf[17];
    // calculate image offset
    *image_out = 
        tga_HEADER_SIZE +
        header_out->idsize +
        header_out->colourmaplength;
}

//*****************************************************************************
void tga_write(
    const tga_header* header,
    unsigned char hbuf_out[tga_HEADER_SIZE])
{
    hbuf_out[ 0] = header->idsize;
    hbuf_out[ 1] = header->colourmaptype;
    hbuf_out[ 2] = header->imagetype;
    hbuf_out[ 3] = (unsigned char) ((header->colourmaporigin & 0x00ff)     );
    hbuf_out[ 4] = (unsigned char) ((header->colourmaporigin & 0xff00) >> 8);
    hbuf_out[ 5] = (unsigned char) ((header->colourmaplength & 0x00ff)     );
    hbuf_out[ 6] = (unsigned char) ((header->colourmaplength & 0xff00) >> 8);
    hbuf_out[ 7] = header->colourmapbits;
    hbuf_out[ 8] = (unsigned char) ((header->x_origin        & 0x00ff)     );
    hbuf_out[ 9] = (unsigned char) ((header->x_origin        & 0xff00) >> 8);
    hbuf_out[10] = (unsigned char) ((header->y_origin        & 0x00ff)     );
    hbuf_out[11] = (unsigned char) ((header->y_origin        & 0xff00) >> 8);
    hbuf_out[12] = (unsigned char) ((header->width           & 0x00ff)     );
    hbuf_out[13] = (unsigned char) ((header->width           & 0xff00) >> 8);
    hbuf_out[14] = (unsigned char) ((header->height          & 0x00ff)     );
    hbuf_out[15] = (unsigned char) ((header->height          & 0xff00) >> 8);
    hbuf_out[16] = header->bitsperpixel;
    hbuf_out[17] = header->descriptor;
}
