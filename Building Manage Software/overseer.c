#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <pthread.h>

// shared memory of shm_overseer
struct {
    char security_alarm; // '-' if inactive, 'A' if active
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} shm_overseer;

// struct for store data of Authorisation file information and interaction information
int access_code_num = 0;
int doorpp [10];
int floorpp[10];
typedef struct{
    
    char* access_code[10];
    int floors[10][10]; 
    int doors[10][10]; 
    int floor_num;
    int door_num;
   
} Authorisation_f;


// struct for store connection file
typedef struct {
    int card_reader_id;
    int door_id;
} connection_f;

// struct of ThreadArgs for store input
struct ThreadArgs {
    char *port;
    char *output;
    char *shm_path;
    int shm_offset;
    const char *authorisation_file;
    const char *connection_file;
    int door_open_duration;
};


// read Authorization File 
Authorisation_f authorization;
void AuthorizationFile(const char* authorisation_file) 
{
    FILE* file = fopen(authorisation_file, "r");
    if (file == NULL) 
    {
        perror("Error opening file");
        return;
    }
    char line[256];
    // store informations 
    while (fgets(line, sizeof(line), file) != NULL) 
    {
        char* token = strtok(line, " ");
        char* access_code = token;

        authorization.access_code[access_code_num] = strdup(access_code);
        
        authorization.floor_num = 0;
        authorization.door_num = 0;

        while ((token = strtok(NULL, " ")) != NULL) 
        {
            if (strncmp(token, "DOOR:", 5) == 0) 
            {
                int door = atoi(token + 5);
                authorization.doors[access_code_num][authorization.door_num++] = door;
            } else if (strncmp(token, "FLOOR:", 6) == 0) {
                int floor = atoi(token + 6);
                authorization.floors[access_code_num][authorization.floor_num++] = floor;
            }  
        }
        
        doorpp[access_code_num] = authorization.door_num;
        floorpp[access_code_num] = authorization.floor_num;

        access_code_num ++;
    }
    fclose(file); // close file connection 
}

// read connection File
connection_f database[100]; 
int dataCount = 0; 
void connectionFile(const char* connection_file)
{
    FILE* file = fopen(connection_file, "r");
    if (file == NULL) {
        perror("Error opening file");

    }

    char line[256];
    while (fgets(line, sizeof(line), file) != NULL) 
    {
        char command[10];
        int card_reader_id, door_id;
        // storing DOOR interactions
        if (sscanf(line, "%s %d %d", command, &card_reader_id, &door_id) == 3) 
        {
            if (strcmp(command, "DOOR") == 0) 
            {
  
                database[dataCount].card_reader_id = card_reader_id;
                database[dataCount].door_id = door_id;
                dataCount++;
            } 
        }
    }



    fclose(file);

}

// set tcp door connection and send dore status after that interact in while loop from recieved informations
void TCP_door(in_port_t ports, char *dataToSendo ) 
{
    int st = 0;
    int st1 = 0;
    if (dataToSendo == "CLOSE#")
    {
        st = 1;
    }

    struct sockaddr_in connect_addr;
    int connection_fd1 = socket(AF_INET, SOCK_STREAM, 0);
    if(connection_fd1 == -1) 
    {
        perror("socket()");
        exit(1);
    }

    memset(&connect_addr, 0, sizeof(connect_addr));
    connect_addr.sin_family = AF_INET;
    connect_addr.sin_port = htons(ports);
    if (inet_pton(AF_INET, "127.0.0.1", &connect_addr.sin_addr) != 1)
    {
        perror("inet_pton");
    }

    if(connect(connection_fd1, (struct sockaddr*)&connect_addr, sizeof(connect_addr)) == -1) 
    {
        perror("connect()");
        close(connection_fd1);
        return;
    }


    if(send(connection_fd1, dataToSendo, strlen(dataToSendo), 0) == -1) 
    {
        perror("send()");
        close(connection_fd1);
        return;
    }

    char buffer[1024];
    
    while (st == 0)
    {
        int bytesRcv = recv(connection_fd1, buffer, sizeof(buffer), 0);
        if (bytesRcv == -1) {
            perror("recv()");
        } else {
            buffer[bytesRcv] = '\0';
        }
        if (strcmp(buffer, "OPENING#") == 0) {
            st1 = 1;
        } else if (st1 == 1 && strcmp(buffer, "OPENED#") == 0) {
            st = 1;
            if (shutdown(connection_fd1, SHUT_RDWR) == -1){
                    perror("shutdown()");
                    return;
            }
            close(connection_fd1);
        }
    }
    st = 0;
    st1 = 0;

}



#define BUFFER_SIZE 1023

void *Overseer(void *args) 
{
    // extract informations from ThreadArgs
    struct ThreadArgs *threadArgs = (struct ThreadArgs *)args;
    char *Port = threadArgs->port;
    char *Output = threadArgs->output;
    char *shm_path = threadArgs->shm_path;
    int shm_offset = threadArgs->shm_offset;
    const char *authorisation_file = threadArgs->authorisation_file;
    const char *connection_file = threadArgs->connection_file;
    int door_open_duration = threadArgs->door_open_duration;

    // set socket and init buffer
    int clientfd;
    char buffer[BUFFER_SIZE];
    struct sockaddr clientaddr;
    socklen_t addrlen;
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd==-1) {
        perror("\nsocket()\n");
        return NULL;
    }

    // set addess informtion
    struct sockaddr_in serverAddress;
    memset(&serverAddress, '\0', sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr=htonl(INADDR_ANY);
    serverAddress.sin_port = htons(atoi(Port));

    //bid socket and set listen
    if (bind(fd, (struct sockaddr *)&serverAddress, sizeof(serverAddress))==-1)
    {
        perror("bind()");
        return NULL;
    }
    if (listen(fd, 10)==-1) {
        perror("listen()");
        return NULL;
    }
    int door_l_num = 0;
    
    while (1) 
    {
        // accept and recieve data form client
        clientfd = accept(fd, &clientaddr, &addrlen);
        if (clientfd==-1) 
        {
            perror("accept()");
            return NULL;
        }
        int bytesRcv = recv(clientfd, buffer, 1023,0);
        if (bytesRcv==-1) 
        {
            perror("bytesrcv");
            return NULL;
        }
        buffer[bytesRcv] ='\0';

        //parse door info
        char Door_door[256]; 
        int Door_Id;
        char Door_ip[256];
        int Door_port;
        char Door_result[256];
        
        int door_port_list[20];

        if (sscanf(buffer, "%s %d %[^:]:%d %s", Door_door, &Door_Id, Door_ip, &Door_port, Door_result) == 5) 
        {
            door_port_list[door_l_num] = Door_port;
            door_l_num++;  
        } 

        // set data for extract further
        char code[256];
        char num[256];
        int door_id[256];
        int scanned_pos = -1;
        
        //scan buffer and interact
        char *scanned = strstr(buffer, "SCANNED");

        //extract information from buffer
        if (scanned != NULL) {
            scanned_pos = scanned - buffer;
            int num_pos = scanned_pos - 2; 
            while (num_pos >= 0 && buffer[num_pos] != ' ') 
            {
                num_pos--;
            }
            if (num_pos >= 0) 
            {
                int end_pos = scanned_pos - 1;
                sscanf(&buffer[num_pos], " %s %*s %s", num, code);

                code[strlen(code) - 1] = '\0';
            }

            // bring authorisation_file info
            AuthorizationFile(authorisation_file);
            
            int j= 0;
            int i = 0;
            int step1 = 0;
            int step2 = 0;
            int measure = 0;
            //finding and comparing in complex for loop and decide step1 value
            for (int t = 0; t < access_code_num; t++) {
                if (strcmp(code, authorization.access_code[t]) == 0)
                {
                    step1 = 1;
                    measure = t;
                    for (int j = 0; j < doorpp[t]; j++ ) 
                    {
                        door_id[j] = authorization.doors[t][j];
                    }
                    
                }
                
            }
            access_code_num = 0;


            //read connection_file
            connectionFile(connection_file);

            //finding and comparing in complex for loop and decide step2 value
            for (int i = 0; i < dataCount; i++) {
                if (atoi(num) == database[i].card_reader_id) 
                {
                    for (int j = 0; j < 10; j++) 
                    {
                        if (door_id[j] == database[i].door_id) 
                        {
                            
                            step2 = 1;
                        }
                    }
                    
                }
            }
            dataCount = 0;
            in_port_t D_P;

            // if scanned card reader info is in the auth file and connection file start process
            if (step1 == 1 && step2 == 1)
            {
                step1 = 0;
                step2 = 0;

                // set ALLOWED at output and respond through send
                char output[] = "ALLOWED#";
                strcat(output, buffer); 

                int bytesSent = send(clientfd, output, strlen(output), 0);
                if (bytesSent == -1) 
                {
                    perror("send()");
                    return NULL;
                }
                
                // send OPEN and wait door_open_duration and send CLOSE by door_port_list
                for (int i = 0; i < door_l_num; i++)
                {
                    D_P = door_port_list[i];
                    TCP_door(D_P, "#");
                    usleep(door_open_duration);
                    TCP_door(D_P, "CLOSE#");  
                } 
            } else {
                // respond DENIED if there is no cardreader's information in auth file and connection file
                char output[] = "DENIED#";
                    strcat(output, buffer); 

                int bytesSent = send(clientfd, output, strlen(output), 0);
                if (bytesSent == -1) 
                {
                    perror("send()");
                    return NULL;
                }
            }

        }
        //close connection 
        close (clientfd);
    } 
    //shutdown and close connection 
    if (shutdown(clientfd, SHUT_RDWR) ==-1) 
    {
        perror("shutdown()");
        return NULL;
    }
    //close connection and return
    close(fd);

    return NULL;
}




int main(int argc, char **argv)
{
    // check input
    if (argc < 9)
    {
        fprintf(stderr, "{address:port} {door open duration (in microseconds)} {datagram resend delay (in microseconds)} {authorisation file} {connections file} {layout file} {shared memory path} {shared memory offset}");
        exit(1);
    }

    //extract input
    const char *addr = argv[1];
    int door_open_duration = atoi(argv[2]);
    int datagram_resend_delay = atoi(argv[3]);
    const char *authorisation_file = argv[4];
    const char *connection_file = argv[5];
    const char *layout_file = argv[6];
    char *shm_path = argv[7];
    int shm_offset = atoi(argv[8]);
    

    //set pthread_t and args interaction informations
    pthread_t overseer_Thread;

    struct ThreadArgs args;
    
    args.door_open_duration = door_open_duration;
    args.connection_file = connection_file;
    args.authorisation_file = authorisation_file;
    args.shm_path = shm_path;
    args.shm_offset = shm_offset;


    //extract port 
    const char *colon_pos = strchr(addr, ':');
    if (colon_pos != NULL) 
    {
        char port[256];
        strncpy(port, colon_pos + 1, sizeof(port) - 1);
        port[sizeof(port) - 1] = '\0';
        args.port = malloc(strlen(port) + 1);
        strcpy(args.port, port);

        //make thread and run
        if (pthread_create(&overseer_Thread, NULL, Overseer, (void *)&args) != 0) 
        {
            perror("pthread_create");
            return 1;
        }
        // wait until finish
        pthread_join(overseer_Thread, NULL);
    }

    return 0;
}
