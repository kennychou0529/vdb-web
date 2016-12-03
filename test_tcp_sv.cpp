#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <signal.h>
#include "tcp.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define SV_PORT 27042
#define CL_PORT 27043

void on_ctrlc(int)
{
    printf("Ctrl+C\n");
    tcp_close();
    exit(1);
}

int main()
{
    signal(SIGINT, on_ctrlc);
    assert(tcp_sv_accept(SV_PORT));
    assert(tcp_nonblock(true));

    while (true)
    {
        printf("Tick\n");
        {
            char buffer[1024];
            int bytes = tcp_recv(buffer, sizeof(buffer));
            if (bytes > 0)
            {
                printf("Got %d bytes\n", bytes);
            }
            else if (bytes < 0)
            {
                printf("Connection closed\n");
                break;
            }
        }
        Sleep(1000);
    }
}
