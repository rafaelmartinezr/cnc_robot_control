#ifndef IPC_H
#define IPC_H

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif 

#include "sysconfig.h"

#include <sys/types.h> 
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/un.h>
#include <signal.h> 
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <time.h>

// Name of the unix socket backing file.
#define SOCKET_NAME "sock_bf"

// Structure for storing position data.
typedef struct pos_data{
    double position;
    struct timespec timestamp;
} pos_data_t;

// Double buffer for sharing position information.
typedef struct pos_buffer{
    pos_data_t data[2];
    int new_available;
    int w_flag;
    int r_ptr;
} pos_buffer_t;

/**
 * @brief Block until another process attempts to connect, and accept the connection. 
 * 
 * For the other process to connect to this one, it must connect() to the backing file.
 * 
 * @return (int) On success, fd of the socket that communicates with the other process. Otherwise, -1.  
 */
int wait_connection(void);

/**
 * @brief Close the listener socket (stop accepting new connections), and delete the backing file.
 * 
 * Should not be called while there is still a live connection between processes.
 * 
 */
void close_listener(void);

#endif
