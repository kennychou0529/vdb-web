#define VDB_LOG_DEBUG
#define VDB_LISTEN_PORT 8000
#include "vdb.h"
#include "vdb.c"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>



int main()
{
    const int width = 8;
    const int height = 4;
    unsigned char image[width*height*3];
    for (int y = 0; y < height; y++)
    for (int x = 0; x < width; x++)
    {
        image[(x + y*width)*3+0] = (x % 64)*8 + 100;
        image[(x + y*width)*3+1] = (y % 64)*8 + 100;
        image[(x + y*width)*3+2] = 128;
    }

    while (vdb_loop(60))
    {
        static int more_lines = 0;

        vdb_xrange(0, 32);
        vdb_yrange(-1.0f, +1.0f);
        for (int i = 0; i < 32; i++)
        {
            vdb_color_ramp(i);
            vdb_line(i+0.5f, -1.0f, i+0.5f, +1.0f);

            if (more_lines)
            {
                vdb_line(0, -1.0f+2.0f*i/32.0f, 32, -1.0f+2.0f*i/32.0f);
            }
        }

        vdb_checkbox("More lines", &more_lines);

        static float y1 = 0.0f;
        static int x1 = 0;
        vdb_slider1i("x1", &x1, 0, 32);
        vdb_slider1f("y1", &y1, -1.0f, +1.0f);

        static float x = 0.0f;
        static float y = 0.0f;
        float mx,my;
        if (vdb_mouse_click(&mx,&my))
        {
            x = mx;
            y = my;
        }

        vdb_translucent();
        vdb_point(x, y);

        vdb_opaque();
        vdb_color_red(1);
        vdb_point((float)x1, y1);

        vdb_setPointSize(32.0f);
        vdb_setLineSize(8.0f);
        static float t = 0.0f;
        t += 1.0f/60.0f;
        vdb_setTranslucency(0.5f+0.5f*sinf(t));
        vdb_setNicePoints(1);
    }

    // while (vdb_loop(30))
    // {
    //     static float t = 0.0f;
    //     t += 1.0f/30.0f;
    //     vdb_imageRGB8(image, width, height);
    //     vdb_translucent();
    //     vdb_color(0);
    //     vdb_point(sinf(t), cosf(t));
    // }

    while (vdb_loop(60))
    {
        vdb_color_white(0);
        vdb_fillRect(-1,-1,2,2);
        draw_cool_spinny_thing(1.0f/60.0f);
    }
    return 0;
}
