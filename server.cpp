#define VDB_LISTEN_PORT 8000
#include "vdb.h"
#include "vdb.c"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

int vdb_point(unsigned char color, float x, float y)
{
    if (vdb_push_bytes(&color, 1) &&
        vdb_push_bytes(&x, 4) &&
        vdb_push_bytes(&y, 4))
        return 1;
    else
        return 0;
}

void draw_cool_spinny_thing(float dt)
{
    int *count_ptr = (int*)vdb_push_bytes(0, 4);
    int count = 0;

    static float t = 0.0f;
    float pi = 3.14f;
    float two_pi = 2.0f*3.14f;
    float pi_half = 3.14f/2.0f;
    float dur = 3.5f;

    #if 1
    int n1 = 30;
    int n2 = 32;
    for (int k = 0; k < n1; k++)
    for (int i = 0; i < n2; i++)
    {
        float x = -1.0f + 2.0f*i/n2;
        float a = pi*x + two_pi*(t/dur) + pi*k/n1;
        float y = 0.1f*sinf(a) + (-1.0f+2.0f*k/n1);
        count += vdb_point(k % 10, x, y);
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
        count += vdb_point(2, r1*cosf(a1), r1*sinf(a1));
        count += vdb_point(8, r2*cosf(a2), r2*sinf(a2));
        count += vdb_point(6, r3*cosf(a3), r3*sinf(a3));
    }
    #endif

    *count_ptr = count;

    t += dt;
    if (t > dur)
        t -= dur;
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
