/*
http://pubs.opengroup.org/onlinepubs/009695399/xrat/xsh_chap02.html#tag_03_02_08_06
https://support.microsoft.com/en-us/help/181611/socket-overlapped-i-o-versus-blocking-nonblocking-mode

IO ports
http://stackoverflow.com/questions/2794535/linux-and-i-o-completion-ports
https://en.wikipedia.org/wiki/Overlapped_I/O
https://en.wikipedia.org/wiki/Asynchronous_I/O
https://msdn.microsoft.com/en-us/library/windows/desktop/ms740087(v=vs.85).aspx
https://msdn.microsoft.com/en-us/library/windows/desktop/ms740115(v=vs.85).aspx

epoll
https://banu.com/blog/2/how-to-use-epoll-a-complete-example-in-c/
Also shows how to do nonblocking accept

general
http://davmac.org/davpage/linux/async-io.html
http://www.wangafu.net/~nickm/libevent-book/01_intro.html
http://stackoverflow.com/questions/16281468/write-to-the-client-returns-ewouldblock-when-server-is-slow
http://www.win32developer.com/tutorial/winsock/winsock_tutorial_7.shtm
http://www.win32developer.com/tutorial/winsock/winsock_tutorial_4.shtm
*/

#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <winsock.h>
#pragma comment(lib, "wsock32.lib")

/*
http://www.kegel.com/c10k.html
http://beej.us/guide/bgnet/output/html/singlepage/bgnet.html
http://beej.us/guide/bgnet/output/html/singlepage/bgnet.html#sendall
https://tangentsoft.net/wskfaq/articles/io-strategies.html
https://tangentsoft.net/wskfaq/examples/basics/AsyncClient/index.html

http://www.winsocketdotnetworkprogramming.com/winsock2programming/winsock2advancediomethod5e.html

For stream-mode:

We want:
* Non-blocking tcp_accept
* Asynchronous recv and send, where we tell the OS to start recv/send
  and get notified upon completion.

I guess we could do non-completion notification async io:

    void on_ready_to_send() // an interrupt routine!
    {
        if (remaining_bytes > 0)
            remaining_bytes -= send(...)
    }

For interactive-mode:
We actually want blocking accept, recv and send. I think.
*/

/*
void vdb_end()
{
    if (!currently_sending && time() - last_time > interval)
    {
        send();
        last_time = time();
    }
}
*/

static int client_socket = 0;
static int listen_socket = 0;
static int has_listen_socket = 0;

int tcp_listen(uint16_t port)
{
    struct sockaddr_in address;
    struct WSAData wsa;
    listen_socket = 0;
    has_listen_socket = 0;
    if (WSAStartup(MAKEWORD(2,2), &wsa) != NO_ERROR)
    {
        printf("Failed to initialize TCP socket.\n");
        return 0;
    }
    listen_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listen_socket == INVALID_SOCKET)
    {
        WSACleanup();
        printf("Failed to create a TCP socket.\n");
        return 0;
    }
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);
    if (bind(listen_socket, (const struct sockaddr*)&address, sizeof(address)) == SOCKET_ERROR)
    {
        closesocket(listen_socket);
        WSACleanup();
        printf("Failed to bind socket to port %d. Try a different port?\n", port);
        return 0;
    }
    if (listen(listen_socket, SOMAXCONN) == SOCKET_ERROR)
    {
        closesocket(listen_socket);
        WSACleanup();
        printf("Failed to begin listening for clients.\n");
        return 0;
    }
    has_listen_socket = 1;
    return 1;
}

int tcp_wait_accept()
{
    if (!has_listen_socket)
    {
        printf("Dont have listen socket.\n");
        return 0;
    }
    client_socket = accept(listen_socket, 0, 0);
    if (client_socket == INVALID_SOCKET)
    {
        printf("Could not accept client socket.\n");
        return 0;
    }
    else
    {
        return 1;
    }
}

int tcp_try_accept()
{
    fd_set read_set;
    struct timeval timeout;
    if (!has_listen_socket)
    {
        printf("Don't have listen socket.\n");
        return 0;
    }
    FD_ZERO(&read_set);
    FD_SET(listen_socket, &read_set);
    timeout.tv_sec = 0; // Zero timeout (poll)
    timeout.tv_usec = 0;
    if (select(listen_socket, &read_set, NULL, NULL, &timeout) == 1)
    {
        printf("A client wants to connect\n");
        client_socket = accept(listen_socket, 0, 0);
        if (client_socket == INVALID_SOCKET)
        {
            printf("Could not accept client socket.\n");
            return 0;
        }
        else
        {
            return 1;
        }
    }
    return 0;
}

int tcp_set_non_blocking(int enabled)
{
    DWORD word = (enabled) ? 1 : 0;
    if (ioctlsocket(client_socket, FIONBIO, &word) != 0)
    {
        printf("Failed to set TCP to nonblocking.\n");
        return 0;
    }
    return 1;
}

int main()
{
    int has_client_socket = 0;
    int i;
    if (!tcp_listen(8000))
        return 0;
    for (i = 0; i < 100; i++)
    {
        if (tcp_try_accept())
        {
            printf("Got a client\n");
            tcp_set_non_blocking(1);
            has_client_socket = 1;
        }
        if (has_client_socket)
        {
            static char buffer[1024*1024] = {0};
            int read_bytes = recv(client_socket, (char*)buffer, sizeof(buffer), 0);
            if (read_bytes > 0)
            {
                printf("[%d]: '%s'\n", read_bytes, buffer);
            }
            else
            {
                int error = WSAGetLastError();
                if (error == WSAEWOULDBLOCK)
                {
                    // no bytes received?
                }
                else
                {
                    printf("Client dropped\n");
                    closesocket(client_socket);
                    has_client_socket = 0;
                }
            }
        }
        Sleep(500);
    }
}
