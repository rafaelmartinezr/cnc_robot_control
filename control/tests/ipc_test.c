//#define NDEBUG

//TODO: Add error checking
//TODO: Fix signal handler (has something to do with select)
//TODO: Move communications to own file

#include "Stepper.h"
#include "Axis.h"
#include "Time.h"
#include "config.h"
#include "ipc.h"
#include "debug.h"

#define BASE_PATH "/home/nvidia/pef_pr21/"
#define MSG_BUFF_SIZE 256

typedef enum cmds{
    CMD_MOVE = 0x01,
    CMD_STOP = 0x02,
    CMD_FINISH = 0x03,
    CMD_GETPOS = 0x04,
    CMD_PARAMS = 0x05
} cmd_t;

static pid_t zed_pid = 0;
static pid_t lidar_pid = 0;

static Axis* x_axis = NULL;

static volatile int stop = 0;

static int zed_socket = 0;
static int lidar_socket = 0;
static int flask_socket = 0;

static int e_stop_fd = 0;

static void sigint_handler(int sig)
{
    DEBUG_PRINT("Ctrl-C catched.");
    stop = 1;
}

static int start_py_process(const char* py_name, int* socket_fd)
{
    pid_t py_pid = fork();
    int rv = -1;
    
    if(py_pid > 0){
        // Im am the parent
        
        DEBUG_PRINT("%s forked successfully.", py_name);

        *socket_fd = wait_connection();
        if(*socket_fd < 0){
            ERROR_PRINT("Error establishing comms with process %s.", py_name);
        } else{
            rv = 0; 
        }

    } else if (py_pid == 0){
        // Im am the child

        char py_filepath[256];
        py_filepath[255] = 0;
        strncpy(py_filepath, BASE_PATH, sizeof(py_filepath)-1);
        strncat(py_filepath, py_name, sizeof(py_filepath)-sizeof(BASE_PATH)-1);

        char* argv[] = {"/usr/bin/python3", py_filepath, NULL};
        if(execv("/usr/bin/python3", argv) < 0){
            ERROR_PRINT("Error execv'ing python program - %s", strerror(errno));
            exit(-1);
        }

    } else{
        // Error
        ERROR_PRINT("Error forking python program - %s", strerror(errno));
    }

    return rv;
}

static int cmd_move(const char* data, int len)
{
    double speed = *(double*)&data[0];
    double distance = *(double*)&data[sizeof(double)];

    axis_set_speed(x_axis, speed);
    axis_move(x_axis, distance);

    return 0;
}

static int cmd_getpos(const char* data, int len)
{
    DEBUG_PRINT("Cmd getpos recieved.");

    //TODO: Add error checking
    double pos = axis_get_position(x_axis);
    struct timespec t_now;
    clock_gettime(CLOCK_MONOTONIC, &t_now);

    char response[64];
    size_t offset = 0;

    response[offset++] = 25;
    response[offset++] = CMD_GETPOS;
    memcpy(&response[offset], &pos, sizeof(double));
    offset += sizeof(double);
    memcpy(&response[offset], &t_now.tv_sec, sizeof(time_t));
    offset += sizeof(time_t);
    memcpy(&response[offset], &t_now.tv_nsec, sizeof(long));
    offset += sizeof(long);

    write(lidar_socket, response, offset);

    return 0;
}

static int read_message(int fd, char* buff)
{
    buff[0] = 255;
    int offset = 0;

    do{
        offset += read(fd, &buff[offset], MSG_BUFF_SIZE);
        buff[offset] = 0;
    } while(offset < buff[0]);

    return (buff[0] != 0) ? 0 : -1;
}

static int decode_message(const char* msg)
{
    int n = msg[0];
    char cmd = msg[1];
    const char* data = &msg[2];
    char dest = 0;

    int retval = 0;

    switch(cmd){
        case CMD_MOVE:
            retval = cmd_move(data, n-2);
            break;
        
        case CMD_STOP:
            axis_stop(x_axis);
            break;
        
        case CMD_FINISH:
            if(data[0] == 0)
                axis_wait(x_axis);
            else
                axis_stop(x_axis);
            stop = 1;
            break;

        case CMD_GETPOS:
            retval = cmd_getpos(data, n-2);
            break;
        
        case CMD_PARAMS:
            dest = data[29];
            if(dest == 1 || dest == 0)
                write(lidar_socket, msg, n);
            if(dest == 2 || dest == 0)
                write(zed_socket, msg, n);
            break;

        default:
            ERROR_PRINT("Unknown command received from partner process.");
            retval = -1;
            break;
    }

    return retval;
}

static int init_system(int argc, char const *argv[])
{
    int rv = -1;
    
    // First in the list is the name of the program
    // Second is zed 
    // Third is lidar
    if(argc != 3){
        ERROR_PRINT("Incorrect number of arguments given.");
        goto exit;
    }

    const char* zed_py_name = argv[1];
    const char* lidar_py_name = argv[2];

    // Config and start motor pins
    if(read_motor_config() < 0){
        ERROR_PRINT("Could not read motor configuration.");
        goto exit;
    }

    DEBUG_PRINT("Motor config read successfully.");

    x_axis = get_axis_by_name("x-axis");
    if(x_axis == NULL){
        ERROR_PRINT("Axis x-axis not found in configuration.");
        goto exit;
    }

    DEBUG_PRINT("Axis x-axis initialized successfully.");

    // Initialize GPIO for emergency stop and limit switches
    // TODO: Add limit switches
    GPIO_Pin* emer_stop = GPIO_init_pin(J21_HEADER_PIN_37, GPIO_DIRECTION_NONE, 0);
    if(gpiod_line_request_rising_edge_events(emer_stop, "PEF") < 0){
        ERROR_PRINT("Could not initialize emergency stop input.");
        goto exit;
    }
    e_stop_fd = gpiod_line_event_get_fd(emer_stop);

    DEBUG_PRINT("Emergency stop initialized successfully.");

    // Set handler for user interrupt
    signal(SIGINT, sigint_handler);

    // // Connect with flask process
    // DEBUG_PRINT("Connecting to flask...");

    // flask_socket = wait_connection();
    // if(flask_socket < 0){
    //     ERROR_PRINT("Error starting comms with FLASK.");
    //     goto exit;
    // }

    // DEBUG_PRINT("Flask comms established successfully.");
    
    // // Start zed process
    // DEBUG_PRINT("Starting ZED process...");

    // if(start_py_process(zed_py_name, &zed_socket) < 0){
    //     ERROR_PRINT("Error starting ZED process.");
    //     goto exit;
    // }

    // DEBUG_PRINT("ZED process started successfully.");


    // Start lidar process
    DEBUG_PRINT("Starting LIDAR process...");

    if(start_py_process(lidar_py_name, &lidar_socket) < 0){
        ERROR_PRINT("Error starting LIDAR process.");
        goto exit;
    }
    
    DEBUG_PRINT("LIDAR process started successfully.");

    rv = 0;

exit:
    return rv;
}

static void cleanup(void){
    close(lidar_socket);
    close(zed_socket);
    close(flask_socket);
    close_listener();
}

int main(int argc, char const *argv[])
{
    // Initialize everything
    if(init_system(argc, argv) < 0){
        ERROR_PRINT("Error initializing system.");
        cleanup();
        exit(-1);
    }

    // Main communication loop
    fd_set read_set;
    char msg[MSG_BUFF_SIZE];

    const unsigned int socket_list[] = {lidar_socket};
    const unsigned int socket_list_len = sizeof(socket_list)/sizeof(socket_list[0]);

    while(!stop){
        FD_ZERO(&read_set);
        FD_SET(e_stop_fd, &read_set);
        for(unsigned int i = 0; i < socket_list_len; i++)
            FD_SET(socket_list[i], &read_set);

        int n = select(FD_SETSIZE, &read_set, NULL, NULL, NULL);
        if(n > 0){
            if(FD_ISSET(e_stop_fd, &read_set)){
                DEBUG_PRINT("Emergency stop pressed. Exiting.");
                stop = 1;
            } else{
                int rdy_sock = 0;
                for(unsigned int i = 0; i < socket_list_len && !rdy_sock; i++)
                    rdy_sock = FD_ISSET(socket_list[i], &read_set) ? socket_list[i] : 0;

                if(read_message(rdy_sock, msg) < 0){
                    ERROR_PRINT("Error reading incomming message.");
                    stop = 1;
                    continue;
                }

                if(decode_message(msg) < 0){
                    ERROR_PRINT("Error decoding recieved message.");
                    stop = 1;
                }
            }
        } else{
            ERROR_PRINT("Error on select - %s.", strerror(errno));
            stop = 1;
        }
    }

    DEBUG_PRINT("Cleaning up...");
    cleanup();
        
    return 0;
}
