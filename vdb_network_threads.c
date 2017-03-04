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

        // send frame header
        vdb_form_frame(vs->bytes_to_send, &frame, &frame_len);
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
                vdb_log("listen failed\n");
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
            char *response;
            int response_len;
            vdb_log("Waiting for handshake\n");
            if (!tcp_recv(vs->recv_buffer, VDB_RECV_BUFFER_SIZE, &read_bytes))
            {
                vdb_log("lost connection during handshake\n");
                tcp_shutdown();
                vdb_sleep(1000);
                continue;
            }

            vdb_log("Generating handshake\n");
            if (!vdb_generate_handshake(vs->recv_buffer, read_bytes, &response, &response_len))
            {
                static char http_response[1024*1024];
                const char *content = "<html>In the future I'll also send the vdb application to you over the browser.<br>For now, you need to open the app.html file in your browser.</html>";

                // If we failed to generate a handshake, it means that either
                // we did something wrong, or the browser did something wrong,
                // or the request was an ordinary HTTP request. If the latter
                // we'll send an HTTP response containing the vdb.html page.
                int len = sprintf(http_response,
                    "HTTP/1.1 200 OK\r\n"
                    "Content-Length: %d\r\n"
                    "Content-Type: text/html\r\n"
                    "Connection: Closed\r\n\r\n%s",
                    (int)strlen(content),
                    content
                    );

                vdb_log("Failed to generate handshake. Sending HTML page.\n");
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
