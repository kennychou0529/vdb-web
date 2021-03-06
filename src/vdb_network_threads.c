#ifdef VDB_WINDOWS
int vdb_wait_data_ready()
{
    WaitForSingleObject(vdb_shared->send_semaphore, INFINITE);
    while (InterlockedCompareExchange(&vdb_shared->busy, 1, 0) == 1)
    {
        vdb_log("CompareExchange blocked\n");
    }
    return 1;
}
int vdb_signal_data_sent()  { vdb_shared->busy = 0; return 1; }
int vdb_poll_data_sent()    { return (InterlockedCompareExchange(&vdb_shared->busy, 1, 0) == 0); }
int vdb_signal_data_ready() { vdb_shared->busy = 0; ReleaseSemaphore(vdb_shared->send_semaphore, 1, 0); return 1; } // @ mfence, writefence
void vdb_sleep(int ms)      { Sleep(ms); }
#else
int vdb_wait_data_ready()   { int val = 0; return  read(vdb_shared->ready[0], &val, sizeof(val)) == sizeof(val); }
int vdb_poll_data_sent()    { int val = 0; return   read(vdb_shared->done[0], &val, sizeof(val)) == sizeof(val); }
int vdb_signal_data_ready() { int one = 1; return write(vdb_shared->ready[1], &one, sizeof(one)) == sizeof(one); }
int vdb_signal_data_sent()  { int one = 1; return  write(vdb_shared->done[1], &one, sizeof(one)) == sizeof(one); }
void vdb_sleep(int ms)      { usleep(ms*1000); }
#endif

int vdb_send_thread()
{
    vdb_shared_t *vs = vdb_shared;
    unsigned char *frame; // @ UGLY: form_frame should modify a char *?
    int frame_len;
    vdb_log("Created send thread\n");
    vdb_sleep(100); // @ RACECOND: Let the parent thread set has_send_thread to 1
    while (!vs->critical_error)
    {
        // blocking until data is signalled ready from main thread
        vdb_critical(vdb_wait_data_ready());

        // send frame header (0x2 indicating binary data)
        vdb_form_frame(vs->bytes_to_send, 0x2, &frame, &frame_len);
        if (!tcp_sendall(frame, frame_len))
        {
            vdb_log("Failed to send frame\n");
            vdb_critical(vdb_signal_data_sent());
            break;
        }

        // send the payload
        if (!tcp_sendall(vs->send_buffer, vs->bytes_to_send))
        {
            vdb_log("Failed to send payload\n");
            vdb_critical(vdb_signal_data_sent());
            break;
        }

        // signal to main thread that data has been sent
        vdb_critical(vdb_signal_data_sent());
    }
    vs->has_send_thread = 0;
    return 0;
}

#ifdef VDB_WINDOWS
DWORD WINAPI vdb_win_send_thread(void *vdata) { (void)(vdata); return vdb_send_thread(); }
#endif

int vdb_strcmpn(const char *a, const char *b, int len)
{
    int i;
    for (i = 0; i < len; i++)
        if (a[i] != b[i])
            return 0;
    return 1;
}

int vdb_is_http_request(const char *str, int len)
{
    // First three characters must be "GET"
    const char *s = "GET";
    int n = (int)strlen(s);
    if (len >= n && vdb_strcmpn(str, s, n))
        return 1;
    return 0;
}

int vdb_is_websockets_request(const char *str, int len)
{
    // "Upgrade: websocket" must occur somewhere
    const char *s = "websocket";
    int n = (int)strlen(s);
    int i = 0;
    for (i = 0; i < len - n; i++)
        if (vdb_strcmpn(str + i, s, n))
            return 1;
    return 0;
}

int vdb_recv_thread()
{
    vdb_shared_t *vs = vdb_shared;
    int read_bytes;
    vdb_msg_t msg;
    vdb_log("Created read thread\n");
    while (!vs->critical_error)
    {
        if (vs->critical_error)
        {
            return 0;
        }
        if (!tcp_has_listen_socket)
        {
            vdb_log("Creating listen socket\n");
            if (!tcp_listen(VDB_LISTEN_PORT))
            {
                vdb_log("Failed to create socket on port %d\n", VDB_LISTEN_PORT);
                vdb_sleep(1000);
                continue;
            }
            vdb_log_once("Visualization is live at host:%d\n", VDB_LISTEN_PORT);
        }
        if (!tcp_has_client_socket)
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
            int is_http_request;
            int is_websocket_request;
            vdb_log("Waiting for handshake\n");
            if (!tcp_recv(vs->recv_buffer, VDB_RECV_BUFFER_SIZE, &read_bytes))
            {
                vdb_log("Lost connection during handshake\n");
                tcp_shutdown();
                vdb_sleep(1000);
                continue;
            }

            is_http_request = vdb_is_http_request(vs->recv_buffer, read_bytes);
            is_websocket_request = vdb_is_websockets_request(vs->recv_buffer, read_bytes);

            if (!is_http_request)
            {
                vdb_log("Got an invalid HTTP request while waiting for handshake\n");
                tcp_shutdown();
                vdb_sleep(1000);
                continue;
            }

            // If it was not a websocket HTTP request we will send the HTML page
            if (!is_websocket_request)
            {
                const char *content = get_vdb_html_page();
                static char http_response[1024*1024];
                int len = sprintf(http_response,
                    "HTTP/1.1 200 OK\r\n"
                    "Content-Length: %d\r\n"
                    "Content-Type: text/html\r\n"
                    "Connection: Closed\r\n\r\n%s",
                    (int)strlen(content), content);

                vdb_log("Sending HTML page.\n");
                if (!tcp_sendall(http_response, len))
                {
                    vdb_log("Lost connection while sending HTML page\n");
                    tcp_shutdown();
                    vdb_sleep(1000);
                    continue;
                }

                // Sending 'Connection: Closed' allows us to close the socket immediately
                tcp_close_client();

                vdb_sleep(1000);
                continue;
            }

            // Otherwise we will set up the Websockets connection
            {
                char *response;
                int response_len;

                vdb_log("Generating WebSockets key\n");
                if (!vdb_generate_handshake(vs->recv_buffer, read_bytes, &response, &response_len))
                {
                    vdb_log("Failed to generate WebSockets handshake key. Retrying.\n");
                    tcp_shutdown();
                    vdb_sleep(1000);
                    continue;
                }

                vdb_log("Sending WebSockets handshake\n");
                if (!tcp_sendall(response, response_len))
                {
                    vdb_log("Connection went down while setting up WebSockets connection. Retrying.\n");
                    tcp_shutdown();
                    vdb_sleep(1000);
                    continue;
                }

                vs->has_connection = 1;
            }
        }
        #ifdef VDB_UNIX
        // The send thread is allowed to return on unix, because if the connection
        // goes down, the recv thread needs to respawn the process after a new
        // client connection has been acquired (to share the file descriptor).
        // fork() shares the open file descriptors with the child process, but
        // if a file descriptor is _then_ opened in the parent, it will _not_
        // be shared with the child.
        if (!vs->has_send_thread)
        {
            vs->send_pid = fork();
            vdb_critical(vs->send_pid != -1);
            if (vs->send_pid == 0)
            {
                signal(SIGTERM, SIG_DFL); // Clear inherited signal handlers
                vdb_send_thread(); // vdb_send_thread sets has_send_thread to 0 upon returning
                _exit(0);
            }
            vs->has_send_thread = 1;
        }
        #else
        // Because we allow it to return on unix, we allow it to return on windows
        // as well, even though file descriptors are shared anyway.
        if (!vs->has_send_thread)
        {
            CreateThread(0, 0, vdb_win_send_thread, NULL, 0, 0); // vdb_send_thread sets has_send_thread to 0 upon returning
            vs->has_send_thread = 1;
        }
        #endif

        if (!tcp_recv(vs->recv_buffer, VDB_RECV_BUFFER_SIZE, &read_bytes)) // @ INCOMPLETE: Assemble frames
        {
            vdb_log("Connection went down\n");
            vs->has_connection = 0;
            tcp_shutdown();
            #ifdef VDB_UNIX
            if (vs->has_send_thread)
            {
                kill(vs->send_pid, SIGUSR1);
                vs->has_send_thread = 0;
            }
            #endif
            vdb_sleep(100);
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
        if (msg.opcode == 0x8) // closing handshake
        {
            unsigned char *frame = 0;
            int frame_len = 0;
            vdb_log("Client voluntarily disconnected\n");
            vdb_form_frame(0, 0x8, &frame, &frame_len);
            if (!tcp_sendall(frame, frame_len))
                vdb_log("Failed to send closing handshake\n");
            continue;
        }
        if (!vdb_handle_message(msg, &vs->status))
        {
            vdb_log("Handled a bad message\n");
            continue;
        }
    }
    return 0;
}

#ifdef VDB_WINDOWS
DWORD WINAPI vdb_win_recv_thread(void *vdata) { (void)(vdata); return vdb_recv_thread(); }
#endif
