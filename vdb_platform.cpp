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
    vdb_buf_t buf1;
    vdb_buf_t buf2;

    vdb_buf_t *avail;
    vdb_buf_t *busy;

    SDL_mutex *mutex;
};

static vdb_sv_globals_t vdb_sv;

// Acquire the available drawcall buffer to render it.
// This is protected by a mutex to ensure that no swapping
// occurs during the rendering.
vdb_buf_t *vdb_sv_getBuf()
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
        if (!tcp_recv_message(&cmd, sizeof(cmd)))
        {
            printf("[vdb_sv] Connection was closed\n");
            return 0;
        }

        if (cmd.type == vdb_cmd_type_begin)
        {
            printf("[vdb_sv] Got begin\n");
            vdb_sv.busy->count = 0;
        }
        else
        if (cmd.type == vdb_cmd_type_end)
        {
            printf("[vdb_sv] Got end\n");

            // Swap buffers
            assert(SDL_LockMutex(vdb_sv.mutex) == 0);
            vdb_buf_t *temp = vdb_sv.avail;
            vdb_sv.avail = vdb_sv.busy;
            vdb_sv.busy = temp;
            assert(SDL_UnlockMutex(vdb_sv.mutex) == 0);

            // Carry on
            vdb_event_t event = {0};
            tcp_send(&event, sizeof(event));
        }
        else
        // if (cmd.type == vdb_cmd_type_point || ...)
        {
            vdb_sv.busy->cmd[vdb_sv.busy->count++] = cmd;
        }
    }
    return 0;
}

void tick(so_input input)
{
    vdb_buf_t *buf = vdb_sv_getBuf();
    vdb_cmd_t *cmd = buf->cmd;
    int cmd_count = buf->count;

    glClearColor(0.05f, 0.07f, 0.10f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

    for (int i = 0; i < cmd_count; i++)
    {
        switch (cmd[i].type)
        {
            case vdb_cmd_type_point:
            {
                glPointSize(4.0f);
                glBegin(GL_POINTS);
                glVertex2f(cmd[i].point.x, cmd[i].point.y);
                glEnd();
            } break;
            case vdb_cmd_type_line:
            {
                glBegin(GL_LINES);
                glVertex2f(cmd[i].line.x1, cmd[i].line.y1);
                glVertex2f(cmd[i].line.x2, cmd[i].line.y2);
                glEnd();
            } break;
            case vdb_cmd_type_color:
            {
                // glColor4f(cmd[i].)
            } break;
            case vdb_cmd_type_clear:
            {

            } break;
        }
    }

    vdb_sv_relBuf();
}

int main(int argc, char **argv)
{
    vdb_sv.mutex = SDL_CreateMutex();
    vdb_sv.avail = &vdb_sv.buf1;
    vdb_sv.busy = &vdb_sv.buf2;
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
