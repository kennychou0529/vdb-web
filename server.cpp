#define VDB_LISTEN_PORT 8000
#include "vdb.h"
#include "vdb.c"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

void draw_cool_spinny_thing(float dt)
{
    float data[2*512];
    int count = 0;
    #define point(x,y) if (count < 512) { data[2*count+0] = x; data[2*count+1] = y; count++; }

    static float t = 0.0f;
    float pi = 3.14f;
    float two_pi = 2.0f*3.14f;
    float pi_half = 3.14f/2.0f;
    float dur = 3.2f;

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

    for (int i = 0; i <= 6; i++)
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
        point(r1*cosf(a1), r1*sinf(a1));
        point(r2*cosf(a2), r2*sinf(a2));
        point(r3*cosf(a3), r3*sinf(a3));
    }

    t += dt;
    if (t > dur)
        t -= dur;

    vdb_push_s32(count);
    for (int i = 0; i < 2*count; i++)
        vdb_push_r32(data[i]);
}

int main()
{
    int running = 1;
    while (running)
    {
        if (vdb_begin())
        {
            draw_cool_spinny_thing(1.0f/60.0f);
            vdb_end();
        }
        vdb_sleep(1000/60);
    }
}
