struct vdb_shared_t
{
    volatile HANDLE send_semaphore;
    volatile LONG busy;
    volatile int bytes_to_send;

    int has_connection;
    int work_buffer_used;
    char recv_buffer[VDB_RECV_BUFFER_SIZE];
    char swapbuffer1[VDB_WORK_BUFFER_SIZE];
    char swapbuffer2[VDB_WORK_BUFFER_SIZE];
    char *work_buffer;
    char *send_buffer;
};

static vdb_shared_t *vdb_shared = 0;

void vdb_sleep(int milliseconds) { Sleep(milliseconds); }

DWORD WINAPI recv_thread(void *vdata)
{
    vdb_shared_t *vs = vdb_shared;
    vdb_log("Created recv thread\n");
    int read_bytes;
    vdb_msg_t msg;
    while (1)
    {
        if (!has_listen_socket)
        {
            if (vdb_listen_port == 0)
            {
                vdb_log_once(
                       "Warning: You have not set the listening port for vdb. Using 8000.\n"
                       "You can use a different port by calling vdb_set_listen_port(<port>),\n"
                       "or by #define VDB_LISTEN_PORT <port> before #including vdb.c.\n"
                       "Valid ports are between 1024 and 65535.\n");
                vdb_listen_port = 8000;
            }
            vdb_log("Creating listen socket\n");
            if (!tcp_listen(vdb_listen_port))
            {
                vdb_log("Listen failed\n");
                vdb_sleep(1000);
                continue;
            }
            vdb_log_once("Visualization is live at <Your IP>:%d\n", vdb_listen_port);
        }
        if (!has_client_socket)
        {
            vdb_log("Waiting for client\n");
            if (!tcp_accept())
            {
                vdb_sleep(1000);
                continue;
            }
        }
        if (!vs->has_connection)
        {
            char *response;
            int response_len;
            vdb_log("Waiting for handshake\n");
            if (!tcp_recv(vs->recv_buffer, VDB_RECV_BUFFER_SIZE, &read_bytes))
            {
                vdb_log("Lost connection during handshake\n");
                tcp_shutdown();
                vdb_sleep(1000);
                continue;
            }

            vdb_log("Generating handshake\n");
            if (!vdb_generate_handshake(vs->recv_buffer, read_bytes, &response, &response_len))
            {
                vdb_log("Failed to generate handshake\n");

                // If we failed to generate a handshake, it means that either
                // we did something wrong, or the browser did something wrong,
                // or the request was an ordinary HTTP request. If the latter
                // we'll send an HTTP response containing the vdb.html page.
                char data[1024];
                const char *content = "<html>In the future I'll also send the vdb application to you over the browser.<br>For now, you need to open the app.html file in your browser.</html>";
                int len = sprintf(data,
                    "HTTP/1.1 200 OK\r\n"
                    "Content-Length: %d\r\n"
                    "Content-Type: text/html\r\n"
                    "Connection: Closed\r\n\r\n%s",
                    strlen(content),
                    content
                    );

                if (!tcp_sendall(data, len))
                {
                    vdb_log("Lost connection while sending HTML page\n");
                    tcp_shutdown();
                    vdb_sleep(1000);
                    continue;
                }

                // Sending 'Connection: Closed' allows us to close the socket immediately
                closesocket(client_socket);
                has_client_socket = 0;

                vdb_sleep(1000);
                continue;
            }

            vdb_log("Sending handshake\n");
            if (!tcp_sendall(response, response_len))
            {
                vdb_log("Failed to send handshake\n");
                tcp_shutdown();
                vdb_sleep(1000);
                continue;
            }

            vs->has_connection = 1;
        }
        if (!tcp_recv(vs->recv_buffer, VDB_RECV_BUFFER_SIZE, &read_bytes)) // @ INCOMPLETE: Assemble frames
        {
            vdb_log("Connection went down\n");
            vs->has_connection = 0;
            tcp_shutdown();
            vdb_sleep(1000);
            continue;
        }
        if (!vdb_parse_message(vs->recv_buffer, read_bytes, &msg))
        {
            vdb_log("Got a bad message\n");
            continue;
        }
        if (!msg.fin)
        {
            vdb_log("Got an incomplete message (%d): '%s'\n", msg.length, msg.payload);
            continue;
        }
        vdb_log("Got a final message (%d): '%s'\n", msg.length, msg.payload);
        if (strcmp(msg.payload, "shutdown") == 0)
        {
            // vdb_should_close = 1;
        }
    }
    return 0;
}

DWORD WINAPI send_thread(void *vdata)
{
    vdb_shared_t *vs = vdb_shared;
    unsigned char *frame;
    int frame_len;
    vdb_log("Created send thread\n");
    while (1)
    {
        WaitForSingleObject(vs->send_semaphore, INFINITE);
        while (InterlockedCompareExchange(&vs->busy, 1, 0) == 1)
        {
            vdb_log("CompareExchange blocked\n");
        }

        // send frame header
        vdb_form_frame(vs->bytes_to_send, &frame, &frame_len);
        if (!tcp_sendall(frame, frame_len))
        {
            vdb_log("Failed to send frame\n");
            vs->busy = 0;
            continue;
        }

        // send the payload
        if (!tcp_sendall(vs->send_buffer, vs->bytes_to_send))
        {
            vdb_log("Failed to send payload\n");
            vs->busy = 0;
            continue;
        }
        vs->busy = 0;
    }
    return 0;
}

int vdb_begin() // @ vdb_begin(dt)
{
    if (!vdb_shared)
    {
        vdb_shared = (vdb_shared_t*)calloc(sizeof(vdb_shared_t),1); // zero-initialize
        if (!vdb_shared)
        {
            vdb_err_once("Tried to allocate too much memory, try lowering VDB_RECV_BUFFER_SIZE and VDB_SEND_BUFFER_SIZE.\n");
            return 0;
        }
        vdb_shared->work_buffer = vdb_shared->swapbuffer1;
        vdb_shared->send_buffer = vdb_shared->swapbuffer2;
        vdb_shared->send_semaphore = CreateSemaphore(0, 0, 1, 0);
        CreateThread(0, 0, recv_thread, NULL, 0, 0);
        CreateThread(0, 0, send_thread, NULL, 0, 0);
    }
    if (!vdb_shared->has_connection)
    {
        return 0;
    }
    vdb_cmdbuf->num_line2 = 0;
    vdb_cmdbuf->num_line3 = 0;
    vdb_cmdbuf->num_point2 = 0;
    vdb_cmdbuf->num_point3 = 0;
    // @ command buffer initialization
    vdb_cmdbuf->color = 0;
    return 1;
}

void vdb_end()
{
    // The following does an atomic compare and exchange, that is
    // it compare busy with 0 (if busy==0) and replaces it with 1
    // if that was the case, in one atomic operation. If busy was
    // zero, it means that we have taken control of the send buffer
    // and we can perform the swap.
    if (InterlockedCompareExchange(&vdb_shared->busy, 1, 0) == 0)
    {
        vdb_shared->work_buffer_used = 0;
        if (!vdb_serialize_cmdbuf())
        {
            vdb_log_once("Too much geometry was drawn. Try increasing the work buffer size.\n");
            vdb_shared->work_buffer_used = 0;
        }

        char *new_work_buffer = vdb_shared->send_buffer;
        vdb_shared->send_buffer = vdb_shared->work_buffer;
        vdb_shared->bytes_to_send = vdb_shared->work_buffer_used;
        vdb_shared->work_buffer = new_work_buffer;
        vdb_shared->work_buffer_used = 0;

        // Relinquish control of the send buffer @ TODO: mfence
        vdb_shared->busy = 0;

        // Notify the sending thread that data is available
        ReleaseSemaphore(vdb_shared->send_semaphore, 1, 0);
    }
    else
    {
        // printf("[vdb] throttled!\n");
    }
}
