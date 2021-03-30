//#define NDEBUG

#include "ipc.h"
#include "debug.h"

// Socket backing file
#define SOCKET_FILE_PATH BASE_PATH SOCKET_NAME

//File descriptor for the listener socket
static int listener_fd = -1;

/**
 * @brief Initialize the listener socket.
 * 
 * Should only be called if the listener doesn't exist.
 * 
 * @return (int) On success, 0. Otherwise, -1. 
 */
static int init_listener(void)
{
    int rv = 0;

    // Create a UNIX socket
    listener_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if(listener_fd < 0){
        ERROR_PRINT("Error creating listener socket  - %s", strerror(errno));
        rv = -1;
        goto exit;
    }

    // Set the backing file as the "address" for the socket
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(struct sockaddr_un));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_FILE_PATH, sizeof(addr.sun_path)-1);

    // Backing file is created on bind()
    if(bind(listener_fd, (struct sockaddr*)&addr, sizeof(struct sockaddr_un)) < 0){
        ERROR_PRINT("Error binding listener socket  - %s", strerror(errno));
        rv = -1;
        goto destroy_socket;
    }

    // Set socket to listen for incomming connections
    if(listen(listener_fd, 10) < 0){
        ERROR_PRINT("Error setting listener socket to listen mode  - %s", strerror(errno));
        rv = -1;
        goto destroy_socket;
    }

    goto exit;

destroy_socket:
    close(listener_fd);
    listener_fd = -1;
    unlink(SOCKET_FILE_PATH);
exit:
    return rv;
}

/********************* PUBLIC API *********************/

/**
 * @brief Block until another process attempts to connect, and accept the connection. 
 * 
 * For the other process to connect to this one, it must connect() to the backing file.
 * 
 * @return (int) On success, fd of the socket that communicates with the other process. Otherwise, -1.  
 */
int wait_connection(void)
{
    int rv = 0;

    // Check if previous backing file still exists
    if(listener_fd < 0 && access(SOCKET_FILE_PATH, F_OK) == 0)
        unlink(SOCKET_FILE_PATH);

    // Create listener socket if it doesnt exist
    if(listener_fd < 0){
        if(init_listener() < 0){
            ERROR_PRINT("Error initializing listener socket.");
            rv = -1;
            goto exit;
        }
    }

    //Accept incomming connections on socket
    rv = accept4(listener_fd, NULL, NULL, SOCK_CLOEXEC);
    if(rv < 0)
        ERROR_PRINT("Error accepting incoming connection - %s", strerror(errno));
    else
        DEBUG_PRINT("Success opening connection.");

exit:
    return rv;
}

/**
 * @brief Close the listener socket (stop accepting new connections), and delete the backing file.
 * 
 * Should not be called while there is still a live connection between processes.
 * 
 */
void close_listener(void)
{
    if(listener_fd > 0){
        close(listener_fd);
        listener_fd = -1;
        unlink(SOCKET_FILE_PATH);
    }
}

/**
 * @brief Initialize the double buffer for sharing the position of the axis.
 * 
 * @param buff (in) Pointer to the buffer. 
 */
void init_pos_buffer(pos_buffer_t* buff)
{
    if(buff != NULL)
        memset(buff, 0, sizeof(pos_buffer_t));
}

/**
 * @brief Push a new position to the buffer.
 * 
 * @param buff (in) Pointer to the buffer
 * @param pos (in) New position value.
 */
void pos_buffer_push(pos_buffer_t* buff, double pos)
{
    if(buff == NULL)
        return;

    buff->w_flag = 1;
    buff->data[buff->r_ptr ^ 1].position = pos;
    clock_gettime(CLOCK_MONOTONIC, &buff->data[buff->r_ptr ^ 1].timestamp);
    buff->new_available = 1;
    buff->w_flag = 0;
}

/**
 * @brief Read the most recent position from the buffer.
 * 
 * @param buff (in) Pointer to the buffer.
 * @param data (out) Pointer into which to store the position data.
 */
void pos_buffer_pop(pos_buffer_t* buff, pos_data_t* data)
{
    if(!buff->w_flag && buff->new_available){
        buff->r_ptr ^= 1;
        buff->new_available = 0;
    }
    
    memcpy(data, &buff->data[buff->r_ptr], sizeof(pos_data_t));
}
