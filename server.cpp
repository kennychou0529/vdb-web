#define VDB_LISTEN_PORT 8000
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
    float r1 = 0.55f;
    float r2 = 0.5f;
    float r3 = 0.25f;

    for (int i = 0; i <= 6; i++)
    {
        float r = r1;
        float a = (two_pi/6.0f)*(i*(t1-t2) + 12.0f*t2 - 1.5f);
        point(r*cosf(a), r*sinf(a));
    }

    for (int i = 0; i <= 6; i++)
    {
        float r = r1 + (r2-r1)*t1 - (r2-r1)*t2;
        float a = (two_pi/6.0f)*(i*(t1-t2) + 11.5f*t2 - 1.5f + 0.5f*t1);
        point(r*cosf(a), r*sinf(a));
    }

    for (int i = 0; i <= 6; i++)
    {
        float r = r1 + (r3-r1)*t1 - (r3-r1)*t2;
        float a = (two_pi/6.0f)*(i*(t1-t2) + 6.0f*t1 + 12.0f*t2 - 1.5f);
        point(r*cosf(a), r*sinf(a));
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
            draw_cool_spinny_thing(1.0f/120.0f);
            vdb_end();
        }
        vdb_sleep(500);
    }
}
