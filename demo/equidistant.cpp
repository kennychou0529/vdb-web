// demo - Fisheye camera projection using the equidistant lens model
//        (see wikipedia)
//        r = f theta
//        u = u0 + r cos phi
//        v = v0 - r sin phi
//
//        x points right in camera space
//        y points up in camera space
//        -z points forward in camera space
//
//        u is 0 at the left and width at the right of the image
//        v is 0 at the top and height at the bottom of the image
//
// compiling -
//        (g++)  g++ equidistant.cpp -o equidistant
//        (msvc) cl equidistant.cpp
//
// running -
//        Open your browser and direct it to 127.0.0.1:8000

#include <math.h>
#include "vdb_release.h"

void project_equidistant(float f, float u0, float v0,
                         float x, float y, float z,
                         float *u, float *v,
                         float *dudx, float *dudy, float *dudz,
                         float *dvdx, float *dvdy, float *dvdz)
{
    float l = sqrtf(x*x+y*y);
    float L = x*x + y*y + z*z;
    float t = atanf(-l/z);
    float r = f*t;
    if (l < 0.001f) // Handle the singularity in center
    {
        *u = u0;
        *v = v0;
        *dudx = 0.0f;
        *dudy = 0.0f;
        *dudz = x*f/L;
        *dvdx = 0.0f;
        *dvdy = 0.0f;
        *dvdz = -y*f/L;
    }
    else
    {
        float c = x/l;
        float s = y/l;
        float cc = c*c;
        float cs = c*s;
        float ss = s*s;
        float rl = r/l;
        float fL = f/L;
        *u = u0 + r*c;
        *v = v0 - r*s;
        *dudx =  ss*rl - cc*z*fL;
        *dudy = -cs*rl - cs*z*fL;
        *dudz =  x*fL;
        *dvdx =  cs*rl + cs*z*fL;
        *dvdy = -cc*rl + ss*z*fL;
        *dvdz = -y*fL;
    }
}

int main()
{
    float f  = 493.9999f;
    float u0 = 649.3241f;
    float v0 = 334.5365f;
    float w  = 1280.0f;
    float h  = 720.0f;
    while (vdb_loop(60))
    {
        static float len = 0.05f; vdb_slider1f("arrow length", &len, 0.01f, 0.1f);
        static float dx = 0.0f;   vdb_slider1f("dx", &dx, -1.5f, +1.5f);
        static float dy = 0.0f;   vdb_slider1f("dy", &dy, -1.5f, +1.5f);
        static float dz = 0.0f;   vdb_slider1f("dz", &dz, -1.5f, +1.5f);
        static   int  n = 8;      vdb_slider1i("bananas", &n, 1, 16);

        vdb_setNicePoints(1);
        vdb_setPointSize(3.0f);

        vdb_color_white(0);
        vdb_fillRect(-1,-1,2,2);

        vdb_xrange(0.0f, w);
        vdb_yrange(h, 0.0f);
        for (int yi = 0; yi < n; yi++)
        for (int xi = 0; xi < n; xi++)
        {
            float x = -1.0f + 2.0f*(xi+0.5f)/n + dx;
            float y = -1.0f + 2.0f*(yi+0.5f)/n + dy;
            float z = -1.0f + dz;

            float u,v;
            float dudx,dudy,dudz;
            float dvdx,dvdy,dvdz;
            project_equidistant(f,u0,v0, x,y,z, &u,&v, &dudx,&dudy,&dudz,&dvdx,&dvdy,&dvdz);

            vdb_color_black(2);
            vdb_point(u,v);

            vdb_color_red(1);
            vdb_line(u,v, u+len*dudx,v+len*dvdx);
            vdb_color_green(1);
            vdb_line(u,v, u+len*dudy,v+len*dvdy);
            vdb_color_blue(1);
            vdb_line(u,v, u+len*dudz,v+len*dvdz);
        }
    }
}
