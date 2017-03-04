#ifdef VDB_UNIX
void vdb_unix_atexit()
{
    if (vdb_shared)
    {
        kill(vdb_shared->recv_pid, SIGTERM);
        kill(vdb_shared->send_pid, SIGTERM);
    }
}
#endif

int vdb_begin()
{
    if (!vdb_shared)
    {
        #ifdef VDB_UNIX
        vdb_shared = (vdb_shared_t*)mmap(NULL, sizeof(vdb_shared_t),
                                         PROT_READ|PROT_WRITE,
                                         MAP_SHARED|MAP_ANONYMOUS, -1, 0);
        if (vdb_shared == MAP_FAILED)
            vdb_shared = 0;
        #else
        vdb_shared = (vdb_shared_t*)calloc(sizeof(vdb_shared_t),1);
        #endif

        if (!vdb_shared)
        {
            vdb_err_once("Tried to allocate too much memory, try lowering VDB_RECV_BUFFER_SIZE and VDB_SEND_BUFFER_SIZE\n");
            return 0;
        }

        #ifdef VDB_UNIX
        atexit(vdb_unix_atexit); // We want to terminate any child processes when we terminate

        vdb_critical(pipe(vdb_shared->ready) != -1);
        vdb_critical(pipe2(vdb_shared->done, O_NONBLOCK) != -1);

        // Create recv process
        vdb_shared->recv_pid = fork();
        vdb_critical(vdb_shared->recv_pid != -1);
        if (vdb_shared->recv_pid == 0)
        {
            signal(SIGTERM, SIG_DFL); // Clear inherited signal handlers
            vdb_recv_thread();
            _exit(0);
        }
        vdb_signal_data_sent(); // Needed for first vdb_end call

        #else
        vdb_shared->send_semaphore = CreateSemaphore(0, 0, 1, 0);
        CreateThread(0, 0, vdb_win_recv_thread, NULL, 0, 0);
        #endif

        vdb_shared->work_buffer = vdb_shared->swapbuffer1;
        vdb_shared->send_buffer = vdb_shared->swapbuffer2;
        // Remaining parameters should be initialized to zero by calloc or mmap
    }
    if (vdb_shared->critical_error)
    {
        vdb_err_once("You must restart your program to use vdb.\n");
        return 0;
    }
    if (!vdb_shared->has_connection)
    {
        return 0;
    }
    vdb_shared->work_buffer_used = 0;
    vdb_begin_submission();
    return 1;
}

void vdb_end()
{
    vdb_shared_t *vs = vdb_shared;
    if (vdb_poll_data_sent()) // Check if send_thread has finished sending data
    {
        vdb_end_submission();

        char *new_work_buffer = vs->send_buffer;
        vs->send_buffer = vs->work_buffer;
        vs->bytes_to_send = vs->work_buffer_used;
        vs->work_buffer = new_work_buffer;
        vs->work_buffer_used = 0;

        // Notify sending thread that data is available
        vdb_signal_data_ready();
    }
}

int vdb_loop(int fps)
{
    static int entry = 1;
    if (entry)
    {
        while (!vdb_begin())
        {
        }
        entry = 0;
    }
    else
    {
        vdb_end();
        vdb_sleep(1000/fps);
        if (vdb_shared->status.flag_continue)
        {
            vdb_shared->status.flag_continue = 0;
            entry = 1;
            return 0;
        }
        while (!vdb_begin())
        {
        }
    }
    return 1;
}
