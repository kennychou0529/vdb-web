#include <stdio.h>
#include <stdint.h>
#include <assert.h>

#include "tcp.h"

#define SV_PORT 27042
#define CL_PORT 27043

int main()
{
    assert(tcp_cl_connect(127,0,0,1,SV_PORT));

    int bytes;
    {
        char *data = "Hello tcp!";
        bytes = tcp_send(data, strlen(data)+1);
        printf("sent %d bytes\n", bytes);
    }

    {
        char buffer[1024];
        bytes = tcp_recv(buffer, sizeof(buffer));
        printf("read %d bytes\n", bytes);
    }

    tcp_close();
}
