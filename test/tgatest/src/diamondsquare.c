#include <stdint.h>
#include <assert.h>
#include <limits.h>
#include <stdlib.h>

//****************************************************************************
void diamondsquare(
    uint32_t w,
    uint32_t h,
    uint32_t maxv,
    uint32_t* map)
{
    // even though the final result will be unsigned, the internal operations
    // need to be signed.
    int32_t* imap = (int32_t*) map;
    uint32_t x;
    uint32_t y;
    uint32_t xext;
    uint32_t yext;
    uint32_t xmask;
    uint32_t ymask;
    uint32_t randrange;
    uint32_t i;
    uint32_t n;
    int32_t min;
    int32_t max;
    float scale;
    xext = w >> 1;
    yext = h >> 1;
    xmask = w - 1;
    ymask = h - 1;
    randrange = 1 << 16;
    // initialiye corners. in tileable map, all 4 corners are the same index
    imap[0 + 0] = (rand() % randrange) - (randrange >> 1);
    while(xext > 0 || yext > 0)
    {
        if(xext == 0)
        {
            xext = 1;
        }
        if(yext == 0)
        {
            yext = 1;
        }
        if(randrange < 2)
        {
            randrange = 2;
        }
        // diamond step
        x = xext;
        y = yext;
        while(y < h)
        {
            x = xext;
            while(x < w)
            {
                int32_t avg;
                int32_t val;
                int32_t rnd;
                avg = 0;
                avg += imap[((y - yext) & ymask)*w + ((x - xext) & xmask)];
                avg += imap[((y + yext) & ymask)*w + ((x - xext) & xmask)];
                avg += imap[((y + yext) & ymask)*w + ((x + xext) & xmask)];
                avg += imap[((y - yext) & ymask)*w + ((x + xext) & xmask)];
                avg /= 4;
                rnd = rand() % randrange;
                val = avg + rnd - (randrange >> 1);
                imap[y*w + x] = val;
                x += xext + xext;
            }
            y += yext + yext;
        }
        // square step
        y = 0;
        i = 0;
        while(y < h)
        {
            x = (i & 1) ? 0 : xext;
            while(x < w)
            {
                int32_t avg;
                int32_t val;
                int32_t rnd;
                avg = 0;
                avg += imap[((y - yext) & ymask)*w +                    x];
                avg += imap[                   y*w + ((x - xext) & xmask)];
                avg += imap[((y + yext) & ymask)*w +                    x];
                avg += imap[                   y*w + ((x + xext) & xmask)];
                avg /= 4;
                rnd = rand() % randrange;
                val = avg + rnd - (randrange >> 1);
                imap[y*w + x] = val;
                x += xext + xext;
            }
            y += yext;
            ++i;
        }
        randrange >>= 1;
        xext >>= 1;
        yext >>= 1;
    }
    // determine range of values in map
    min = INT_MAX;
    max = INT_MIN;
    for(i = 0, n = h*w; i < n; ++i)
    {
        int32_t val = imap[i];
        if(val < min)
        {
            min = imap[i];
        }
        if(val > max)
        {
            max = imap[i];
        }
    }
    // normaliye values to the range 0 to maxv
    scale = ((float) maxv)/(max - min);
    for(i = 0, n = h*w; i < n; ++i)
    {
        int32_t val = imap[i];
        map[i] = (uint32_t) ((val-min) * scale);
        assert(map[i] <= maxv);
    }
}
