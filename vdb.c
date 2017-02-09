#include <stdint.h>
#include <stdio.h>

// Configuration interface
int vdb_set_listen_port(int port);

// Primary interface (draw commands and control flow)
int vdb_begin();
void vdb_end();
void vdb_sleep(int milliseconds);

// Middle-level interface (raw buffer manipulation)
int vdb_push_s32(int32_t x);
int vdb_push_r32(float x);

// Implementation
#define vdb_assert(EXPR) if (!(EXPR)) { printf("[error]\n\tAssert failed at line %d in file %s:\n\t'%s'\n", __LINE__, __FILE__, #EXPR); return 0; }
#define vdb_log(...) { printf("[vdb] %s@L%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); }
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

#include "tcp.c"
#include "websocket.c"
#if defined(_WIN32) || defined(_WIN64)
#include "vdb_win.c"
#else
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

int vdb_push_s32(int32_t x)
{
    if (vdb_shared->work_buffer_used + sizeof(x) < VDB_WORK_BUFFER_SIZE)
    {
        *(int32_t*)(vdb_shared->work_buffer + vdb_shared->work_buffer_used) = x;
        vdb_shared->work_buffer_used += sizeof(x);
        return 1;
    }
    return 0;
}

int vdb_push_r32(float x)
{
    if (vdb_shared->work_buffer_used + sizeof(x) < VDB_WORK_BUFFER_SIZE)
    {
        *(float*)(vdb_shared->work_buffer + vdb_shared->work_buffer_used) = x;
        vdb_shared->work_buffer_used += sizeof(x);
        return 1;
    }
    return 0;
}
