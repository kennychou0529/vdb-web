#include "vdb.h"
#include <assert.h>
#include <stdio.h>
#include <math.h>

const int vdb_cmd_buffer_capacity = (1280*720);
static bool vdb_has_socket = false;
static vdb_cmd vdb_cmd_buffer[vdb_cmd_buffer_capacity];
static int vdb_cmd_buffer_count = 0;

void vdb_begin()
{
    if (!vdb_has_socket)
    {
        bool ok = udp_open(20020, true);
        assert(ok == true);
        vdb_has_socket = true;
    }
    vdb_cmd_buffer[0].magic = vdb_cmd_magic_begin;
    vdb_cmd_buffer_count++;

    // todo: udp_recv
}

void vdb_end()
{
    assert(vdb_has_socket);
    udp_addr dst = { 127, 0, 0, 1, 20021 };
    assert(vdb_cmd_buffer_capacity % vdb_segment_count == 0);
    assert(vdb_cmd_buffer_count + vdb_segment_count <= vdb_cmd_buffer_capacity);
    while (vdb_cmd_buffer_count % vdb_segment_count != 0)
    {
        vdb_cmd_buffer[vdb_cmd_buffer_count].magic = vdb_cmd_magic_padding;
        vdb_cmd_buffer_count++;
    }

    int remaining = vdb_cmd_buffer_count;
    vdb_cmd *ptr = vdb_cmd_buffer;
    while (remaining >= vdb_segment_count)
    {
        udp_send((void*)ptr, vdb_segment_size, dst);
        ptr += vdb_segment_count;
        remaining -= vdb_segment_count;
    }

    assert(remaining == 0);
    vdb_cmd_buffer_count = 0;
}

void vdb_point2(float x, float y)
{
    assert(vdb_cmd_buffer_count <= vdb_cmd_buffer_capacity);
    if (vdb_cmd_buffer_count == vdb_cmd_buffer_capacity)
        return;

    vdb_cmd cmd = {0};
    cmd.magic = vdb_cmd_magic_point2;
    cmd.point.x = x;
    cmd.point.y = y;
    vdb_cmd_buffer[vdb_cmd_buffer_count++] = cmd;
}

int main()
{
    printf("...");
    getchar();
    vdb_begin();
    for (int i = 0; i < 128; i++)
    {
        float t = 2.0f*3.1415926f*i/128.0f;
        float x = 0.5f*cosf(t);
        float y = 0.5f*sinf(t);
        vdb_point2(x, y);
    }
    vdb_end();
}
