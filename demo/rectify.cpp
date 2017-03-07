#include <math.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "vdb_release.h"

typedef unsigned char u08;

void bilinear(u08 *src, int w, int h, float x, float y, u08 *r, u08 *g, u08 *b)
{
    int x0 = (int)floorf(x);
    int x1 = (int)ceilf(x);
    int y0 = (int)floorf(y);
    int y1 = (int)ceilf(y);
    if (x0 < 0) x0 = 0;
    if (x0 > w-1) x0 = w-1;
    if (x1 < 0) x1 = 0;
    if (x1 > w-1) x1 = w-1;
    if (y0 < 0) y0 = 0;
    if (y0 > h-1) y0 = h-1;
    if (y1 < 0) y1 = 0;
    if (y1 > h-1) y1 = h-1;

    u08 r00 = src[(y0*w+x0)*3+0];
    u08 r10 = src[(y0*w+x1)*3+0];
    u08 r01 = src[(y1*w+x0)*3+0];
    u08 r11 = src[(y1*w+x1)*3+0];

    u08 g00 = src[(y0*w+x0)*3+1];
    u08 g10 = src[(y0*w+x1)*3+1];
    u08 g01 = src[(y1*w+x0)*3+1];
    u08 g11 = src[(y1*w+x1)*3+1];

    u08 b00 = src[(y0*w+x0)*3+2];
    u08 b10 = src[(y0*w+x1)*3+2];
    u08 b01 = src[(y1*w+x0)*3+2];
    u08 b11 = src[(y1*w+x1)*3+2];

    float dx = x-x0;
    float dy = y-y0;

    u08 r0 = r00 + (r10-r00)*dx;
    u08 r1 = r01 + (r11-r01)*dx;
    *r = r0 + (r1-r0)*dy;

    u08 g0 = g00 + (g10-g00)*dx;
    u08 g1 = g01 + (g11-g01)*dx;
    *g = g0 + (g1-g0)*dy;

    u08 b0 = b00 + (b10-b00)*dx;
    u08 b1 = b01 + (b11-b01)*dx;
    *b = b0 + (b1-b0)*dy;
}

void project_equidistant(float f, float u0, float v0, float x, float y, float z, float *u, float *v)
{
    float l = sqrtf(x*x+y*y);
    float L = x*x + y*y + z*z;
    float t = atanf(-l/z);
    float r = f*t;
    if (l < 0.001f) // Handle the singularity in center
    {
        *u = u0;
        *v = v0;
    }
    else
    {
        *u = u0 + r*x/l;
        *v = v0 - r*y/l;
    }
}

int main(int, char**)
{
    int Ix,Iy,Ic;
    u08 *I = stbi_load("../fisheye.jpg", &Ix, &Iy, &Ic, 3);
    if (!I) { printf("Failed to load image\n"); return 0; }

    // The rectified image will be stored here
    int Rx = 640;
    int Ry = 360;
    u08 *R = (u08*)calloc(Rx*Ry, 3*sizeof(u08));

    while (vdb_loop(5))
    {
        // Fisheye lens parameters
        static float f_f = 494.0f;
        static float u0_f = 649.0f;
        static float v0_f = 335.0f;

        // Desired pinhole lens parameters
        static float yfov_p = 90.0f*3.14f/180.0f;
        float f_p = Ry / tanf(yfov_p/2.0f);
        float u0_p = Rx/2.0f;
        float v0_p = Ry/2.0f;

        vdb_slider1f("yfov", &yfov_p, 0.1f, 3.0f);
        vdb_slider1f("f_f", &f_f, 450.0f, 550.0f);
        vdb_slider1f("u0_f", &u0_f, 600.0f, 700.0f);
        vdb_slider1f("v0_f", &v0_f, 300.0f, 400.0f);

        for (int v_dst = 0; v_dst < Ry; v_dst++)
        for (int u_dst = 0; u_dst < Rx; u_dst++)
        {
            // The pinhole model in spherical coordinates:
            //   r = f tan(theta)
            //   u = u0 + r cos(phi)
            //   v = v0 - r sin(phi)

            // We want to recover the unit direction vector for (u,v)
            // given by its spherical coordinates:
            //   x =  sin(theta)cos(phi)
            //   y =  sin(theta)sin(phi)
            //   z = -cos(theta)

            // Straightforward:
            //   (u-u0)2 + (v0-v)2 = r2
            //   -> r = sqrt((u-u0)2 + (v0-v)2)
            //   -> tantheta = r/f
            //   -> cos(phi) = (u-u0)/r
            //   -> sin(phi) = (v0-v)/r

            // Use trig identities:
            //   -> costheta = 1 / sqrt(1 + tantheta^2)
            //   -> sintheta = tantheta / sqrt(1 + tantheta^2)

            float f_p2 = f_p*f_p;
            float du = (u_dst - u0_p);
            float dv = (v0_p - v_dst);
            float r2 = du*du + dv*dv;
            float r = sqrtf(r2);
            float cosphi = du/r;
            float sinphi = dv/r;
            float costheta = 1.0f / sqrtf(1.0f + r2/f_p2);
            float sintheta = (r/f_p) / sqrtf(1.0f + r2/f_p2);
            float x = sintheta*cosphi;
            float y = sintheta*sinphi;
            float z = -costheta;

            // We then project this vector into the fisheye image to
            // find out what the destination pixel color should be:

            float u_src,v_src;
            project_equidistant(f_f,u0_f,v0_f, x,y,z, &u_src,&v_src);

            u08 cr = 0; u08 cg = 0; u08 cb = 0;
            if (u_src >= 0.0f && u_src < Ix && v_src >= 0.0f && v_src < Iy)
                bilinear(I, Ix, Iy, u_src, v_src, &cr, &cg, &cb);

            R[(u_dst + v_dst*Rx)*3 + 0] = cr;
            R[(u_dst + v_dst*Rx)*3 + 1] = cg;
            R[(u_dst + v_dst*Rx)*3 + 2] = cb;
        }

        vdb_imageRGB8(R, Rx, Ry);
    }

    return 0;
}
