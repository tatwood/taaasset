#ifndef DEBUGFONT_H_
#define DEBUGFONT_H_

#include <taa/gl.h>

typedef struct debugfont_vert_s debugfont_vert;

enum
{
    DEBUGFONT_CHAR_WIDTH  = 8,
    DEBUGFONT_CHAR_HEIGHT = 14
};

struct debugfont_vert_s
{
    float pos[2];
    float uv[2];
    unsigned color;
};

void debugfont_init(
    taa_texture2d tex2d);

unsigned debugfont_gen_vertices(
    const char* str,
    unsigned vieww,
    unsigned viewh,
    int x,
    int y,
    unsigned color,
    debugfont_vert* verts_out,
    unsigned maxverts);

#endif // DEBUGFONT_H_
