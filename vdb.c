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

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "tcp.c"
#include "websocket.c"
#if defined(_WIN32) || defined(_WIN64)
#include "vdb_win.c"
#else
#warning "Todo: Implement nice user-feedback logging for unix"
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
