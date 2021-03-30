/*
 * Stepper.c
 * 
 * Author: Rafael Martinez
 * Date: 07.03.2021
 */

#define NDEBUG

#include "Stepper.h"
#include "debug.h"

// Step request structure
struct stepper_req{
    Stepper* motor_list[MOTOR_LIST_SIZE_MAX];
    Stepper* motor_waiting;
    GPIO_Bulk* pin_bulk;
    unsigned int count;
    unsigned int req_steps;
};

//TODO: Substitute later for calibration value
#define HALF_PERIOD_LIMIT 100
#define MAX_PPS 4160

static const int low[MOTOR_LIST_SIZE_MAX] = {0, 0, 0, 0, 0, 0, 0, 0};
static const int high[MOTOR_LIST_SIZE_MAX] = {1, 1, 1, 1, 1, 1, 1, 1};

/**
 * @brief Assert if absolute direction parameter has a valid value.
 * 
 * @param[in] direction Value to test.
 * @return (int) 1 if valid, 0 otherwise.
 */
static inline int is_valid_direction_abs(direction_abs_t direction)
{
    return (direction == DIRECTION_CLOCKWISE || direction == DIRECTION_COUNTERCLOCKWISE);
}

/**
 * @brief Assert if relative direction parameter has a valid value.
 * 
 * @param[in] direction Value to test.
 * @return (int) 1 if valid, 0 otherwise.
 */
static inline int is_valid_direction_rel(direction_rel_t direction)
{
    return (direction == DIRECTION_POSITIVE || direction == DIRECTION_NEGATIVE);
}

/**
 * @brief Get the state of the busy flag for the motor
 * 
 * If the motor is part of a list of motors, it gets the state of the busy flag
 * from the first motor of the list.
 * 
 * @param[in] motor Pointer to the stepper object which to check
 * @return (int) 1 if the motor is busy, 0 otherwise 
 */
static inline int stepper_is_busy(Stepper* motor)
{
    //Stepper is considered busy if there is a request pending
    //If the shared mutex exists, then the request also exists

    return (motor->shared_mutex != NULL);
}

/**
 * @brief Allocate space for a new request and initialize it
 * 
 * When controlling a single motor, a bulk of lines with only one line is created
 * When controlling multiple motors, a bulk multiple lines is created
 * Said bulk is then assigned as the target for the gpio write functions
 * For each motor in the motors[] array, their current request pointer is set to the newly created request
 * 
 * @param motors Array of the motors to which the request corresponds
 * @param count Amount of motors in the array
 * @param req_steps Amount of (micro)steps to take 
 * @return (Stepper_req*) Pointer to the new request if successful, NULL otherwise
 */
static Stepper_req* stepper_create_new_request(Stepper* motors[], unsigned int count, unsigned int req_steps)
{
    // Validation not needed, because this is an internal function, and callers validate previously.
    // motors[] contents are validated on the initialization of motor_list.

    Stepper_req* request = NULL;
    GPIO_Bulk* bulk = NULL;
    pthread_mutex_t* req_mutex = NULL; // Allocated later to allow bulk and request to be freed separately from the mutex

    // Allocate enough memory for a Stepper_req and a GPIO_Bulk.
    // Done in the same call for efficiency.
    void* block = malloc(sizeof(Stepper_req)+sizeof(GPIO_Bulk));
    if(block == NULL){
        ERROR_PRINT("Failure allocating memory.");
        goto exit;
    }

    memset(block, 0, sizeof(Stepper_req)+sizeof(GPIO_Bulk));

    request = block;  // request is the base of the block
    bulk = block + sizeof(Stepper_req);

    // Initialize motor_list
    for(unsigned int i = 0; i < count; i++){
        if(motors[i] == NULL){
            ERROR_PRINT("Motor reference at index %d is invalid.", i);
            goto failure;
        }
        request->motor_list[i] = motors[i];
    }
    request->count = count;
    // request->motor_waiting = NULL;  // Redundant because of the memset

    // Create and initialize bulk of lines
    // To control multiple motors simultaneously, all lines must be requested together
    gpiod_line_bulk_init(bulk);
    for(unsigned int i = 0; i < count; i++)
        gpiod_line_bulk_add(bulk, motors[i]->step_pin);
    
    if(gpiod_line_request_bulk_output(bulk, "PEF", low) < 0){
        ERROR_PRINT("Error requesting line bulk\n");
        goto failure;
    } 

    // Set bulk as target for the request
    request->pin_bulk = bulk;
    request->req_steps = req_steps;

    // Create mutex to protect the request
    req_mutex = malloc(sizeof(pthread_mutex_t));
    if(req_mutex == NULL){
        ERROR_PRINT("Failure allocating memory.");
        goto failure;
    }

    if(pthread_mutex_init(req_mutex, NULL) != 0){
        ERROR_PRINT("Failure initializing mutex.");
        goto failure;
    }

    // Point all motors in the list to this request
    // struct_mutex doesn't need to be locked because a new request cannot be created while there is a pending req
    for(unsigned int i = 0; i < count; i++){
        motors[i]->current_req = request;
        motors[i]->shared_mutex = req_mutex;
    }

    goto exit;

failure:
    gpiod_line_release_bulk(bulk);
    free(block);
    free(req_mutex);
    request = NULL;
exit:
    return request;
}

/**
 * @brief Delete a stepper request.
 * 
 * Shared mutex must have been locked previously!!!
 * For each motor in the motor_list of the request, the current request pointer and mutex pointer are reset to NULL
 * Mutex must still be freed after this call
 * 
 * @param[in] request Pointer to the request to free.
 */
static void stepper_destroy_request(Stepper_req* request)
{
    // Release the pin bulk
    gpiod_line_release_bulk(request->pin_bulk);

    // Remove reference to this request from all motors in the motor_list
    for(unsigned int i = 0; i < request->count; i++){
        request->motor_list[i]->current_req = NULL;
        request->motor_list[i]->shared_mutex = NULL;
    }

    // Free the request
    free(request);  // request is the base of the malloc'ed block
}

#ifndef NDEBUG
/**
 * @brief (DEBUG FUNCTION) Print the contents of a stepper object
 * 
 * @param[in] motor Pointer to the stepper object to print
 */
static void stepper_print(Stepper* motor)
{
    DEBUG_PRINT("Dir Pin: %p", motor->dir_pin);
    DEBUG_PRINT("Step Pin: %p", motor->step_pin);
    DEBUG_PRINT("Positive dir: %d", motor->pos_direction);
    DEBUG_PRINT("Current dir: %d", motor->curr_direction);
    DEBUG_PRINT("Half period: %d", motor->half_period);
    DEBUG_PRINT("MS/rot: %d", motor->microsteps_per_rotation);
    DEBUG_PRINT("Steps: %d", motor->steps);
    DEBUG_PRINT("Stop: %d", motor->stop);
    DEBUG_PRINT("Name: %s\n", motor->name);
}
#endif

/**
 * @brief Motor controlling function
 * 
 * For each initialized motor, a thread is created with this function as its entry point.
 * If multiple motors are to be controlled simulatenously, only the thread for the first motor
 * in the motor_list of the respective request is awakened. 
 * 
 * @param[in] arg Pointer to the initialized Stepper object
 */
static void motor_pulser(void* arg)
{
    Stepper* motor = arg;

    while(1){
        // Wait until a step request is made
        pthread_mutex_lock(&motor->struct_mutex);
        while(!motor->req_available)
            pthread_cond_wait(&motor->req_cv, &motor->struct_mutex);
        motor->req_available = 0;
        pthread_mutex_unlock(&motor->struct_mutex);

        // Create timespec for sleeping bewteen transitions
        struct timespec pulse_duration;
        microsec_to_timespec(&pulse_duration, motor->half_period);

        unsigned int num_motors = motor->current_req->count;
        int stop = 0;

        do{
            // Pulse the pin
            GPIO_write_bulk(motor->current_req->pin_bulk, high);
            clock_nanosleep(CLOCK_MONOTONIC, 0, &pulse_duration, NULL);
            GPIO_write_bulk(motor->current_req->pin_bulk, low);
            clock_nanosleep(CLOCK_MONOTONIC, 0, &pulse_duration, NULL);

            // Update step counter for each motor in the request and check if they requested to stop
            for(unsigned int i = 0; i < num_motors; i++){
                Stepper* node = motor->current_req->motor_list[i];
                if(node->curr_direction == node->pos_direction)
                    node->steps++;
                else
                    node->steps--;
                
                stop |= node->stop;
            }
        } while(--motor->current_req->req_steps && !stop);

        // Free the request
        pthread_mutex_t* shr_mutex = motor->shared_mutex;
        Stepper* waiting_motor = NULL;

        pthread_mutex_lock(shr_mutex);
        waiting_motor = motor->current_req->motor_waiting;
        stepper_destroy_request(motor->current_req);
        pthread_mutex_unlock(shr_mutex);

        if(waiting_motor != NULL){
            // Clear
            waiting_motor->stop = 0;

            // Tell thread waiting for the motor to stop that it is finished
            pthread_cond_signal(&waiting_motor->wait_cv);
        }

        free(shr_mutex);
    }
}

/************************ PUBLIC API ************************/

/**
 * @brief Function to initialize a Stepper object.
 * 
 * @param[in] name String. Name of the motor.
 * @param[in] step_pin  Pin controlling the STEP input of the stepper driver. 
 *                      Value must be from the defined symbols in GPIO.h.
 * @param[in] dir_pin   Pin controlling the DIR input of the stepper driver.
 *                      Value must be from the defined symbols in GPIO.h 
 * @param[in] microstep Microstep configuration of the stepper driver.
 *                      Valid values are FULL, HALF, QUARTER, EIGHT, and SIXTEENTH.
 * @param[in] steps_per_rotation Amount of full steps in a single rotation of the stepper motor.
 * @param[in] init_dir Initial rotating direction for the motor. Considered the "positive" direcition.
 * @return (Stepper*) On success, pointer to a new and initialized Stepper object. Otherwise, NULL.
 */
Stepper* stepper_init(const char* name, int step_pin, int dir_pin, unsigned int microstep, unsigned int steps_per_rotation, direction_abs_t init_dir)
{   
    Stepper* motor = NULL;

    // Parameter validation
    // step_pin and dir_pin are validated with GPIO_init_pin
    if(name == NULL){
        ERROR_PRINT("Name string is invalid.");
        goto exit;
    } else if(!is_valid_microstep(microstep)){
        ERROR_PRINT("Microstep value is invalid.");
        goto exit;
    } else if(steps_per_rotation == 0){
        ERROR_PRINT("Steps/rot value is invalid.");
        goto exit;
    } else if(!is_valid_direction_abs(init_dir)){
        ERROR_PRINT("Direction is invalid.");
        goto exit;
    }

    // Create motor object
    motor = malloc(sizeof(Stepper));
    if(motor == NULL){
        ERROR_PRINT("Failure allocating memory.");
        goto exit;
    }

    memset(motor, 0, sizeof(Stepper));

    // Attempt to reclaim dir_pin
    motor->dir_pin = GPIO_init_pin(dir_pin, GPIO_DIRECTION_OUTPUT, 0);
    if(motor->dir_pin == NULL){
        ERROR_PRINT("Could not initialize direction pin (pin %d).", GPIO_GET_LINE(dir_pin));
        goto failure;
    }
    
    // Attempt to reclaim step_pin
    // GPIO_DIRECTION_NONE only reserves the line but doesnt request it from the driver. This allows for the line
    // to be requested in bulk in the future for simultaneous control of multiple motors.  
    motor->step_pin = GPIO_init_pin(step_pin, GPIO_DIRECTION_NONE, 0); 
    if(motor->step_pin == NULL){
        ERROR_PRINT("Could not initialize step pin (pin %d).", GPIO_GET_LINE(step_pin));
        goto failure;
    }

    // Initilialize the state of the motor (commented lines are redundant because of the memset)
    // motor->current_req = NULL;
    // motor->shared_mutex = NULL;
    pthread_mutex_init(&motor->struct_mutex, NULL);
    pthread_cond_init(&motor->req_cv, NULL);
    pthread_cond_init(&motor->wait_cv, NULL);
    strncpy(motor->name, name, MOTOR_NAME_LEN-1);
    motor->pos_direction = init_dir;
    stepper_set_direction_abs(motor, init_dir); 
    // motor->half_period = 0;
    motor->microsteps_per_rotation = microstep * steps_per_rotation;
    // motor->steps = 0;
    // motor->stop = 0;
    // motor->req_available = 0;

    // Create handling thread for the motor
    if(CreateTask(name, 1024, motor_pulser, motor) == 0){
        ERROR_PRINT("Could not create handler thread for the motor.");
        goto failure;
    }

    goto exit;

failure:
    free(motor);
    motor = NULL;
exit:
    return motor;
}

/**
 * @brief Free the Stepper object.
 * 
 * Calling this function while the Stepper is fulfilling a request will cancel and stop it.
 * Lines assigned to the motor can be freely used by other processes after this call.
 * Pointer to the Stepper object will be invalid after this call.
 * 
 * @param[in] motor Pointer to the object which will be freed.
 */
void stepper_destroy(Stepper* motor)
{
    // Parameter validation
    if(motor == NULL){
        ERROR_PRINT("Motor reference is invalid.");
        return;
    }

    // Stop handler thread
    stepper_stop(motor); // Stopping the current request will also call stepper_destroy_request
    Task_kill(Task_get_id_by_name(motor->name));

    // Free the mutexes, semaphores and condition variables
    pthread_cond_destroy(&motor->wait_cv);
    pthread_cond_destroy(&motor->req_cv);
    pthread_mutex_destroy(&motor->struct_mutex);

    // Release all reserved lines
    gpiod_line_release(motor->step_pin);
    gpiod_line_release(motor->dir_pin);

    free(motor);
}

/**
 * @brief Set the absolute turning direction of the motor.
 * 
 * @param[in] motor Pointer to the motor to update.
 * @param[in] direction New direction (DIRECTION_CLOCKWISE, DIRECTION_COUNTERCLOCKWISE).
 * @return (int) 0 on success, negative value otherwise.
 */
int stepper_set_direction_abs(Stepper* motor, direction_abs_t direction)
{
    int retval = -1;

    // Parameter validation
    if(motor == NULL){
        ERROR_PRINT("Motor reference invalid.");
        goto exit;
    } else if(!is_valid_direction_abs(direction)){
        ERROR_PRINT("Motor absolute direction invalid.");
        goto exit;   
    }

    // Check if direction can be changed
    if(stepper_is_busy(motor)){
        ERROR_PRINT("Motor is busy, try again later.");
        goto exit;
    }

    // Try to change direction
    if(gpiod_line_set_value(motor->dir_pin, direction) < 0){
        ERROR_PRINT("Error setting absolute direction for motor.");
        goto exit;
    }

    //On success, update the stepper struct
    motor->curr_direction = direction;
    retval = 0;

exit:
    return retval;
}

/**
 * @brief Get the current absolute turning direction of the motor.
 * 
 * @param[in] motor Pointer to the motor of interest.
 * @return (direction_abs_t) Appropiate direction value on success, DIRECTION_INVALID on error
 */
direction_abs_t stepper_get_direction_abs(Stepper* motor)
{
    // Parameter validation
    if(motor == NULL){
        ERROR_PRINT("Invalid motor reference");
        return DIRECTION_INVALID;
    }

    return motor->curr_direction;
}

/**
 * @brief Set the relative turning direction of the motor.
 * 
 * @param[in] motor Pointer to the motor to update.
 * @param[in] direction New direction (DIRECTION_POSITIVE, DIRECTION_NEGATIVE).
 * @return (int) 0 on success, negative value otherwise.
 */
int stepper_set_direction_rel(Stepper* motor, direction_rel_t direction)
{
    int retval = -1;

    // Parameter validation
    if(motor == NULL){
        ERROR_PRINT("Motor reference invalid.");
        goto exit;
    } else if(!is_valid_direction_rel(direction)){
        ERROR_PRINT("Motor relative direction invalid.");
        goto exit;   
    }

    retval = stepper_set_direction_abs(motor, (direction == DIRECTION_POSITIVE) ? motor->pos_direction : !motor->pos_direction);
    if(retval < 0)
        ERROR_PRINT("Error setting relative direction for the motor.");

exit:
    return retval;
}

/**
 * @brief Get the current relative turning direction of the motor.
 * 
 * @param[in] motor Pointer to the motor of interest.
 * @return (direction_abs_t) Appropiate direction value on success, DIRECTION_INVALID on error
 */
direction_rel_t stepper_get_direction_rel(Stepper* motor)
{
    // Parameter validation
    if(motor == NULL){
        ERROR_PRINT("Invalid motor reference");
        return DIRECTION_INVALID;
    }

    return (motor->curr_direction == motor->pos_direction) ? DIRECTION_POSITIVE : DIRECTION_NEGATIVE;
}

/**
 * @brief Set the speed of the motor, in microsteps per second.
 * 
 * The mangitude of the microstep depends on the microstep parameter value when the Stepper object
 * was initialized.
 * 
 * @param[in] motor Pointer to the motor to update.
 * @param[in] pps New speed, in microsteps per second.
 * @return (int) 0 on success, negative value otherwise and errno set to specific error value.
 */
int stepper_set_speed(Stepper* motor, unsigned int pps)
{
    //Special case of calling set_speed_multiple with only one motor in the list
    return stepper_set_speed_multiple(&motor, pps, 1);
}

/**
 * @brief Set the speed of multiple motors, in microsteps per second.
 * 
 * The mangitude of the microstep depends on the microstep parameter value when the Stepper object
 * was initialized.
 * 
 * @param[in] motors Array of pointers to motors, which are the motors to be updated.
 * @param[in] pps New speed for the motors, in microsteps per second.
 * @param[in] count Amount of motors in the motors array.
 * @return (int) 0 on success, negative value otherwise.
 */
int stepper_set_speed_multiple(Stepper* motors[], unsigned int pps, unsigned int count)
{
    int retval = -1;

    // Parameter validation
    if(motors == NULL){
        ERROR_PRINT("Motor list reference invalid.");
        goto exit;
    } else if(pps == 0){
        ERROR_PRINT("Invalid pps value.");
        goto exit;
    } else if(count == 0 || count > MOTOR_LIST_SIZE_MAX){
        ERROR_PRINT("Invalid amount of motors.");
        goto exit;
    }

    // Limit the speed if it exceeds maximum supported value
    if(pps > MAX_PPS){
        ERROR_PRINT("Specified speed exceeds supported maximum, limited to %d.", MAX_PPS);
        pps = MAX_PPS;
    }

    for(unsigned int i = 0; i < count; i++){
        // For each motor in the list, check if the speed can be changed, and if
        // their pointer points to a valid address
        if(stepper_is_busy(motors[i])){
            ERROR_PRINT("A motor in the list is busy, try again later.");
            goto exit;
        } else if(motors[i] == NULL){
            ERROR_PRINT("Motor reference at index %d is invalid.", i);
            goto exit;
        }

        // TODO: take into account calibration value
        // Update the motor speed
        motors[i]->half_period = 500000/pps;
    }

    retval = 0;

exit:
    return retval;
}

/**
 * @brief Step a motor. 
 * 
 * Steps are taken in the direction previously specified in the initialization of the 
 * Stepper object or by the stepper_set_direction (either absolute or relative) function.
 * 
 * @param[in] motor Pointer to the Stepper object to be stepped.
 * @param[in] steps Amount of steps to take.
 * @return (int) 0 on success, negative value otherwise.
 */
int stepper_step(Stepper* motor, unsigned int steps)
{
    // Special case of calling step_multiple with only one motor in the list
    return stepper_step_multiple(&motor, steps, 1);
}

/**
 * @brief Step multiple motors.
 * 
 * Steps are taken in the direction previously specified in the initialization of each 
 * Stepper object or by the stepper_set_direction (either absolute or relative) function.
 * 
 * @param[in] motors Array of pointers to Stepper objects, which are the motors to be stepped.
 * @param[in] steps Amount of steps to take.
 * @param[in] count Amount of motors in the motors array.
 * @return (int) 0 on success, negative value otherwise.
 */
int stepper_step_multiple(Stepper* motors[], unsigned int steps, int count)
{
    int retval = -1;

    // Parameter validation
    if(motors == NULL){
        ERROR_PRINT("Motor list reference invalid.");
        goto exit;
    } else if(steps == 0){
        ERROR_PRINT("Invalid value for steps.");
        goto exit;        
    } else if(count == 0 || count > MOTOR_LIST_SIZE_MAX){
        ERROR_PRINT("Invalid amount of motors.");
        goto exit;
    }

    // Check if a new request can be assigned
    if(stepper_is_busy(motors[0])){
        ERROR_PRINT("Motor is still completing last request, try again later.");
        goto exit;
    }

    // Create the new request
    Stepper_req* request = stepper_create_new_request(motors, count, steps);
    if(request == NULL){
        ERROR_PRINT("Error creating the new request.");
        goto exit;
    }

    // Signal the first motor in the motors array
    motors[0]->req_available = 1;
    pthread_cond_signal(&motors[0]->req_cv);

    retval = 0;

exit:
    return retval;
}

/**
 * @brief Get the absolute amount of steps taken by the motor.
 * 
 * @param[in] motor Pointer to the motor of interest.
 * @return (int) Absolute amount of steps taken; INT_MIN on error.
 */
int stepper_get_steps(Stepper* motor)
{   
    // Parameter validation
    if(motor == NULL){
        ERROR_PRINT("Motor reference invalid.");
        return INT_MIN;
    }
    
    // Mutex is not needed
    // Worst case is that we get one step less than the value of the actual counter. 
    // Not critical, and saves the performance loss from mutexes
    return motor->steps;
}

/**
 * @brief Stop a motor.
 * 
 * Function blocks until the motor has stopped. 
 * 
 * @param[in] motor Pointer to the motor to be stopped.
 */
void stepper_stop(Stepper* motor)
{
    // Parameter validation
    if(motor == NULL){
        ERROR_PRINT("Motor reference is invalid");
        return;
    }

    // Only stop the motor if it is busy
    motor->stop = stepper_is_busy(motor);
    
    // Block only if the stop "signal" was sent
    if(motor->stop)
        stepper_wait(motor);
}

/**
 * @brief Wait until the motor is finished stepping.
 * 
 * @param[in] motor Pointer to the Stepper object to wait.
 */
void stepper_wait(Stepper* motor)
{
    // Parameter validation
    if(motor == NULL){
        DEBUG_PRINT("Motor reference is invalid");
        return;
    }

    int flag = 0; 

    // Attempt to tell handler that we are waiting
    if(motor->shared_mutex != NULL){
        DEBUG_PRINT("shr_mutex locking: %p", motor->shared_mutex);
        pthread_mutex_lock(motor->shared_mutex);
        DEBUG_PRINT("shr_mutex lock ok");

        motor->current_req->motor_waiting = motor;
        flag = 1;
        
        DEBUG_PRINT("shr_mutex unlocking: %p", motor->shared_mutex);
        pthread_mutex_unlock(motor->shared_mutex);
        DEBUG_PRINT("shr_mutex unlock ok");
    }

    // If handler knows we are waiting, wait for its finish signal.
    if(flag == 1){
        pthread_mutex_lock(&motor->struct_mutex);
        while(stepper_is_busy(motor))
            pthread_cond_wait(&motor->wait_cv, &motor->struct_mutex);
        pthread_mutex_unlock(&motor->struct_mutex);
    }
}

/**
 * @brief Check if motor is ready for new commands.
 * 
 * @param[in] motor Handle to the motor.
 * @return (int) If ready, 1. Otherwise, 0. 
 */
int stepper_ready(Stepper* motor)
{
    // Parameter validation
    if(motor == NULL){
        DEBUG_PRINT("Motor reference is invalid");
        return 0;
    }

    return !stepper_is_busy(motor);
}

/**
 * @brief Function to assert if microstep parameter has a valid value
 * 
 * @param[in] microstep value to test
 * @return (int) 1 if valid, 0 otherwise
 */
int is_valid_microstep(int microstep)
{
    int retval = 0;

    switch(microstep){
        case FULL:
        case HALF:
        case QUARTER:
        case EIGHT:
        case SIXTEENTH:
            retval = 1;
            break;

        default:
            retval = 0;
            break;
    }

    return retval;
}
