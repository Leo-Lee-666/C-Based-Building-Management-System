/**
 * Safety Critical System (door)
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

#include "shm_struct.h" // shared memories stored
#include "safety.h" // functions that are goint go be used

#define BUFFER_SIZE 1023

int main(int argc, char **argv)
{
    /* input check */
    if (argc < 7)
    {
        fprintf(stderr, "{id} {address:port} {FAIL_SAFE | FAIL_SECURE} {shared memory path} {shared memory offset} {overseer address:port}");
        exit(1);
    }

    /* extract informations from input */
    int id = atoi(argv[1]);
    const char *door_addr = argv[2];
    const char *fail_info = argv[3];
    char *shm_path = argv[4];
    int shm_offset = atoi(argv[5]);
    const char *overseer_addr = argv[6];
    
    /* set shared memory */
    char *shm = set_shared_memory(shm_path);
    shm_door *shared = (shm_door *)(shm + shm_offset);
   
    /* connect to TCP overseer */
    int overseer_fd = tcp_connect(overseer_addr);

    /* store Door information to formattedString and send formattedString through tcp connection 
    * depends on conditions, after process end, close*/
    char formattedString[256]; 
    if ((strcmp(fail_info, "FAIL_SECURE") == 0) || (strcmp(fail_info, "FAIL_SAFE") == 0))
    {
        sprintf(formattedString, "DOOR %i %s %s#", id, door_addr, fail_info);  
        if (send(overseer_fd, formattedString, strlen(formattedString), 0)== -1)
        {
            perror("send()");
            exit(1);
        }
        close(overseer_fd);
    }

    /* set informtations for TCP receive */
    int clientfd;
    struct sockaddr clientaddr;
    socklen_t addrlen;

    /* listen tcp */
    int fd = listen_tcp(door_addr);
    
    /* set informtations for situation adjustment */
    int close_status = 0;
    char buffer[BUFFER_SIZE];
    char *tcpOut;

    /* enter to loop */
    while(1)
    {
        /* accept TCP and receive data from TCP and store data into buffer*/
        clientfd = accept(fd, &clientaddr, &addrlen);
        if (clientfd==-1) 
        {
            perror("accept()");
            exit(1);
        }
        
        int bytesRcv = recv(clientfd, buffer, BUFFER_SIZE,0);
        if (bytesRcv==-1) 
        {
            perror("bytesrcv");
            exit(1);
        }

        buffer[bytesRcv] ='\0';
        
        /* Lock the mutex, retrieve the current door status, then unlock it */
        pthread_mutex_lock(&shared->mutex);
        char currentStatus = shared->status;
        pthread_mutex_unlock(&shared->mutex);

        /* If the buffer is OPEN# and currentStatus is O, respond with ALREADY#, close the connection 
        * or 
        * If the buffer is CLOSE# and currentStatus is C, respond with ALREADY#, close the connection 
        * and EMERGENCY_MODE adjustment action if close_status  is 1*/
        if ((strcmp(buffer, "OPEN#") == 0 && currentStatus == 'O') || (strcmp(buffer, "CLOSE#") == 0 && currentStatus == 'C'))
        {
            tcpOut = "ALREADY#"; 

            /* all further CLOSE# requests with EMERGENCY_MODE# */
            if (strcmp(buffer, "CLOSE#") == 0  && close_status == 1)
            {
                tcpOut = "EMERGENCY_MODE#";
                pthread_mutex_lock(&shared->mutex);
                shared->status = 'o';
                currentStatus = shared->status;
                pthread_cond_signal(&shared->cond_start);
                pthread_cond_wait(&shared->cond_end, &shared->mutex);
                pthread_mutex_unlock(&shared->mutex);
            }
             
            /* respond with dataToSend and close connection*/
            char *dataToSend = tcpOut;
            if (send(clientfd, dataToSend, strlen(dataToSend), 0) == -1)
            {
                perror("send()");
                exit(1);
            }
            close (clientfd);
            
        }

        /* If the buffer is OPEN# and the currentStatus is C */
        if ((strcmp(buffer, "OPEN#") == 0 && currentStatus == 'C') )
        {
            /* Respond with OPENING# */
            tcpOut = "OPENING#";
            char *dataToSend = tcpOut;
            if (send(clientfd, dataToSend, strlen(dataToSend), 0) == -1)
            {
                perror("send()");
                exit(1);
            }
            
            /* Lock the mutex ->  Set the door status to o -> Signal cond_start 
            * -> Wait on cond_end -> Unlock the mutex */
            pthread_mutex_lock(&shared->mutex);
            shared->status = 'o';
            currentStatus = shared->status;
            pthread_cond_signal(&shared->cond_start);
            pthread_cond_wait(&shared->cond_end, &shared->mutex);
            pthread_mutex_unlock(&shared->mutex);

            /* Respond with OPENED#  and close connection*/
            tcpOut = "OPENED#";
            char *dataToSend1 = tcpOut;
            if (send(clientfd, dataToSend1, strlen(dataToSend1), 0) == -1)
            {
                perror("send()");
                exit(1);
            }
            close (clientfd);
            
        }

        /* If the buffer is CLOSE# and the currentStatus is O */
        if ((strcmp(buffer, "CLOSE#") == 0 && currentStatus == 'O'))
        {
            /* close_status = means non emergency */
            if (close_status == 0)
            {   
                /* Respond with CLOSING# */
                tcpOut = "CLOSING#";
                char *dataToSend = tcpOut;
                if (send(clientfd, dataToSend, strlen(dataToSend), 0) == -1)
                {
                    perror("send()");
                    exit(1);
                }
            }
            
            /* Lock the mutex ->  Set the door status to depend on close_status -> Signal cond_start 
            * -> Wait on cond_end -> Unlock the mutex */
            pthread_mutex_lock(&shared->mutex);
            shared->status = 'o';
            if (close_status == 0)
            {
                shared->status = 'c';
            }
            currentStatus = shared->status;
            pthread_cond_signal(&shared->cond_start);
            pthread_cond_wait(&shared->cond_end, &shared->mutex);
            pthread_mutex_unlock(&shared->mutex);


            /* set respond depends on close_status*/
            tcpOut = "EMERGENCY_MODE#";
            if (close_status == 0)
            {
                tcpOut = "CLOSED#";
            }

            /* send respond depends on close_status and close connection*/
            char *dataToSend1 = tcpOut;
            if (send(clientfd, dataToSend1, strlen(dataToSend1), 0) == -1)
            {
                perror("send()");
                exit(1);
            }
            close (clientfd);
            
        }

        /* If the buffer is OPEN_EMERG# and the currentStatus is C */
        if ((strcmp(buffer, "OPEN_EMERG#") == 0 && currentStatus == 'C') )
        {

            /* Lock the mutex ->  Set the door status to o -> Signal cond_start 
            * -> Wait on cond_end -> Unlock the mutex */
            pthread_mutex_lock(&shared->mutex);
            shared->status = 'o';
            currentStatus = shared->status;
            pthread_cond_signal(&shared->cond_start);
            pthread_cond_wait(&shared->cond_end, &shared->mutex);
            pthread_mutex_unlock(&shared->mutex);

            /* set close_status to emergency num for futher loop process*/
            close_status = 1;
            tcpOut = "EMERGENCY_MODE#";
            char *dataToSend1 = tcpOut;

            /* Respond with EMERGENCY_MODE# and close connection*/
            if (send(clientfd, dataToSend1, strlen(dataToSend1), 0) == -1)
            {
                perror("send()");
                exit(1);
            }
            close (clientfd);
            
        }

        /* If the buffer is OPEN_EMERG# and the currentStatus is O */
        if ((strcmp(buffer, "OPEN_EMERG#") == 0 && currentStatus == 'O') )
        {
            /* set close_status to emergency num for futher loop process*/
            close_status = 1;
            tcpOut = "EMERGENCY_MODE#";
            char *dataToSend1 = tcpOut;

            /* Respond with EMERGENCY_MODE# and close connection*/
            if (send(clientfd, dataToSend1, strlen(dataToSend1), 0) == -1)
            {
                perror("send()");
                exit(1);
            }
            close (clientfd);
            
        }

    }

    /* close connections */
    close (clientfd);
    close(fd);
    return 0;

}
