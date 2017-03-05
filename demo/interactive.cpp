#include "vdb_release.h"

int main()
{
    while (vdb_loop(60))
    {
        static float x1 = -0.5f;
        static float y1 = -0.5f;
        static float x2 = 0.5f;
        static float y2 = 0.5f;
        static float point_size = 4;
        static int nice_points = 1;
        static int bg_shade = 0;

        float mx,my;
        if (vdb_mouse_click(&mx, &my))
        {
            x1 = mx;
            y1 = my;
        }

        vdb_color_white(bg_shade);
        vdb_fillRect(-1.0f,-1.0f,2.0f,2.0f);

        vdb_color_blue(1);
        vdb_point(x1,y1);

        vdb_color_red(1);
        vdb_point(x2,y2);

        vdb_slider1f("Red point x", &x2, -1.0f, +1.0f);
        vdb_slider1f("Red point y", &y2, -1.0f, +1.0f);

        vdb_slider1f("Point size", &point_size, 1.0f, 64.0f);
        vdb_slider1i("Background shade", &bg_shade, 0, 2);
        vdb_checkbox("Nice points", &nice_points);

        vdb_setNicePoints(nice_points);
        vdb_setPointSize(point_size);
    }
}
