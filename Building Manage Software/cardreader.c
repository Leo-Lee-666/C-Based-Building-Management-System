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

#include "shm_struct.h" // shared memories stored
#include "safety.h" // functions that are goint go be used

#define LEN_STR 256
int main(int argc, char **argv)
{
    // input check
    if (argc < 6)
    {
        fprintf(stderr, "{id} {wait time (in microseconds)} {shared memory path} {shared memory offset} {overseer address:port}");
        exit(1);
    }

    // extract info from input
    int id = atoi(argv[1]);
    // int waittime = atoi(argv[2]);
    const char *shm_path = argv[3];
    int shm_offset = atoi(argv[4]);
    const char *overseer_addr = argv[5];

    //set_shared_memory
    void *shm = set_shared_memory(shm_path);
    shm_cardreader *shared = (shm_cardreader *)(shm + shm_offset);

    // connect to TCP overseer
    int overseer_fd = tcp_connect(overseer_addr);

    // store CARDREADER info to out_string
    char out_string[256];  
    sprintf(out_string, "CARDREADER %i HELLO#", id);

    // send out_string through overseer tcp connection
    if (send(overseer_fd, out_string, strlen(out_string), 0) == -1)
    {
        perror("send()");
        exit(1);
    }
    close(overseer_fd); // close TCP
    
    // Lock the mutex
    pthread_mutex_lock(&shared->mutex);
    // enter to loop
    while(1)
    {
        // If the bytes are all Not \0 (NUL) process steps
        if (shared->scanned[0] != '\0'){
            // store scanned info into buf for further info sending
            char buf[17];
            memcpy(buf, shared->scanned, 16);
            buf[16] = '\0';

            // Open a TCP connection to the overseer, Send the following data
            overseer_fd = tcp_connect(overseer_addr);
            
            // store CARDREADER info to in_string
            char in_string[LEN_STR];  
            sprintf(in_string, "CARDREADER %i SCANNED %s#", id, buf);

            // send in_string through overseer_fd tcp connection
            if (send(overseer_fd, in_string, strlen(in_string), 0) == -1)
            {
                perror("send()");
                exit(1);
            }

            // store receved info to from overseer_fd tcp into buffer
            char buffer[LEN_STR];
            int bytesRcv = recv(overseer_fd, buffer, sizeof(buffer), 0);

            // Anything else set the response char to N
            if (bytesRcv == -1)
            {
                    shared->response = 'N';
            }
            buffer[bytesRcv] = '\0'; // set buffer NULL

            // If the response was ALLOWED#, set the 'response' char to Y and Anything else set the response char to N
            if (strcmp(buffer, "ALLOWED#") == 0) 
            {
                shared->response = 'Y';  
            } else {
                shared->response = 'N';    
            }
            
            pthread_cond_signal(&shared->response_cond); // Signal 'response_cond'

        }   
        pthread_cond_wait(&shared->scanned_cond, &shared->mutex); // Wait on 'scanned_cond'
    }

    close(overseer_fd); // close TCP connection
    return 0;

}