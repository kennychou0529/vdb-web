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
