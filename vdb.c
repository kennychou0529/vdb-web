#define vdb_assert(EXPR)  { if (!(EXPR)) { printf("[error]\n\tAssert failed at line %d in file %s:\n\t'%s'\n", __LINE__, __FILE__, #EXPR); return 0; } }
#ifdef VDB_LOG_DEBUG
#define vdb_log(...)      { printf("[vdb] %s@L%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); }
#else
#define vdb_log(...)      { }
#endif
#define vdb_log_once(...) { static int first = 1; if (first) { printf("[vdb] "); printf(__VA_ARGS__); first = 0; } }
#define vdb_err_once(...) { static int first = 1; if (first) { printf("[vdb] Error at line %d in file %s:\n[vdb] ", __LINE__, __FILE__); printf(__VA_ARGS__); first = 0; } }
#define vdb_critical(EXPR) if (!(EXPR)) { printf("[vdb] Something went wrong at line %d in file %s\n", __LINE__, __FILE__); vdb_shared->critical_error = 1; return 0; }

#if defined(_WIN32) || defined(_WIN64)
#define VDB_WINDOWS
#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#define VDB_UNIX
#include <sys/mman.h>
#include <sys/types.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#endif
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

// Draw commands are stored in a work buffer that is allocated
// once on the first vdb_begin call, and stays a fixed size that
// is given below in number-of-bytes. If you are memory constrained,
// or if you need more memory than allocated by default, you can
// define your own work buffer size before #including vdb.
#ifndef VDB_WORK_BUFFER_SIZE
#define VDB_WORK_BUFFER_SIZE (32*1024*1024)
#endif

// Messages received from the browser are stored in a buffer that
// is allocated once on the first vdb_begin call, and stays a fixed
// size is given below in number-of-bytes. If you are sending large
// messages from the browser back to the application you can define
// your own recv buffer size before #including vdb.
#ifndef VDB_RECV_BUFFER_SIZE
#define VDB_RECV_BUFFER_SIZE (1024*1024)
#endif

#ifndef VDB_LISTEN_PORT
#define VDB_LISTEN_PORT 8000
#endif

#if VDB_LISTEN_PORT < 1024 || VDB_LISTEN_PORT > 65535
#error "[vdb] The specified listen port is outside of the valid range (1024-65535)"
#endif

#define VDB_LITTLE_ENDIAN
#if !defined(VDB_LITTLE_ENDIAN) && !defined(VDB_BIG_ENDIAN)
#error "You must define either VDB_LITTLE_ENDIAN or VDB_BIG_ENDIAN"
#endif

#define VDB_LABEL_LENGTH 16
typedef struct
{
    char chars[VDB_LABEL_LENGTH+1];
} vdb_label_t;

#define VDB_MAX_VAR_COUNT 1024
typedef struct
{
    vdb_label_t var_label[VDB_MAX_VAR_COUNT];
    float       var_value[VDB_MAX_VAR_COUNT];
    int         var_count;

    int         flag_continue;

    int         mouse_click;
    float       mouse_click_x;
    float       mouse_click_y;
} vdb_status_t;

typedef struct
{
    #ifdef VDB_WINDOWS
    volatile HANDLE send_semaphore;
    volatile LONG busy;
    volatile int bytes_to_send;
    #else
    int bytes_to_send;
    pid_t recv_pid;
    pid_t send_pid;
    // These pipes are used for flow control between the main thread and the sending thread
    // The sending thread blocks on a read on pipe_ready, until the main thread signals the
    // pipe by write on pipe_ready. The main thread checks if sending is complete by polling
    // (non-blocking) pipe_done, which is signalled by the sending thread.
    int ready[2]; // [0]: read, [1]: send
    int done[2];// [0]: read, [1]: send
    #endif

    int has_send_thread;
    int critical_error;
    int has_connection;
    int work_buffer_used;
    char swapbuffer1[VDB_WORK_BUFFER_SIZE];
    char swapbuffer2[VDB_WORK_BUFFER_SIZE];
    char *work_buffer;
    char *send_buffer;

    char recv_buffer[VDB_RECV_BUFFER_SIZE];

    vdb_status_t status;
} vdb_shared_t;

static vdb_shared_t *vdb_shared = 0;

int vdb_cmp_label(vdb_label_t *a, vdb_label_t *b)
{
    int i;
    for (i = 0; i < VDB_LABEL_LENGTH; i++)
        if (a->chars[i] != b->chars[i])
            return 0;
    return 1;
}

void vdb_copy_label(vdb_label_t *dst, const char *src)
{
    int i = 0;
    while (i < VDB_LABEL_LENGTH && src[i])
    {
        dst->chars[i] = src[i];
        i++;
    }
    while (i < VDB_LABEL_LENGTH)
    {
        dst->chars[i] = ' ';
        i++;
    }
    dst->chars[VDB_LABEL_LENGTH] = 0;
}

#include "tcp.c"
#include "websocket.c"
#include "vdb_handle_message.c"
#include "vdb_network_threads.c"
#include "vdb_push_buffer.c"
#include "vdb_draw_commands.c"
#include "vdb_begin_end.c"
