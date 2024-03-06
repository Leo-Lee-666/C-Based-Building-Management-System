/**
 * Safety Critical System (call point)
 *
 * It was implemented to catch most errors, Except parts that is for input or the author was unaware of.
 * There is while loop that does not finish for process  steps
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "data.h" // data structures stored
#include "shm_struct.h" // shared memories stored
#include "safety.h" // functions that are goint go be used


int main(int argc, char **argv)
{
    /* input check */
    if (argc < 5)
    {
        fprintf(stderr, "{resend delay (in microseconds)} {shared memory path} {shared memory offset} {fire alarm unit address:port}");
        exit(1);
    }

    /* extract informations from input */
    int delay = atoi(argv[1]);
    const char *shm_path = argv[2];
    int shm_offset = atoi(argv[3]);
    const char *fire_alarm_addr = argv[4];

    /* set shared memory */
    char *shm = set_shared_memory(shm_path);
    shm_callpoint *shared = (shm_callpoint *)(shm + shm_offset);

    /* Lock the mutex */
    pthread_mutex_lock(&shared->mutex);

    /* Enter loop */
    while(1)
    {
        /* Check 'status'- if it is '*' */
        if (shared->status == '*')
        {
            /* Send a fire emergency datagram */
            int fire_fd = send_fire(fire_alarm_addr);
            
            /* leep for {resend delay} microseconds */
            usleep(delay);
            
        } else {
            /* Otherwise, wait on 'cond' */
            pthread_cond_wait(&shared->cond, &shared->mutex);
        }
        
    }
    return 0;

}
