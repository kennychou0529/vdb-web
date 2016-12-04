#include <stdio.h>
#include <stdint.h>
#include <assert.h>

#include "vdb.h"

#define SV_PORT 27042
#define CL_PORT 27043

bool vdb_block_begin()
{
    vdb_cmd_t cmd = {0};
    cmd.type = vdb_cmd_type_begin;
    tcp_send(&cmd, sizeof(cmd));

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

void vdb_point(float x, float y)
{
    cmd.type = vdb_cmd_type_point;
    cmd.point.x = x;
    cmd.point.y = y;
    tcp_send(&cmd, sizeof(cmd));
}

int main()
{
    assert(tcp_cl_connect(127,0,0,1,SV_PORT));

    vdb_block_begin();

    vdb_point(0.3f, 0.5f);
    vdb_point(0.1f, 0.7f);
    vdb_point(0.2f, 0.3f);

    {
        char buffer[1024];
        int bytes = tcp_recv(buffer, sizeof(buffer));
        printf("read %d bytes\n", bytes);
    }

    tcp_close();
}
