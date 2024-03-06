/**
 * Safety Critical System (fire alarm)
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
#define ADDR_LEN 100

int main(int argc, char **argv)
{

    /* input check */
    if (argc < 9){
        fprintf(stderr, "{address:port} {temperature threshold} {min detections} {detection period (in microseconds)} {reserved argument} {shared memory path} {shared memory offset} {overseer address:port}");
        exit(1);
    }

    /* extract informations from input */
    const char *addr = argv[1];
    int temperature_threshold = atoi(argv[2]);
    int min_detections = atoi(argv[3]);
    int detection_period = atoi(argv[4]);
    int reserved_argument = atoi(argv[5]);
    char *shm_path = argv[6];
    int shm_offset = atoi(argv[7]);
    const char *overseer_addr = argv[8];

    /* set shared memory */
    char *shm = set_shared_memory(shm_path);
    shm_alarm *shared = (shm_alarm *)(shm + shm_offset);
    
    /* bind udp */
    int sockfd = bind_udp(addr);

    /* connect to TCP overseer */
    int connection_fd = tcp_connect(overseer_addr);

    /* store FIREALARM information to formattedString */
    char formattedString[256]; 
    sprintf(formattedString, "FIREALARM %s HELLO#", addr);  
    char *dataToSend2 = formattedString;

    /* send dataToSend2 through tcp connection */
    if (send(connection_fd, dataToSend2, strlen(dataToSend2), 0) == -1)
    {
        perror("send()");
        exit(1);
    }

    /* set informtations for UDP receive */
    struct sockaddr_in client_addr;
    char buffer[BUFFER_SIZE];
    socklen_t addr_size;

    /* set informtations for situation adjustment */
    int num_door = 0;
    int si_case = 0;
    struct in_addr addrs[ADDR_LEN];
    in_port_t ports[ADDR_LEN];

    /* enter to loop */
    while(1)
    {
        /* receive data from UDP and store data into buffer*/
        bzero(buffer, BUFFER_SIZE);
        addr_size = sizeof(client_addr);
        int messageSize = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr*)&client_addr, &addr_size);
        buffer[messageSize]='\0';

        /* if buffer is FIRE, Lock the mutex -> Set 'alarm' to A
         * -> Unlock the mutex -> Signal cond -> TCP connect and send OPEN_EMERG# */
        if (strcmp(buffer, "FIRE") == 0 && si_case == 0)
        {
           
            pthread_mutex_lock(&shared->mutex);
            shared->alarm = 'A';
            pthread_mutex_unlock(&shared->mutex);
            pthread_cond_signal(&shared->cond);
            
            /* TCP connect and send OPEN_EMERG# */
            for(int i = 0; i < num_door; i++) 
            {
                open_door(ntohs(ports[i]));
            }
            si_case = 1;
        
        }

        /* if buffer is DOOR, Lock the mutex -> Set door registration datagram
         * -> If the door is not already in the door list and if si_case is 1 
         *  TCP connect and send OPEN_EMERG#*/
        else if (memcmp(buffer, "DOOR", 4) == 0) 
        {
            /* Set door registration datagram */
            door_Data door_datagram;
            memcpy(&door_datagram, buffer, sizeof(door_Data));
            struct in_addr door_addr = door_datagram.door_addr;
            in_port_t door_port = door_datagram.door_port;

            /*door check and put from door list*/
            int door_check = 0;
            for (int i = 0; i < num_door; i++) 
            {
                if(memcmp(&addrs[i], &door_addr, sizeof(door_addr)) == 0 && ports[i] == door_port) 
                {
                    door_check = 1; 
                    break;
                }
            }
            
            /* TCP connect and send OPEN_EMERG# */
            if (si_case == 1)
            {
                open_door(ntohs(door_port));
            }

            /*If the door are in the door list, Send a door confirmation datagram to the overseer*/
            if (!door_check) 
            {
                addrs[num_door] = door_addr;
                ports[num_door] = door_port;
                num_door++;
                dreg_Data dreg_door = {{'D', 'R', 'E', 'G'}, door_addr, door_port};
                sendto(sockfd, &dreg_door, sizeof(dreg_door), 0, (struct sockaddr*)&client_addr, sizeof(client_addr));
            }
          
        } 
        
    }
    
    /* close connections */
    close(connection_fd);
    close(sockfd);
    return 0;
 
}









