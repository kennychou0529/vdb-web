#define VDB_LOG_DEBUG
#define VDB_LISTEN_PORT 8000
#include "vdb.h"
#include "vdb.c"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

void draw_cool_spinny_thing(float dt)
{
    static float t = 0.0f;
    float pi = 3.14f;
    float two_pi = 2.0f*3.14f;
    float pi_half = 3.14f/2.0f;
    float dur = 3.5f;

    #if 0
    int n1 = 30;
    int n2 = 32;
    for (int k = 0; k < n1; k++)
    for (int i = 0; i < n2; i++)
    {
        float x = -1.0f + 2.0f*i/n2;
        float a = pi*x + two_pi*(t/dur) + pi*k/n1;
        float y = 0.1f*sinf(a) + (-1.0f+2.0f*k/n1);
        vdb_color1i(k % 10);
        vdb_point2(x, y);
    }
    #else
    float t1 = 0.0f;
    float t2 = 0.0f;
    if (t < dur/2.0f)
    {
        t1 = 0.5f+0.5f*sinf(pi*t/(dur/2.0f) - pi_half);
        t2 = 0.0f;
    }
    else
    {
        t1 = 1.0f;
        t2 = 0.5f+0.5f*sinf(pi*(t-dur/2.0f)/(dur/2.0f) - pi_half);
    }

    for (int i = 0; i <= 5; i++)
    {
        float r10 = 0.55f;
        float r20 = 0.50f;
        float r30 = 0.25f;
        float r1 = r10;
        float r2 = r10 + (r20-r10)*t1 - (r20-r10)*t2;
        float r3 = r10 + (r30-r10)*t1 - (r30-r10)*t2;
        float a1 = (two_pi/6.0f)*(i*(t1-t2) + 12.0f*t2 - 1.5f);
        float a2 = (two_pi/6.0f)*(i*(t1-t2) + 11.5f*t2 - 1.5f + 0.5f*t1);
        float a3 = (two_pi/6.0f)*(i*(t1-t2) + 6.0f*t1 + 12.0f*t2 - 1.5f);

        vdb_color1i(2);
        vdb_line2(r1*cosf(a1), r1*sinf(a1), r2*cosf(a2), r2*sinf(a2));
        vdb_color1i(8);
        vdb_line2(r1*cosf(a1), r1*sinf(a1), r3*cosf(a3), r3*sinf(a3));
        vdb_color1i(6);
        vdb_line2(r2*cosf(a2), r2*sinf(a2), r3*cosf(a3), r3*sinf(a3));

        vdb_color1i(2); vdb_circle(r1*cosf(a1), r1*sinf(a1), 2.0f+t1*8.0f-t2*8.0f);
        vdb_color1i(8); vdb_circle(r2*cosf(a2), r2*sinf(a2), 2.0f+t1*8.0f-t2*8.0f);
        vdb_color1i(6); vdb_circle(r3*cosf(a3), r3*sinf(a3), 2.0f+t1*8.0f-t2*8.0f);
    }

    #endif

    t += dt;
    if (t > dur)
        t -= dur;
}

int main()
{
    const int width = 256;
    const int height = 256;
    unsigned char image[width*height*3];
    for (int y = 0; y < height; y++)
    for (int x = 0; x < width; x++)
    {
        image[(x + y*width)*3+0] = (x % 64) + 100;
        image[(x + y*width)*3+1] = (y % 64) + 100;
        image[(x + y*width)*3+2] = 128;
    }

    // int uploaded = 0;
    // while (!uploaded)
    // {
    //     if (vdb_begin())
    //     {
    //         vdb_image_rgb8(image, width, height);
    //         vdb_end();
    //         uploaded = 1;
    //     }
    //     vdb_sleep(1000);
    // }

    int running = 1;
    while (running)
    {
        if (vdb_begin())
        {
            vdb_image_rgb8(image, width, height);
            draw_cool_spinny_thing(1.0f/60.0f);
            vdb_end();
        }
        vdb_sleep(1000/60);
    }
}
