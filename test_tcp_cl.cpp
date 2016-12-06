#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <math.h>

#include "vdb.h"

#define SV_PORT 27042
#define CL_PORT 27043

void vdb_block_begin()
{
    vdb_cmd_t cmd = {0};
    cmd.type = vdb_cmd_type_begin;
    tcp_send(&cmd, sizeof(cmd));
}

void vdb_block_end()
{
    vdb_cmd_t cmd = {0};
    cmd.type = vdb_cmd_type_end;
    tcp_send(&cmd, sizeof(cmd));

    vdb_event_t event;
    tcp_recv_message(&event, sizeof(event));
}

#if 0
// int main()
// {
//     while (vdb_loop())
//     {
//         ...
//     }
// }
bool vdb_loop()
{
    vdb_event_t event;
    tcp_recv(&event, sizeof(event));
    if (event.type == vdb_event_type_step_once)
    {
        return false;
    }
    else
    {
        return true;
    }
}
#endif

void vdb_point(float x, float y)
{
    vdb_cmd_t cmd = {0};
    cmd.type = vdb_cmd_type_point;
    cmd.point.x = x;
    cmd.point.y = y;
    tcp_send(&cmd, sizeof(cmd));
}

int main()
{
    assert(tcp_cl_connect(127,0,0,1,SV_PORT));

    vdb_block_begin();

    for (int i = 0; i < 32; i++)
    {
        float t = 2.0f*3.14f*i/32.0f;
        float x = 0.5f*cosf(t);
        float y = 0.5f*sinf(t);
        vdb_point(x, y);
    }

    vdb_point(0.3f, 0.5f);
    vdb_point(0.1f, 0.7f);
    vdb_point(0.2f, 0.3f);

    vdb_block_end();

    {
        char buffer[1024];
        int bytes = tcp_recv(buffer, sizeof(buffer));
        printf("read %d bytes\n", bytes);
    }

    tcp_close();
}
