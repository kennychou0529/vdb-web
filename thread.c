#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <winsock.h>
#pragma comment(lib, "wsock32.lib")

static HANDLE send_semaphore = 0;
static volatile LONG send_buffer_busy = 0;
static char *send_buffer = 0;

// Reads from TCP until a full message has been received.
DWORD WINAPI recv_thread(void *vdata)
{

    return 0;
}

// Blocks until main thread signals that there is data to be sent.
// It then sends the data and signals when it's done.
DWORD WINAPI send_thread(void *vdata)
{
    int i = 0;
    for (i = 0; i < 10; i++)
    {
        printf("[send] waiting for data...\n");
        WaitForSingleObject(send_semaphore, INFINITE);
        while (send_buffer_busy)
        {
            // block
        }
        send_buffer_busy = 1;
        if (send_buffer)
        {
            int j;
            printf("[send] sending '%s'", send_buffer);
            for (j = 0; j < 10; j++)
            {
                printf(".");
                Sleep(200);
            }
            printf("done\n");
        }
        send_buffer_busy = 0;
    }
    return 0;
}

void vdb_end()
{
    if (InterlockedCompareExchange(&send_buffer_busy, 1, 0) == 0) // if busy==0, busy=1 { }
    {
        printf("[main] sending data\n");
        send_buffer = "Hello world";
        // todo: memory fence
        send_buffer_busy = 0;
        ReleaseSemaphore(send_semaphore, 1, 0);
    }
}

int main()
{
    send_semaphore = CreateSemaphore(0, 0, 1, 0);
    CreateThread(0, 0, recv_thread, NULL, 0, 0);
    CreateThread(0, 0, send_thread, NULL, 0, 0);

    vdb_end();
    Sleep(3000);

    return 0;
}
