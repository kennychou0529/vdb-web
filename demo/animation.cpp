#include "vdb_release.h"
#include <math.h>

void draw_cool_spinny_thing(float dt)
{
    static float t = 0.0f;
    float pi = 3.14f;
    float two_pi = 2.0f*3.14f;
    float pi_half = 3.14f/2.0f;
    float dur = 3.5f;

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

        vdb_color_red(1);

        vdb_line(0.5f*r1*cosf(a1), r1*sinf(a1), 0.5f*r2*cosf(a2), r2*sinf(a2));
        vdb_line(0.5f*r1*cosf(a1), r1*sinf(a1), 0.5f*r3*cosf(a3), r3*sinf(a3));
        vdb_line(0.5f*r2*cosf(a2), r2*sinf(a2), 0.5f*r3*cosf(a3), r3*sinf(a3));

        vdb_color_red(2);
        vdb_point(0.5f*r1*cosf(a1), r1*sinf(a1));
        vdb_point(0.5f*r2*cosf(a2), r2*sinf(a2));
        vdb_point(0.5f*r3*cosf(a3), r3*sinf(a3));
    }

    t += dt;
    if (t > dur)
        t -= dur;
}

int main()
{
    while (vdb_loop(60))
    {
        vdb_setNicePoints(1);
        vdb_setPointSize(5.0f);
        vdb_setLineSize(3.0f);

        vdb_color_white(0);
        vdb_fillRect(-1.0f,-1.0f,2.0f,2.0f);
        draw_cool_spinny_thing(1.0f/60.0f);
    }
}
