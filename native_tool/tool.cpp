// The size of the VDB window is remembered between sessions.
// This path specifies the executable-relative path where the
// information is stored.
#ifndef VDB_SETTINGS_FILENAME
#define VDB_SETTINGS_FILENAME "./.build/vdb.ini"
#endif

// The state of ImGui windows is remembered between sessions.
// This path specifies the executable-relative path where the
// information is stored.
#ifndef VDB_IMGUI_INI_FILENAME
#define VDB_IMGUI_INI_FILENAME "./.build/imgui.ini"
#endif

#include "imgui.cpp"
#include "imgui_draw.cpp"
#include "imgui_demo.cpp"

#define SO_PLATFORM_IMPLEMENTATION
#define SO_PLATFORM_IMGUI
#include "so_platform_sdl.h"

int main(int, char**)
{
    int multisamples = 4;   // Set to > 0 to get smooth edges on points, lines and triangles.
    int alpha_bits = 8;     // Set to > 0 to be able to take screenshots with transparent backgrounds.
    int depth_bits = 24;    // Set to > 0 if you want to use OpenGL depth testing.
    int stencil_bits = 0;   // Set to > 0 if you want to use the OpenGL stencil operations.
    int gl_major = 3;
    int gl_minor = 1;
    int w = 640;
    int h = 480;
    int x = -1;
    int y = -1;
    so_openWindow("vdb", w, h, x, y, gl_major, gl_minor, multisamples, alpha_bits, depth_bits, stencil_bits);
    // so_imgui_init();

    so_input input = {0};
    while (so_loopWindow(&input))
    {
        // so_imgui_processEvents(input);
        glViewport(0, 0, input.width, input.height);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        so_swapBuffersAndSleep(input.dt);
    }

    return 0;
}
