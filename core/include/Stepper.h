/**
 * @file Stepper.h
 * @author Rafael Martinez (rafael.martinez@udem.edu)
 * @brief Stepper library public interface.
 * @details Library for controling stepper motors. Motors might be controlled either individually, in bulk, or
 *          alternate between both modes.  For every motor, a handler thread is created at its initialization.
 *          Said thread sleeps until a call to stepper_move() is done. Beforehand, stepper motors might
 *          be configured to turn in a specific direction, or to rotate at a given speed. For motors controlled in 
 *          bulk, only the thread of one the motors in the bulk is used, ensuring optimal use of CPU time and true 
 *          parallel control. For further details, refer to the documentation of the available functions. Here, motor 
 *          movement is measured in microsteps. For measuring movement in along dimension in millimeters, see Axis.h.
 * @see Axis.h 
 * @version 1.0
 * @date 03.07.2021
 * 
 * @copyright Copyright (c) 2021
 */

#ifndef STEPPER_H
#define STEPPER_H

#include "GPIO.h"
#include "Tasks.h"
#include "Time.h"
#include <string.h>
#include <limits.h>

/**
 * @brief Maximum amount of motors that might be controlled simultaneously.
 */
#define MOTOR_LIST_SIZE_MAX 8
/**
 * @brief Maximum length for the name of a motor. 
 */
#define MOTOR_NAME_LEN 32

/**
 * @brief Invalid direction constant. 
 * @details As #define because same name cannot be included in two enums.
 */
#define DIRECTION_INVALID 2

/**
 * @brief Absolute rotational directions.
 */
typedef enum direction_abs{
    DIRECTION_COUNTERCLOCKWISE = 0, /**< Counterclockwise */
    DIRECTION_CLOCKWISE = 1         /**< Clockwise */
} direction_abs_t;

/**
 * @brief Relative rotational directions.
 */
typedef enum direction_rel{
    DIRECTION_NEGATIVE = -1, /**< Negative direction */
    DIRECTION_POSITIVE = 1   /**< Positive direction */
} direction_rel_t;

/**
 * @brief Microstep configuration of the stepper drivers.
 */
enum microstep_config{
    FULL = 1,
    HALF = 2,
    QUARTER = 4,
    EIGHT = 8, 
    SIXTEENTH = 16
};

/**
 * @internal
 * @brief Move request object.
 */
typedef struct stepper_req Stepper_req;

/**
 * @brief Stepper motor object.
 * @details Constructed by stepper_init(). Motors are assigned a starting direction
 * at initialization. This direction is then considered "positive". Steps taken in
 * this direction are added to the position accumulator, while steps taken in the
 * opposite direction are substracted from the accumulator.
 */
typedef struct stepper{
    GPIO_Pin* dir_pin;              /**< Pin handle setting the direction */
    GPIO_Pin* step_pin;             /**< Pin handle for stepping the motor */
    Stepper_req* current_req;       /**< @internal Handle to the current move request */
    pthread_mutex_t* shared_mutex;  /**< @internal Protects access to shared_mutex (pointer and contents) */ 
    pthread_mutex_t struct_mutex;   /**< @internal Protects conditional variables */
    pthread_cond_t req_cv;          /**< @internal Cond. var. for signaling that a request is ready */
    pthread_cond_t wait_cv;         /**< @internal Cond. var. for waiting on a reques to finish */
    char name[MOTOR_NAME_LEN];      /**< Name of the stepper motor */
    direction_abs_t pos_direction;  /**< Positive direction of the motor */
    direction_abs_t curr_direction; /**< Current direction of the motor */
    unsigned int half_period;       /**< Pulse width for the pulse train driving the stepper */
    unsigned int microsteps_per_rotation; /**< Microstep configuration of the driver */
    volatile int steps;             /**< Steps accumulator */
    volatile unsigned int stop;     /**< @internal Flag for stopping the stepper */
    volatile unsigned int req_available;  /**< @internal Flag indicating that a move request is available */
} Stepper;

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
Stepper* stepper_init(const char* name, int step_pin, int dir_pin, unsigned int microstep, unsigned int steps_per_rotation, direction_abs_t init_dir);

/**
 * @brief Free the Stepper object.
 * 
 * Calling this function while the Stepper is fulfilling a request will cancel and stop it.
 * Lines assigned to the motor can be freely used by other processes after this call.
 * Pointer to the Stepper object will be invalid after this call.
 * 
 * @param[in] motor Pointer to the object which will be freed.
 */
void stepper_destroy(Stepper* motor);

/**
 * @brief Set the absolute turning direction of the motor.
 * 
 * @param[in] motor Pointer to the motor to update.
 * @param[in] direction New direction (DIRECTION_CLOCKWISE, DIRECTION_COUNTERCLOCKWISE).
 * @return (int) 0 on success, negative value otherwise.
 */
int stepper_set_direction_abs(Stepper* motor, direction_abs_t direction);

/**
 * @brief Get the current absolute turning direction of the motor.
 * 
 * @param[in] motor Pointer to the motor of interest.
 * @return (direction_abs_t) Appropiate direction value on success, DIRECTION_INVALID on error
 */
direction_abs_t stepper_get_direction_abs(Stepper* motor);

/**
 * @brief Set the relative turning direction of the motor.
 * 
 * @param[in] motor Pointer to the motor to update.
 * @param[in] direction New direction (DIRECTION_POSITIVE, DIRECTION_NEGATIVE).
 * @return (int) 0 on success, negative value otherwise.
 */
int stepper_set_direction_rel(Stepper* motor, direction_rel_t direction);

/**
 * @brief Get the current relative turning direction of the motor.
 * 
 * @param[in] motor Pointer to the motor of interest.
 * @return (direction_abs_t) Appropiate direction value on success, DIRECTION_INVALID on error
 */
direction_rel_t stepper_get_direction_rel(Stepper* motor);

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
int stepper_set_speed(Stepper* motor, unsigned int pps);

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
int stepper_set_speed_multiple(Stepper* motors[], unsigned int pps, unsigned int count);

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
int stepper_step(Stepper* motor, unsigned int steps);

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
int stepper_step_multiple(Stepper* motors[], unsigned int steps, int count);

/**
 * @brief Get the absolute amount of steps taken by the motor.
 * 
 * @param[in] motor Pointer to the motor of interest.
 * @return (int) Absolute amount of steps taken; INT_MIN on error.
 */
int stepper_get_steps(Stepper* motor);

/**
 * @brief Stop a motor.
 * 
 * Function blocks until the motor has stopped. 
 * 
 * @param[in] motor Pointer to the motor to be stopped.
 */
void stepper_stop(Stepper* motor);

/**
 * @brief Wait until the motor is finished stepping.
 * 
 * @param[in] motor Pointer to the Stepper object to wait.
 */
void stepper_wait(Stepper* motor);

/**
 * @brief Check if motor is ready for new commands.
 * 
 * @param[in] motor Handle to the motor.
 * @return (int) If ready, 1. Otherwise, 0. 
 */
int stepper_ready(Stepper* motor);

/**
 * @brief Function to assert if microstep parameter has a valid value
 * 
 * @param[in] microstep value to test
 * @return (int) 1 if valid, 0 otherwise
 */
int is_valid_microstep(int microstep);

#endif
