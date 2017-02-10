#include <stdint.h>

#define vdb_assert(EXPR)  { if (!(EXPR)) { printf("[error]\n\tAssert failed at line %d in file %s:\n\t'%s'\n", __LINE__, __FILE__, #EXPR); return 0; } }
#define vdb_log(...)      { printf("[vdb] %s@L%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); }
#define vdb_log_once(...) { static int first = 1; if (first) { printf("[vdb] "); printf(__VA_ARGS__); first = 0; } }
#define vdb_err_once(...) { static int first = 1; if (first) { printf("[vdb] Error at line %d in file %s:\n[vdb] ", __LINE__, __FILE__); printf(__VA_ARGS__); first = 0; } }

#ifndef VDB_WORK_BUFFER_SIZE
#define VDB_WORK_BUFFER_SIZE (1024*1024)
#endif

#ifndef VDB_RECV_BUFFER_SIZE
#define VDB_RECV_BUFFER_SIZE (1024*1024)
#endif

#ifdef VDB_LISTEN_PORT
static int vdb_listen_port = VDB_LISTEN_PORT;
#else
static int vdb_listen_port = 0;
#endif

#define VDB_NUM_POINT2 (1024*100)
#define VDB_NUM_POINT3 (1024*100)
#define VDB_NUM_LINE2 (1024*100)
#define VDB_NUM_LINE3 (1024*100)
struct vdb_point2_t { uint8_t c; float x,y; };
struct vdb_point3_t { uint8_t c; float x,y,z; };
struct vdb_line2_t  { uint8_t c; float x1,y1,x2,y2; };
struct vdb_line3_t  { uint8_t c; float x1,y1,z1,x2,y2,z2; };
struct vdb_cmdbuf_t
{
    // Possibly other stuff
    // Desired projection, view angles,
    uint8_t color;
    uint32_t num_line2;
    uint32_t num_line3;
    uint32_t num_point2;
    uint32_t num_point3;
    vdb_line2_t  line2[VDB_NUM_LINE2];
    vdb_line3_t  line3[VDB_NUM_LINE3];
    vdb_point2_t point2[VDB_NUM_POINT2];
    vdb_point3_t point3[VDB_NUM_POINT3];
};
static vdb_cmdbuf_t vdb_cmdbuf_static = {0}; // @ Might fail if too big; use calloc
static vdb_cmdbuf_t *vdb_cmdbuf = &vdb_cmdbuf_static;
int vdb_serialize_cmdbuf();

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "tcp.c"
#include "websocket.c"
#if defined(_WIN32) || defined(_WIN64)
#include "vdb_win.c"
#else
#warning "Todo: Implement nice user-feedback logging for unix"
#error "Todo: Implement serialize in end()"
#error "Todo: Initialize vdb_cmdbuf!!"
#include <sys/mman.h>
#include <sys/types.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include "vdb_unix.c"
#endif

int vdb_set_listen_port(int port)
{
    #ifdef VDB_LISTEN_PORT
    printf("[vdb] Warning: You are setting the port with both vdb_set_listen_port and #define VDB_LISTEN_PORT.\nAre you sure this is intentional?\n");
    #endif
    vdb_listen_port = port;
    return 1;
}

void *vdb_push_bytes(const void *data, int count)
{
    if (vdb_shared->work_buffer_used + count <= VDB_WORK_BUFFER_SIZE)
    {
        const char *src = (const char*)data;
              char *dst = vdb_shared->work_buffer + vdb_shared->work_buffer_used;
        if (src) memcpy(dst, src, count);
        else     memset(dst, 0, count);
        vdb_shared->work_buffer_used += count;
        return (void*)dst;
    }
    return 0;
}

int vdb_push_u08(uint8_t x)
{
    if (vdb_shared->work_buffer_used + sizeof(uint8_t) <= VDB_WORK_BUFFER_SIZE)
    {
        *(uint8_t*)(vdb_shared->work_buffer + vdb_shared->work_buffer_used) = x;
        vdb_shared->work_buffer_used += sizeof(uint8_t);
        return 1;
    }
    return 0;
}

int vdb_push_u32(uint32_t x)
{
    if (vdb_shared->work_buffer_used + sizeof(uint32_t) <= VDB_WORK_BUFFER_SIZE)
    {
        *(uint32_t*)(vdb_shared->work_buffer + vdb_shared->work_buffer_used) = x;
        vdb_shared->work_buffer_used += sizeof(uint32_t);
        return 1;
    }
    return 0;
}

int vdb_push_r32(float x)
{
    if (vdb_shared->work_buffer_used + sizeof(float) <= VDB_WORK_BUFFER_SIZE)
    {
        *(float*)(vdb_shared->work_buffer + vdb_shared->work_buffer_used) = x;
        vdb_shared->work_buffer_used += sizeof(float);
        return 1;
    }
    return 0;
}

int vdb_serialize_cmdbuf()
{
    vdb_cmdbuf_t *b = vdb_cmdbuf;
    if (!vdb_push_u32(b->num_line2))  return 0;
    if (!vdb_push_u32(b->num_line3))  return 0;
    if (!vdb_push_u32(b->num_point2)) return 0;
    if (!vdb_push_u32(b->num_point3)) return 0;
    for (uint32_t i = 0; i < b->num_line2; i++)
    {
        if (!vdb_push_u08(b->line2[i].c))  return 0;
        if (!vdb_push_r32(b->line2[i].x1)) return 0;
        if (!vdb_push_r32(b->line2[i].y1)) return 0;
        if (!vdb_push_r32(b->line2[i].x2)) return 0;
        if (!vdb_push_r32(b->line2[i].y2)) return 0;
    }
    for (uint32_t i = 0; i < b->num_line3; i++)
    {
        if (!vdb_push_u08(b->line3[i].c))  return 0;
        if (!vdb_push_r32(b->line3[i].x1)) return 0;
        if (!vdb_push_r32(b->line3[i].y1)) return 0;
        if (!vdb_push_r32(b->line3[i].z1)) return 0;
        if (!vdb_push_r32(b->line3[i].x2)) return 0;
        if (!vdb_push_r32(b->line3[i].y2)) return 0;
        if (!vdb_push_r32(b->line3[i].z2)) return 0;
    }
    for (uint32_t i = 0; i < b->num_point2; i++)
    {
        if (!vdb_push_u08(b->point2[i].c)) return 0;
        if (!vdb_push_r32(b->point2[i].x)) return 0;
        if (!vdb_push_r32(b->point2[i].y)) return 0;
    }
    for (uint32_t i = 0; i < b->num_point3; i++)
    {
        if (!vdb_push_u08(b->point3[i].c)) return 0;
        if (!vdb_push_r32(b->point3[i].x)) return 0;
        if (!vdb_push_r32(b->point3[i].y)) return 0;
    }

    return 1;
}

void vdb_color1i(int c)
{
    if (c < 0) c = 0;
    if (c > 255) c = 255;
    vdb_cmdbuf->color = (unsigned char)(c);
}

void vdb_color1f(float c)
{
    int ci = c*255.0f;
    if (ci < 0) ci = 0;
    if (ci > 255) ci = 255;
    vdb_cmdbuf->color = (unsigned char)(ci);
}

void vdb_point2(float x, float y)
{
    if (vdb_cmdbuf && vdb_cmdbuf->num_point2 < VDB_NUM_POINT2)
    {
        vdb_point2_t *p = &vdb_cmdbuf->point2[vdb_cmdbuf->num_point2];
        p->c = vdb_cmdbuf->color;
        p->x = x;
        p->y = y;
        vdb_cmdbuf->num_point2++;
    }
}

void vdb_point3(float x, float y, float z)
{
    if (vdb_cmdbuf && vdb_cmdbuf->num_point3 < VDB_NUM_POINT3)
    {
        vdb_point3_t *p = &vdb_cmdbuf->point3[vdb_cmdbuf->num_point3];
        p->c = vdb_cmdbuf->color;
        p->x = x;
        p->y = y;
        p->z = z;
        vdb_cmdbuf->num_point3++;
    }
}

void vdb_line2(float x1, float y1, float x2, float y2)
{
    if (vdb_cmdbuf && vdb_cmdbuf->num_line2 < VDB_NUM_LINE2)
    {
        vdb_line2_t *p = &vdb_cmdbuf->line2[vdb_cmdbuf->num_line2];
        p->c = vdb_cmdbuf->color;
        p->x1 = x1;
        p->y1 = y1;
        p->x2 = x2;
        p->y2 = y2;
        vdb_cmdbuf->num_line2++;
    }
}

void vdb_line3(float x1, float y1, float z1, float x2, float y2, float z2)
{
    if (vdb_cmdbuf && vdb_cmdbuf->num_line3 < VDB_NUM_LINE3)
    {
        vdb_line3_t *p = &vdb_cmdbuf->line3[vdb_cmdbuf->num_line3];
        p->c = vdb_cmdbuf->color;
        p->x1 = x1;
        p->y1 = y1;
        p->z1 = z1;
        p->x2 = x2;
        p->y2 = y2;
        p->z2 = z2;
        vdb_cmdbuf->num_line3++;
    }
}
