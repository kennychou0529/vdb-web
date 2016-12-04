#include "vdb.h"

#define SO_PLATFORM_IMPLEMENTATION
#include "so_platform_sdl.h"
#include <SDL_mutex.h>

#include <stdio.h>
#include <assert.h>

#define vdb_buf_capacity (1280*720)
struct vdb_buf_t
{
    vdb_cmd_t cmd[vdb_buf_capacity];
    int count;
};

struct vdb_sv_globals_t
{
    vdb_buf_t avail;
    vdb_buf_t busy;
    SDL_mutex *mutex;
};

static vdb_sv_globals_t vdb_sv;

// Acquire the available drawcall buffer to render it.
// This is protected by a mutex to ensure that no swapping
// occurs during the rendering.
vdb_buf_t vdb_sv_getBuf()
{
    assert(SDL_LockMutex(vdb_sv.mutex) == 0);
    return vdb_sv.avail;
}

// Release the drawcall buffer to let the sv swap it.
void vdb_sv_relBuf()
{
    assert(SDL_UnlockMutex(vdb_sv.mutex) == 0);
}

int vdb_sv_thread(void *userdata)
{
    vdb_cmd_t cmd = {0};

    printf("[vdb_sv] Waiting for client...\n");
    tcp_sv_accept(vdb_sv_port);
    while (true)
    {
        printf("[vdb_sv] Waiting for recv...\n");
        int bytes = tcp_recv(&cmd, sizeof(cmd));

        if (bytes == tcp_connection_closed)
        {
            printf("[vdb_sv] Connection was closed\n");
            return 0;
        }

        if (cmd.type == vdb_cmd_type_begin)
        {
            printf("[vdb_sv] Got begin\n");
        }

        vdb_event_t event = {0};
        tcp_sv_send(&event, sizeof(event));


    }
    return 0;
}

void tick(so_input input)
{

}

int main(int argc, char **argv)
{
    vdb_sv.mutex = SDL_CreateMutex();
    SDL_CreateThread(vdb_sv_thread, "vdb_sv_thread", 0);

    so_openWindow("Hello sailor!", 640, 480, -1, -1, 1, 5, 4, 8, 24, 0);

    so_input input = {0};
    while (so_loopWindow(&input))
    {
        glViewport(0, 0, input.width, input.height);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        tick(input);

        if (input.keys[SO_PLATFORM_KEY(ESCAPE)].pressed)
            break;

        so_swapBuffersAndSleep(input.dt);
    }
    so_closeWindow();

    printf("[vdb] Closing socket...\n");
    tcp_close();
    SDL_DestroyMutex(vdb_sv.mutex);
    return 0;
}
