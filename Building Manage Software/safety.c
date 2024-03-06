
#include "safety.h" // Safety-critical link 
#include "data.h" // data structures stored

void *set_shared_memory(const char *shm_path) 
{
    // set and open shared memory file
    int shm_fd = shm_open(shm_path, O_CREAT | O_RDWR, 0);
    if (shm_fd == -1)
    {
        perror("shm_open()");
        exit(1);
    }
    // bring share memory info
    struct stat shm_stat;
    if (fstat(shm_fd, &shm_stat) == -1)
    {
        perror("fstat()");
        exit(1);
    }
    // put shared memory into process
    char *shm = mmap(NULL, shm_stat.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shm == MAP_FAILED)
    {
        perror("mmap()");
        exit(1);
    }

    // return pointer of shared memory
    return shm;
}


int tcp_connect(const char* overseer_addr) 
{
    char address[40];
    int port;

    // extract address and port from overseer_addr
    if (sscanf(overseer_addr, "%39[^:]:%d", address, &port) != 2)
    {
        perror("sscanf");
        exit(1);
    }
    
    // creat tcp socket
    int connection_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (connection_fd == -1) 
    {
        perror("socket()"); 
        exit(1);
    }

    // set info of address
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    //  chage to network address from address
    if (inet_pton(AF_INET, address, &addr.sin_addr) != 1) 
    {
        perror("inet_pton");
        exit(1);
    }
    // connect
    if (connect(connection_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1)
    {
        perror("connect()");
        exit(1);
    }

    // return tcp socket discript
    return connection_fd;
}


int send_fire(const char*  fire_alarm_addr)
{
    char address[40];
    int port;
    // extract address and port from fire_alarm_addr
    if (sscanf(fire_alarm_addr, "%39[^:]:%d", address, &port) != 2)
    {
        perror("sscanf");
        exit(1);
    }
    
    // creat udp socket
    int sockfd;
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) 
    {
        perror("socket()");
        exit(1);
    }

    // set info of address
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_pton(AF_INET, address, &addr.sin_addr) != 1) 
    {
        perror("inet_pton");
        exit(1);
    }

    //put FIRE into fire_emergency_data and send
    fire_emergency_data fire_datagram;
    fire_datagram.header[0] = 'F';
    fire_datagram.header[1] = 'I';
    fire_datagram.header[2] = 'R';
    fire_datagram.header[3] = 'E';
    sendto(sockfd, &fire_datagram, sizeof(fire_datagram), 0, (struct sockaddr *)&addr, sizeof(addr));
    // close socket
    close(sockfd);

    // return socket discript
    return sockfd;
}
 


int listen_tcp(const char* address_port) 
{
    char address[40];
    int port;
    // extract address and port from address_port
    if (sscanf(address_port, "%39[^:]:%d", address, &port) != 2)
    {
        perror("sscanf");
        exit(1);
    }
    
    // creat tcp socket
    int fd = socket(AF_INET, SOCK_STREAM, 0); 
    if (fd == -1) 
    {
        perror("\nsocket()\n");
        exit(1);
    }

    // set info of address
    struct sockaddr_in serverAddress;
    memset(&serverAddress, '\0', sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(port);
    if (inet_pton(AF_INET, address, &serverAddress.sin_addr) != 1) 
    {
        perror("inet_pton");
        exit(1);
    }

    // bind socket at specified address and port
    if (bind(fd, (struct sockaddr *)&serverAddress, sizeof(serverAddress))==-1)
    {
        perror("bind()");
        exit(1);
    }
    
    // set socket to listen
    if (listen(fd, 10)==-1) 
    {
        perror("listen()");
        exit(1);
    }

    return fd;
}
    

int bind_udp(const char* address_port) 
{
    char address[40];
    int port;
    // extract address and port from address_port
    if (sscanf(address_port, "%39[^:]:%d", address, &port) != 2)
    {
        perror("sscanf");
        exit(1);
    }

    // creat udp socket
    int sockfd;
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) 
    {
        perror("[-]socket error");
        exit(1);
    }
    // set info of address
    struct sockaddr_in server_addr;
    memset(&server_addr, '\0', sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, address, &server_addr.sin_addr) != 1) 
    {
        perror("inet_pton");
        exit(1);
    }
    // bind socket at specified address and port
    if (bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr))==-1) 
    {
        perror("[-]bind error");
        exit(1);
    }

    return sockfd;
}
    
void open_door(in_port_t ports) 
{
    // creat tcp socket
    struct sockaddr_in connect_addr;
    int connection_fd1 = socket(AF_INET, SOCK_STREAM, 0);
    if(connection_fd1 == -1) 
    {
        perror("socket()");
        exit(1);
    }
    // set info of address
    memset(&connect_addr, 0, sizeof(connect_addr));
    connect_addr.sin_family = AF_INET;
    connect_addr.sin_port = htons(ports);
    if (inet_pton(AF_INET, "127.0.0.1", &connect_addr.sin_addr) != 1)
    {
        perror("inet_pton");
    }
    //connect tcp
    if(connect(connection_fd1, (struct sockaddr*)&connect_addr, sizeof(connect_addr)) == -1) 
    {
        perror("connect()");
        close(connection_fd1);
        return;
    }


    // respond with "OPEN_EMERG#" through socket
    char *dataToSendo = "OPEN_EMERG#";

    if(send(connection_fd1, dataToSendo, strlen(dataToSendo), 0) == -1) 
    {
        perror("send()");
        close(connection_fd1);
        return;
    }
    

    // shutdown and close socket
    if (shutdown(connection_fd1, SHUT_RDWR) == -1)
    {
            perror("shutdown()");
            return;
    }

    close(connection_fd1);

}
