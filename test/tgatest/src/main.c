#if defined(_DEBUG) && defined(_MSC_FULL_VER)
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif

#include "debugfont.h"
#include "tgaasset.h"
#include "tga.h"
#include <taa/keyboard.h>
#include <taa/mouse.h>
#include <taa/path.h>
#include <taa/glcontext.h>
#include <taa/thread.h>
#include <taa/timer.h>
#include <taa/assetdir.h>
#include <GL/gl.h>
#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { IMAGE_WIDTH = 512 };
enum { IMAGE_HEIGHT = 512 };
enum { NUM_IMAGES = 64 };
enum { NUM_WORKER_THREADS = 2 };
enum { NUM_BOXES = 8 };

void diamondsquare(
    uint32_t w,
    uint32_t h,
    uint32_t maxv,
    uint32_t* map);
    
static void populate_asset_dir(
    const char* rootdir,
    const char* assetdir)
{
    uint32_t* hmap;
    uint32_t* smap;
    uint32_t* vmap;
    uint8_t* image;
    int32_t i;
    tga_header tga;
    tga_init(IMAGE_WIDTH, IMAGE_HEIGHT, 32, 0, &tga);

    hmap = (uint32_t*) malloc(IMAGE_WIDTH * IMAGE_HEIGHT * sizeof(*hmap));
    smap = (uint32_t*) malloc(IMAGE_WIDTH * IMAGE_HEIGHT * sizeof(*hmap));
    vmap = (uint32_t*) malloc(IMAGE_WIDTH * IMAGE_HEIGHT * sizeof(*hmap));
    image = (uint8_t*) malloc(IMAGE_WIDTH * IMAGE_HEIGHT * 4);
    for(i = 0; i < NUM_IMAGES; ++i)
    {
        char fname[16];
        char path[taa_PATH_SIZE];
        FILE* fp;        
        int32_t j;
        diamondsquare(IMAGE_WIDTH, IMAGE_HEIGHT, 0xffff, hmap);
        diamondsquare(IMAGE_WIDTH, IMAGE_HEIGHT, 0xffff, smap);
        diamondsquare(IMAGE_WIDTH, IMAGE_HEIGHT, 0xffff, vmap);
        // convert the hsv data into bgra
        for(j = 0; j < IMAGE_WIDTH * IMAGE_HEIGHT; ++j)
        {
            float b, g, r;
            float h, s, v;
            float h1, c, x, m;
            h = hmap[j] * (360.0f/0xffff); // hue in range 0 - 360
            s = smap[j] * (  1.0f/0xffff); // sat in range 0 - 1
            v = vmap[j] * (  1.0f/0xffff); // val in range 0 - 1
            s = s * 0.5f + 0.5f;
            c = v * s; // chroma
            h1 = h/60.0f; // h'
            x = c * (1.0f - (float) fabs(fmod(h1, 2) - 1.0f));
            if(h1 < FLT_EPSILON)
            {
                r = 0; g = 0; b = 0;
            }
            else if(h1 < 1.0f)
            {
                r = c; g = x; b = 0;
            }
            else if(h1 < 2.0f)
            {
                r = x; g = c; b = 0;
            }
            else if(h1 < 3.0f)
            {
                r = 0; g = c; b = x;
            }
            else if(h1 < 4.0f)
            {
                r = 0; g = x; b = c;
            }
            else if(h1 < 5.0f)
            {
                r = x; g = 0; b = c;
            }
            else
            {
                r = c; g = 0; b = x;            
            }
            m = v - c;
            image[j*4+0] = (uint8_t) ((b + m) * 0xff);
            image[j*4+1] = (uint8_t) ((g + m) * 0xff);
            image[j*4+2] = (uint8_t) ((r + m) * 0xff);
            image[j*4+3] = 0xff;
        }
        sprintf(fname, "%i.tga", i);
        taa_path_set(path, sizeof(path), rootdir);
        taa_path_append(path, sizeof(path), assetdir);
        taa_path_append(path, sizeof(path), fname);
        fp = fopen(path, "wb");
        if(fp != NULL)
        {
            unsigned char hbuf[tga_HEADER_SIZE];
            tga_write(&tga, hbuf);
            fwrite(hbuf, 1, sizeof(hbuf), fp);
            fwrite(image, 1, IMAGE_WIDTH*IMAGE_HEIGHT*4, fp);
            assert(ftell(fp) == (tga_HEADER_SIZE+IMAGE_WIDTH*IMAGE_HEIGHT*4));
            fclose(fp);
        }
    }
    free(image);
    free(vmap);
    free(smap);
    free(hmap);
}

static taa_asset_group* scan_asset_dir(
    taa_asset_dir_storage* dirmgr,
    const char* rootdir,
    const char* assetdir)
{
    char path[taa_PATH_SIZE];
    taa_path_set(path, sizeof(path), rootdir);
    taa_path_append(path, sizeof(path), assetdir);
    return taa_asset_scan_dir(dirmgr, assetdir, path);
}

typedef struct main_win_s main_win;

struct main_win_s
{
    taa_window_display windisplay;
    taa_window win;
    taa_glcontext_display rcdisplay;
    taa_glcontext_surface rcsurface;
    taa_glcontext rc;
};

//****************************************************************************
static int main_init_window(
    main_win* mwin)
{
    int err = 0;
    int rcattribs[] =
    {
        taa_GLCONTEXT_BLUE_SIZE   ,  8,
        taa_GLCONTEXT_GREEN_SIZE  ,  8,
        taa_GLCONTEXT_RED_SIZE    ,  8,
        taa_GLCONTEXT_DEPTH_SIZE  , 24,
        taa_GLCONTEXT_STENCIL_SIZE,  8,
        taa_GLCONTEXT_NONE
    };
    taa_glcontext_config rcconfig;
    memset(mwin, 0, sizeof(*mwin));
    mwin->windisplay = taa_window_open_display();
    err = (mwin->windisplay != NULL) ? 0 : -1;
    if(err == 0)
    {
        err = taa_window_create(
            mwin->windisplay,
            "tga test",
            720,
            405,
            taa_WINDOW_FULLSCREEN,
            &mwin->win);
    }
    if(err == 0)
    {
        mwin->rcdisplay = taa_glcontext_get_display(mwin->windisplay);
        err = (mwin->rcdisplay != NULL) ? 0 : -1;
    }
    if(err == 0)
    {
        err = (taa_glcontext_initialize(mwin->rcdisplay)) ? 0 : -1;
    };
    if(err == 0)
    {
        int numconfig = 0;
        taa_glcontext_choose_config(
            mwin->rcdisplay,
            rcattribs,
            &rcconfig,
            1,
            &numconfig);
        err = (numconfig >= 1) ? 0 : -1;
    }
    if(err == 0)
    {
        mwin->rcsurface = taa_glcontext_create_surface(
            mwin->rcdisplay,
            rcconfig,
            mwin->win);
        err = (mwin->rcsurface != 0) ? 0 : -1;
    }
    if(err == 0)
    {
        mwin->rc = taa_glcontext_create(
            mwin->rcdisplay,
            mwin->rcsurface,
            rcconfig,
            NULL,
            NULL);
        err = (mwin->rc != NULL) ? 0 : -1;
    }
    if(err == 0)
    {
        int success = taa_glcontext_make_current(
            mwin->rcdisplay,
            mwin->rcsurface,
            mwin->rc);
        err = (success) ? 0 : -1;
    }
    if(err == 0)
    {
        taa_window_show(mwin->windisplay, mwin->win, 1);
    }
    return err;
}

//****************************************************************************
static void main_close_window(
    main_win* mwin)
{
    taa_window_show(mwin->windisplay, mwin->win, 0);
    if(mwin->rc != NULL)
    {
        taa_glcontext_make_current(mwin->rcdisplay,mwin->rcsurface,NULL);
        taa_glcontext_destroy(mwin->rcdisplay, mwin->rc);
        taa_glcontext_destroy_surface(
            mwin->rcdisplay,
            mwin->win,
            mwin->rcsurface);
    }
    if(mwin->rcdisplay != NULL)
    {
        taa_glcontext_terminate(mwin->rcdisplay);
    }
    taa_window_destroy(mwin->windisplay, mwin->win);
    if(mwin->windisplay != NULL)
    {
        taa_window_close_display(mwin->windisplay);
    }
}

//****************************************************************************
void main_exec(
    main_win* mwin,
    const char* rootdir)
{
    taa_workqueue* wq;
    taa_asset_storage* storage;
    taa_asset_dir_storage* dirmgr;
    taa_asset_group* group;
    tgaasset_mgr* tgamgr;
    taa_mouse_state mouse;
    taa_keyboard_state kb;
    uint32_t vw;
    uint32_t vh;
    tgaasset* assets[NUM_BOXES];
    taa_texture2d textures[NUM_BOXES];
    taa_texture2d txfont;
    int i;
    int quit;
    unsigned frame;
    unsigned framemark;
    float fps;
    float avgms;
    float peakms;
    int64_t fpstimer;
    int64_t frametimer;
    int64_t timenow;
    uint64_t fpsdelta;
    uint64_t framedelta;

    // initialize
    taa_workqueue_create(32, &wq);
    // debug font
    taa_texture2d_create(&txfont);
    debugfont_init(txfont);
    // initialize asset managers. opengl contexts must be active at this point
    taa_asset_create_storage(2, 8, &storage);
    taa_asset_create_dir_storage(2, &dirmgr);
    tgaasset_create_mgr(storage,wq,32,12,&tgamgr);
    // create data
    populate_asset_dir(rootdir, "data");
    // scan for data
    group = scan_asset_dir(dirmgr, rootdir, "data");
    if(group != NULL)
    {
        tgaasset_register_storage(tgamgr, group);
    }
    // ready to begin, show the window
    taa_keyboard_query(mwin->windisplay, &kb);
    taa_mouse_query(mwin->windisplay, mwin->win, &mouse);
    taa_window_get_size(mwin->windisplay, mwin->win, &vw, &vh);
    quit = 0;
    // initialize assets
    for(i = 0; i < NUM_BOXES; ++i)
    {
        assets[i] = NULL;
        textures[i] = 0;
    }
    // main loop
    frame = 0;
    framemark = 0;
    fpstimer = taa_timer_sample_cpu();
    frametimer = fpstimer;
    fpsdelta = 0;
    framedelta = 0;
    fps = 0.0f;
    avgms = 0.0f;
    peakms = 0.0f;
    while(!quit)
    {
        static const float tc[] =
        {
            0.0f,0.0f, 0.0f,1.0f, 1.0f, 0.0f, 1.0f,1.0f
        };
        taa_window_event events[16];
        taa_window_event* evt;
        taa_window_event* evtend;
        taa_workqueue_func wkfunc;
        void* wkdata;
        float x;
        float y;
        uint32_t numevents;
        // sim window
        numevents = taa_window_update(mwin->windisplay,mwin->win,events,16);
        taa_keyboard_update(events, numevents, &kb);
        taa_mouse_update(events, numevents, &mouse);
        evt = events;
        evtend = evt + numevents;
        while(evt != evtend)
        {
            switch(evt->type)
            {
            case taa_WINDOW_EVENT_CLOSE:
                quit = 1; // true
                break;
            case taa_WINDOW_EVENT_KEY_DOWN:
                if(evt->key.keycode == taa_KEY_ESCAPE)
                {
                    quit = 1;
                }
                break;
            case taa_WINDOW_EVENT_SIZE:
                vw = evt->size.width;
                vh = evt->size.height;
                break;
            default:
                break;
            };
            ++evt;
        }
        // sim assets
        for(i = 0; i < NUM_BOXES; ++i)
        {
            if((rand() % 16) == 7)
            {
                // randomly acquire or release an asset depending on its state
                if(assets[i] == NULL)
                {
                    if(group != NULL && group->numfiles > 0)
                    {
                        int keynum = rand()%group->numfiles;
                        taa_asset_key key;
                        key.parts.group = group->key;
                        key.parts.file = group->files[keynum].filekey;
                        assets[i] = tgaasset_acquire(tgamgr, key);
                    }
                }
                else
                {
                    tgaasset_release(assets[i]);
                    assets[i] = NULL;
                    textures[i] = 0;
                }
            }
            if(assets[i] != NULL && textures[i] == 0)
            {
                // asset exists, but no texture yet
                tgaasset_poll(tgamgr, assets[i], textures + i);
            }
        }
        // process work
        while(taa_workqueue_pop(wq, 0, &wkfunc, &wkdata))
        {
            wkfunc(wkdata);
        }
        // render
        taa_glcontext_swap_buffers(mwin->rcdisplay, mwin->rcsurface);
        glViewport(0, 0, vw, vh);
        glClearColor(0.25f, 0.25f, 0.25f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glEnable(GL_BLEND);
        glEnable(GL_TEXTURE_2D);
        glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glEnableClientState(GL_VERTEX_ARRAY);
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
        glTexCoordPointer(2, GL_FLOAT, 8, tc);
        x = -0.75f;
        y = -0.75f;
        i = (vw < vh) ? vw : vh;
        glScalef(((float)i)/vw, ((float)i)/vh, 1.0f);
        for(i = 0; i < NUM_BOXES; ++i)
        {
            if(textures[i] != 0)
            {
                float pos[] =
                {
                    -0.2f + x, -0.2f + y,
                    -0.2f + x,  0.2f + y,
                     0.2f + x, -0.2f + y,
                     0.2f + x,  0.2f + y
                };
                glBindTexture(GL_TEXTURE_2D, (GLuint) textures[i]);
                glVertexPointer(2, GL_FLOAT, 8, pos);
                glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            }
            x += 0.5f;
            if(x >= 1.0f)
            {
                x = -0.75f;
                y += 1.5f;
            }
        }
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
        glDisable(GL_BLEND);
        glDisable(GL_TEXTURE_2D);
        glLoadIdentity();
        {
            float pos[] =
            {
                -0.9f, -0.03125f,
                -0.9f,  0.03125f,
                 0.9f, -0.03125f,
                 0.9f,  0.03125f 
            };
            float s = ((float) fabs((fpsdelta/1000000000.0)-0.5))*2.0f + 0.1f;
            glScalef(s, 1.0f, 1.0f);
            glVertexPointer(2, GL_FLOAT, 8, pos); 
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            glLoadIdentity();
        }
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
        glEnable(GL_BLEND);
        glEnable(GL_TEXTURE_2D);
        {
            debugfont_vert vtxt[32 * 6];
            char str[32];
            unsigned n;
            glBindTexture(GL_TEXTURE_2D, (GLuint) (size_t) txfont);
            sprintf(str, "fps    : %7.3f", fps);
            n = debugfont_gen_vertices(
                str,
                vw, vh,
                vw - 20*DEBUGFONT_CHAR_WIDTH, 16,
                0xffffff,
                vtxt, 64*6);
            glVertexPointer(2, GL_FLOAT, sizeof(*vtxt), vtxt[0].pos);
            glTexCoordPointer(2, GL_FLOAT, sizeof(*vtxt), vtxt[0].uv);
            glDrawArrays(GL_TRIANGLES, 0, n);
            sprintf(str, "avg ms : %7.3f", avgms);
            n = debugfont_gen_vertices(
                str,
                vw, vh,
                vw - 20*DEBUGFONT_CHAR_WIDTH, 32,
                0xffffff,
                vtxt, 64*6);
            glVertexPointer(2, GL_FLOAT, sizeof(*vtxt), vtxt[0].pos);
            glTexCoordPointer(2, GL_FLOAT, sizeof(*vtxt), vtxt[0].uv);
            glDrawArrays(GL_TRIANGLES, 0, n);

            sprintf(str, "peak ms: %7.3f", peakms);
            n = debugfont_gen_vertices(
                str,
                vw, vh,
                vw - 20*DEBUGFONT_CHAR_WIDTH, 48,
                0xffffff,
                vtxt, 64*6);
            glVertexPointer(2, GL_FLOAT, sizeof(*vtxt), vtxt[0].pos);
            glTexCoordPointer(2, GL_FLOAT, sizeof(*vtxt), vtxt[0].uv);
            glDrawArrays(GL_TRIANGLES, 0, n);
        }
        glDisableClientState(GL_VERTEX_ARRAY);
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
        glDisable(GL_BLEND);
        glDisable(GL_TEXTURE_2D);
        timenow = taa_timer_sample_cpu();
        fpsdelta = timenow - fpstimer;
        if(fpsdelta >= taa_TIMER_S_TO_NS(1)) // 1 sec
        {
            float secdelta = (float) taa_TIMER_NS_TO_S((double) fpsdelta);
            fps = (frame - framemark)/secdelta;
            if(fps != 0)
            {
                avgms = (float) (taa_TIMER_NS_TO_MS((double) fpsdelta)/fps);
            }
            peakms = 0;
            framemark = frame;
            fpstimer = taa_timer_sample_cpu();
        }
        framedelta = timenow - frametimer;
        if(taa_TIMER_NS_TO_MS(framedelta) > peakms)
        {
            peakms = (float) taa_TIMER_NS_TO_MS((double) framedelta);
        }
        taa_sched_yield();
        frametimer = taa_timer_sample_cpu();
        ++frame;
    }
    // make sure the window gets hidden
    taa_window_show(mwin->windisplay, mwin->win, 0);
    taa_workqueue_abort(wq);
    taa_asset_stop_storage_thread(storage);
    // clean up
    taa_texture2d_destroy(txfont);
    tgaasset_destroy_mgr(tgamgr);
    taa_asset_destroy_dir_storage(dirmgr);
    taa_asset_destroy_storage(storage);

    taa_workqueue_destroy(wq);
}

int main(int argc, char* argv[])
{
    main_win mwin;
    char rootdir[taa_PATH_SIZE];
    int err;

    taa_path_get_dir(argv[0], rootdir, sizeof(rootdir));
    err = main_init_window(&mwin);
    if(err == 0)
    {
        main_exec(&mwin, rootdir);
    }
    main_close_window(&mwin);

#if defined(_DEBUG) && defined(_MSC_FULL_VER)
    _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDOUT);
    _CrtCheckMemory();
    _CrtDumpMemoryLeaks();
#endif
    return EXIT_SUCCESS;
}

