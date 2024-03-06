#ifndef SAFETY_H
#define SAFETY_H

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
#include "safety.c" // functions that are goint go be used


/**
 * set_shared_memory: set shared memory
 */
void *set_shared_memory(const char *shm_path);

/**
 * tcp_connect: connect to tcp but not closed
 */
int tcp_connect(const char* overseer_addr);

/**
 * send_fire: send fire_datagram through udp and set fire_datagram to 'FIRE'
 */
int send_fire(const char*  fire_alarm_addr);

/**
 * listen_tcp: listen tcp but not closed
 */
int listen_tcp(const char* address_port);

/**
 * bind_udp: bind_udp but not closed
 */
int bind_udp(const char* address_port);

/**
 * open_door: respond OPEN_EMERG through tcp connection
 */
void open_door(in_port_t ports);

#endif