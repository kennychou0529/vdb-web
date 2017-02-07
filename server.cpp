#define vdb_assert(EXPR) if (!(EXPR)) { printf("[error]\n\tAssert failed at line %d in file %s:\n\t'%s'\n", __LINE__, __FILE__, #EXPR); return 0; }

#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <signal.h> // for signal
#include <math.h>   // for demo

#include "websocket.c"
#include "tcp.c"

int vdb_send_handshake(const char *request, int request_len)
{
    char accept_key[1024];
    char response[1024];
    int response_len;
    vdb_assert(vdb_generate_accept_key(request, request_len, accept_key));
    response_len = sprintf(response,
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: %s\r\n\r\n",
        accept_key
    );
    tcp_send(response, response_len);
    return 1;
}

int vdb_wait_for_handshake()
{
    static char request[1024];
    int request_len = 0;
    vdb_assert(tcp_recv(request, sizeof(request), &request_len));
    vdb_assert(vdb_send_handshake(request, request_len));
    return 1;
}

int vdb_self_test()
{
    size_t request_len;
    char accept_key[1024];
    const char *request =
        "GET /chat HTTP/1.1\r\n"
        "Host: server.example.com\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
        "Sec-WebSocket-Protocol: chat, superchat\r\n"
        "Sec-WebSocket-Version: 13\r\n"
        "Origin: http://example.com\r\n\r\n";
    request_len = strlen(request);
    vdb_generate_accept_key(request, request_len, accept_key);
    vdb_assert(strcmp(accept_key, "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=") == 0);
    return 1;
}

void vdb_print_bytes(void *recv_buffer, int n)
{
    int index;
    for (index = 0; index < n; index++)
    {
        unsigned char c = ((unsigned char*)recv_buffer)[index];
        printf("%d:\t", index);
        for (int i = 7; i >= 4; i--) printf("%d", (c >> i) & 1);
        printf(" ");
        for (int i = 3; i >= 0; i--) printf("%d", (c >> i) & 1);
        printf("\n");
    }
}

int vdb_send(const void *data, int length)
{
    uint8_t frame[16] = {0};
    int frame_length = 0;
    {
        // fin rsv1 rsv2 rsv3 opcode
        // 1   0    0    0    0010
        frame[0] = 0x82;
        if (length <= 125)
        {
            frame[1] = (uint8_t)(length & 0xFF);
            frame_length = 2;
        }
        else if (length <= 65535)
        {
            frame[1] = 126;
            frame[2] = (uint8_t)((length >> 8) & 0xFF);
            frame[3] = (uint8_t)((length >> 0) & 0xFF);
            frame_length = 4;
        }
        else
        {
            frame[1] = 127;
            frame[2] = 0; // @ assuming length is max 32 bit
            frame[3] = 0; // @ assuming length is max 32 bit
            frame[4] = 0; // @ assuming length is max 32 bit
            frame[5] = 0; // @ assuming length is max 32 bit
            frame[6] = (length >> 24) & 0xFF;
            frame[7] = (length >> 16) & 0xFF;
            frame[8] = (length >>  8) & 0xFF;
            frame[9] = (length >>  0) & 0xFF;
            frame_length = 10;
        }
    }
    vdb_assert(tcp_send(frame, frame_length));
    vdb_assert(tcp_send(data, length));
    return 1;
}

struct vdb_msg_t
{
    char *payload;
    int length;
    int fin;
    int opcode;
};

int vdb_parse_message(uint8_t *recv_buffer, int received, vdb_msg_t *msg)
{
    // https://tools.ietf.org/html/rfc6455#section-5.4
    // Note: WebSocket does not send fields unless
    // they are needed. For example, extended len
    // is not sent if the len fits inside payload
    // len.
    int i = 0;
    uint8_t key[4] = {0};
    uint32_t opcode = ((recv_buffer[i  ] >> 0) & 0xF);
    uint32_t fin    = ((recv_buffer[i++] >> 7) & 0x1);
    uint64_t len    = ((recv_buffer[i  ] >> 0) & 0x7F);
    uint32_t mask   = ((recv_buffer[i++] >> 7) & 0x1);
    vdb_assert(mask == 1); // client messages must be masked
    if (len == 126)
    {
        vdb_assert(i + 2 <= received);
        len = 0;
        len |= recv_buffer[i++]; len <<= 8;
        len |= recv_buffer[i++];
    }
    else if (len == 127)
    {
        vdb_assert(i + 8 <= received);
        len = 0;
        len |= recv_buffer[i++]; len <<= 8;
        len |= recv_buffer[i++]; len <<= 8;
        len |= recv_buffer[i++]; len <<= 8;
        len |= recv_buffer[i++]; len <<= 8;
        len |= recv_buffer[i++]; len <<= 8;
        len |= recv_buffer[i++]; len <<= 8;
        len |= recv_buffer[i++]; len <<= 8;
        len |= recv_buffer[i++];
    }

    vdb_assert(len <= (uint64_t)received);
    vdb_assert(i + 4 <= received);
    key[0] = recv_buffer[i++];
    key[1] = recv_buffer[i++];
    key[2] = recv_buffer[i++];
    key[3] = recv_buffer[i++];

    // decode payload
    {
        int j = 0;
        vdb_assert(i + len <= received);
        for (j = 0; j < len; j++)
            recv_buffer[i+j] = recv_buffer[i+j] ^ key[j % 4];
        recv_buffer[i+len] = 0;
    }

    msg->payload = (char*)(recv_buffer + i);
    msg->length = (int)len;
    msg->opcode = (int)opcode;
    msg->fin = (int)fin;

    // print payload
    {
        #if 0
        printf("[vdb] received %d bytes, first ten are:\n", received);
        vdb_print_bytes(recv_buffer, 10);
        printf("fin :\t%u\n", fin);
        printf("code:\t%u\n", opcode);
        printf("mask:\t%u\n", mask);
        printf("len :\t%d bytes\n", *out_length);
        printf("data:\t'%s'\n", *out_payload);
        #endif
    }
    return 1;
}

static HANDLE send_semaphore = 0;
static volatile LONG send_buffer_busy = 0;
static volatile int vdb_should_close = 0;
static int bytes_to_send = 0;

static int vdb_initialized = 0;
static int vdb_has_client = 0;
static int recv_buffer_size = 0;
static int work_buffer_size = 0;
static int work_buffer_used = 0;
static uint8_t *recv_buffer = 0;
static uint8_t *work_buffer = 0;
static uint8_t *send_buffer = 0;
static uint16_t vdb_sv_port = 8000; // @ sv port should be constant, or selected automatically?

DWORD WINAPI recv_thread(void *vdata)
{
    while (!vdb_should_close)
    {
        if (!vdb_has_client)
        {
            printf("[vdb] waiting for connection.\n");
            if (!tcp_accept(vdb_sv_port))
            {
                printf("[vdb] tcp_accept failed.\n"); // @ user friendly error message
                continue;
            }
            printf("[vdb] waiting for handshake.\n");
            if (!vdb_wait_for_handshake())
            {
                printf("[vdb] handshake with browser did not work.\n"); // @ user friendly error message
                continue;
            }
            vdb_has_client = 1;
        }
        else
        {
            int received = 0;
            if (!tcp_recv(recv_buffer, recv_buffer_size, &received))
            {
                printf("[vdb] client closed connection.\n");
                vdb_has_client = 0;
                tcp_close();
            }
            else if (received > 0) // @ Incomplete: Assemble frames
            {
                vdb_msg_t msg = {0};
                if (vdb_parse_message(recv_buffer, received, &msg))
                {
                    if (!msg.fin)
                    {
                        printf("[vdb] got an incomplete message (%d): '%s'\n", msg.length, msg.payload);
                    }
                    else
                    {
                        printf("[vdb] got a final message (%d): '%s'\n", msg.length, msg.payload);
                        if (strcmp(msg.payload, "shutdown") == 0)
                        {
                            printf("[vdb] received shutdown signal from browser.\n");
                            vdb_should_close = 1;
                        }
                    }
                }
            }
        }
    }
    return 0;
}

DWORD WINAPI send_thread(void *vdata)
{
    while (!vdb_should_close)
    {
        // printf("[vdb] waiting for data to send.\n");
        WaitForSingleObject(send_semaphore, INFINITE);
        while (send_buffer_busy)
        {
            // block
        }
        send_buffer_busy = 1;
        if (bytes_to_send > 0 && vdb_has_client)
        {
            // printf("[vdb] sending %d bytes.\n", bytes_to_send);
            vdb_send(send_buffer, bytes_to_send); // @ vdb_send might fail, what to do?
            bytes_to_send = 0;
        }
        send_buffer_busy = 0;
    }
    return 0;
}

int vdb_begin() // @ vdb_begin(dt)
{
    if (!vdb_initialized)
    {
        // Allocate buffers
        recv_buffer_size = 1024*1024; // one megabyte
        work_buffer_size = 1024*1024; // one megabyte
        recv_buffer = (uint8_t*)calloc(recv_buffer_size,sizeof(uint8_t));
        work_buffer = (uint8_t*)calloc(work_buffer_size,sizeof(uint8_t));
        send_buffer = (uint8_t*)calloc(work_buffer_size,sizeof(uint8_t));
        vdb_assert(recv_buffer); // @ assert should not crash the program!!!
        vdb_assert(work_buffer); // @ assert should not crash the program!!!
        vdb_assert(send_buffer); // @ assert should not crash the program!!!
        vdb_assert(vdb_self_test()); // @ assert should not crash the program!!!

        vdb_should_close = 0;
        vdb_has_client = 0;
        work_buffer_used = 0;
        bytes_to_send = 0;

        // Spawn threads
        send_semaphore = CreateSemaphore(0, 0, 1, 0);
        CreateThread(0, 0, recv_thread, NULL, 0, 0);
        CreateThread(0, 0, send_thread, NULL, 0, 0);

        vdb_initialized = 1;
    }
    if (!vdb_has_client)
    {
        return 0;
    }
    if (vdb_should_close)
    {
        return 0;
    }
    work_buffer_used = 0;
    return 1;
}

void vdb_end()
{
    if (InterlockedCompareExchange(&send_buffer_busy, 1, 0) == 0) // if busy==0, busy=1 { }
    {
        // swap
        uint8_t *new_work_buffer = send_buffer;
        send_buffer = work_buffer;
        bytes_to_send = work_buffer_used;
        work_buffer = new_work_buffer;
        work_buffer_used = 0;

        // todo: memory fence
        send_buffer_busy = 0;
        ReleaseSemaphore(send_semaphore, 1, 0);
    }
    else
    {
        // printf("[vdb] throttled!\n");
    }
}

int vdb_push_u16(uint16_t x)
{
    vdb_assert(work_buffer);
    if (work_buffer_used + (int)sizeof(x) < work_buffer_size)
    {
        *(uint16_t*)(work_buffer + work_buffer_used) = x;
        work_buffer_used += (int)sizeof(x);
        return 1;
    }
    return 0;
}

int vdb_push_r32(float x)
{
    vdb_assert(work_buffer);
    if (work_buffer_used + (int)sizeof(x) < work_buffer_size)
    {
        *(float*)(work_buffer + work_buffer_used) = x;
        work_buffer_used += (int)sizeof(x);
        return 1;
    }
    return 0;
}

void vdb_on_ctrlc(int)
{
    printf("[vdb] shutting down.\n");
    vdb_should_close = 1;
    exit(1);
}

int main()
{
    signal(SIGINT, vdb_on_ctrlc);
    int running = 1;
    while (running)
    {
        if (vdb_begin())
        {
            float data[2*512];
            uint16_t count = 0;
            #define point(x,y) if (count < 512) { data[2*count+0] = x; data[2*count+1] = y; count++; }

            static float t = 0.0f;
            float pi = 3.14f;
            float two_pi = 2.0f*3.14f;
            float pi_half = 3.14f/2.0f;
            float dur = 3.2f;

            float t1 = 0.0f;
            float t2 = 0.0f;
            if (t < dur/2.0f)
            {
                t1 = 0.5f+0.5f*sinf(pi*t/(dur/2.0f) - pi_half);
                t2 = 0.0f;
            }
            else
            {
                t1 = 1.0f;
                t2 = 0.5f+0.5f*sinf(pi*(t-dur/2.0f)/(dur/2.0f) - pi_half);
            }
            float r1 = 0.55f;
            float r2 = 0.5f;
            float r3 = 0.25f;

            for (int i = 0; i <= 6; i++)
            {
                float r = r1;
                float a = (two_pi/6.0f)*(i*(t1-t2) + 12.0f*t2 - 1.5f);
                point(r*cosf(a), r*sinf(a));
            }

            for (int i = 0; i <= 6; i++)
            {
                float r = r1 + (r2-r1)*t1 - (r2-r1)*t2;
                float a = (two_pi/6.0f)*(i*(t1-t2) + 11.5f*t2 - 1.5f + 0.5f*t1);
                point(r*cosf(a), r*sinf(a));
            }

            for (int i = 0; i <= 6; i++)
            {
                float r = r1 + (r3-r1)*t1 - (r3-r1)*t2;
                float a = (two_pi/6.0f)*(i*(t1-t2) + 6.0f*t1 + 12.0f*t2 - 1.5f);
                point(r*cosf(a), r*sinf(a));
            }

            t += 1.0f/120.0f;
            if (t > dur)
                t -= dur;

            vdb_push_u16(count);
            for (int i = 0; i < 2*count; i++)
                vdb_push_r32(data[i]);
            vdb_end();
        }
        Sleep(1000/120);

        if (vdb_should_close)
            running = 0;
    }
    tcp_close(); // @ vdb_close()
}
