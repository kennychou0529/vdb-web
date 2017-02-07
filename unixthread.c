/*
http://stackoverflow.com/questions/13274786/how-to-share-memory-between-process-fork

wait: Used to wait for child process to terminate
*/

#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h>

void sleepms(int ms)
{
    useconds_t us = ms*1000;
    usleep(us);
}

struct shared_t
{
    int x;
    int port;
};

static shared_t *shared = 0;

void recv_thread()
{
    while (1)
    {
        shared->x++;
        sleepms(50);
    }
}

int main()
{
    shared = (shared_t*)mmap(NULL, sizeof(shared_t), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (shared == MAP_FAILED)
    {
        printf("mmap failed!\n");
    }

    pid_t pid = fork();
    if (pid == 0) // is child
    {
        recv_thread();
    }
    else
    {
        if (pid == -1)
        {
            printf("fork failed\n");
        }
        for (int i = 0; i < 10; i++)
        {
            printf("x = %d\n", shared->x);
            sleepms(100);
        }
        munmap(shared, sizeof(shared_t));
    }
}
