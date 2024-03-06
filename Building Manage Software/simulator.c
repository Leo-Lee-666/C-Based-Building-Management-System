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

#define MAX_ADDRESS_PORT_LEN 20
// struc for store info of overseer
typedef struct {
    char address_port[MAX_ADDRESS_PORT_LEN];

    char *door_open_duration;
    char *datagram_resend_delay;
    char *authorisation_file;
    char *connections_file;
    char *layout_file;

    char *shm_path;
    int shm_offset;
}Overseer;

// set firealarm stuct
typedef struct {
    float temperature_threshold;
    int min_detections; 
    int detection_period;
    int reserved;  
}firealarm ;

// set cardreader stuct
typedef struct {
    int id;
    int wait_time;
}cardreader ;

// set door stuct
typedef struct {
    int id;
    char* fail_info;
    int open_close_time;
}door;

// set callpoint stuct
typedef struct {
    int reserved; 
    int resend_delay; 
}callpoint ;


int port_num = 0;
void Scenariofile(const char* scenario_file) {
    FILE* file = fopen(scenario_file, "r");
    if (file == NULL) {
        perror("Error opening file");
        return;
    }
    char line[256];
    
    while (fgets(line, sizeof(line), file) != NULL) {
        char* token = strtok(line, " ");
        // recognize if token is INIT and set into token
        if (strcmp(token, "INIT") != 1){
            token = strtok(NULL, " \n");
            if (token != NULL) { 

                char* value = token + strlen(token) + 1;
                int base_port = 3000;
                
                if (strcmp(token, "overseer") == 0){
                    Overseer overseer;
                    printf("overseer: %s\n", value);
                    sprintf(overseer.address_port, "127.0.0.1:%d", base_port);

                    overseer.shm_path = "/shm_overseer";
                    overseer.shm_offset = 0;

                    char *token = strtok(strdup(value), " ");
                    overseer.door_open_duration = strdup(token);
                    token = strtok(NULL, " ");
                    overseer.datagram_resend_delay = strdup(token);
                    token = strtok(NULL, " ");
                    overseer.authorisation_file = strdup(token);
                    token = strtok(NULL, " ");
                    overseer.connections_file = strdup(token);
                    token = strtok(NULL, " ");
                    overseer.layout_file = strdup(token);


                    
                    int shm_fd = shm_open("/shm_overseer", O_CREAT | O_RDWR, 0666);
                    if (shm_fd == -1) {
                        perror("shm_open");
                        exit(1);
                    }

                    size_t size = sizeof(Overseer);
                    if (ftruncate(shm_fd, size) == -1) {
                        perror("ftruncate");
                        exit(1);
                    }

                    Overseer *shm_ptr = (Overseer *)mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
                    if (shm_ptr == MAP_FAILED) {
                        perror("mmap");
                        exit(1);
                    }

                    memcpy(shm_ptr, &overseer, sizeof(Overseer));
                    

                }else if(strcmp(token, "firealarm") == 0){
                    printf("firealarm: %s\n", value);

                    

                }else if(strcmp(token,"cardreader") == 0){
                    printf("cardreader: %s\n", value);
                    

                }else if(strcmp(token,"door") == 0){      
                    printf("door: %s\n", value);                  
                    

                }else if(strcmp(token,"callpoint") == 0){
                    printf("callpoint: %s\n", value);
                    
                }
            }
        }
        port_num ++;
    }

    fclose(file);
}



int main(int argc, char **argv)
{

    if (argc != 2) 
    {
        fprintf(stderr, "{scenario file}\n");
        exit(1);
    }

    const char *scenario_file = argv[1];
    printf("title: %s\n", scenario_file);
    // read Scenariofile and tried to store into each informations and failed!
    Scenariofile(scenario_file);
    

    return 0;

}

    


