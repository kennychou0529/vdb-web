#include <math.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "vdb_release.h"

// Difference-from-color based threshold segmentation
void threshold(unsigned char *in_rgb,
               unsigned char *out_gray,
               int w,
               int h,
               float rt,
               float gt,
               float bt,
               float dt)
{
    for (int y = 0; y < h; y++)
    for (int x = 0; x < w; x++)
    {
        unsigned char *pixel = &in_rgb[(y*w+x)*3];
        float r = (float)pixel[0];
        float g = (float)pixel[1];
        float b = (float)pixel[2];

        float dr = fabs(r - rt);
        float dg = fabs(g - gt);
        float db = fabs(b - bt);
        float dd = (dr + dg + db) / 3.0f;

        unsigned char result;
        if (dd < dt)
        {
            float result_real = (r + r + b + g + g + g) / 6.0f;
            result_real *= 1.0f - dd/dt;
            int result_rounded = (int)result_real;
            if (result_rounded < 0) result_rounded = 0;
            if (result_rounded > 255) result_rounded = 255;
            result = (unsigned char)result_rounded;
        }
        else
        {
            result = 0;
        }
        out_gray[y*w+x] = result;
    }
}

int main(int, char**)
{
    int Ix,Iy,Ic;
    unsigned char *I = stbi_load("../fisheye2.jpg", &Ix, &Iy, &Ic, 3);
    if (!I) { printf("Failed to load image\n"); return 0; }
    unsigned char *T = (unsigned char*)malloc(Ix*Iy);
    unsigned char *T_rgb = (unsigned char*)malloc(Ix*Iy*3);

    float rt = 200.0f;
    float gt = 218.0f;
    float bt = 177.0f;
    float dt = 57.0f;

    while (vdb_loop(5))
    {
        vdb_slider1f("rt", &rt, 0.0f, 255.0f);
        vdb_slider1f("gt", &gt, 0.0f, 255.0f);
        vdb_slider1f("bt", &bt, 0.0f, 255.0f);
        vdb_slider1f("dt", &dt, 0.0f, 255.0f);

        threshold(I, T, Ix, Iy, rt, gt, bt, dt);
        for (int i = 0; i < Ix*Iy; i++)
        {
            T_rgb[3*i+0] = T[i];
            T_rgb[3*i+1] = T[i];
            T_rgb[3*i+2] = T[i];
        }

        vdb_imageRGB8(T_rgb, Ix, Iy);
    }

    return 0;
}
