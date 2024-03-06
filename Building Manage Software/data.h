#ifndef DATA_H
#define DATA_H

// fire alarm datagram 
typedef struct{
    char header[4]; // {'F', 'I', 'R', 'E'}
}fire_emergency_data;

// door register datagram 
typedef struct {
    char header[4]; // {'D', 'O', 'O', 'R'}
    struct in_addr door_addr;
    in_port_t door_port;
}door_Data;

// door confirm datagram 
typedef struct {
    char header[4]; // {'D', 'R', 'E', 'G'}
    struct in_addr door_addr;
    in_port_t door_port;
}dreg_Data;

#endif