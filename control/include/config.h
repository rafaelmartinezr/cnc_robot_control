#ifndef CONFIG_H
#define CONFIG_H

#include "sysconfig.h"

#include "Stepper.h"
#include "Axis.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

// Name of the configuration file.
#define MOTOR_CONFIG_NAME "motor.conf"

/**
 * The config file can contain the following paramaters/identifiers:
 * 
 * [motor] = identifier for initializing a motor.
 * Following parameters apply only to motors:
 *     name = Name used to identify the motor.
 *     step_pin = Pin number on the J21 header that controls the STEP input of the stepper driver.
 *     dir_pin = Pin number on the J21 header that controls the DIRECTION input of the stepper driver.
 *     steps_per_rotation = positive integer, indicating the amount of full steps in a single rotation of a motor.
 *     direction = String, either "clockwise" or "counterclockwise". Initial rotational direction of the motor.
 *     microstep = Number, either 1, 2, 4, 8, 16 or 32. Indicates the microstep resolution of the driver.
 * 
 * [axis] = identifier for initializing an axis.
 * Following parameters apply only to axes:
 *      name = Name use to identify the axis.
 *      motors = Comma-separated list with the names of previously defined motors
 *      mm_per_rotation = Positive integer, indicating how many millimeters does the axis advance on a single rotation
 * 
 * Lines starting with a # (pound sign) will be considered comments (ignored)
 */    

/**
 * @brief Read the motor configuration file and initialized all configured objects.
 * 
 * See config.h for a description on the configuration file.
 * 
 * @return (int) On success, 0. Otherwise, -1.  
 */
int read_motor_config(void);

/**
 * @brief Get the handle to a motor defined in the motors configuration file.
 * 
 * Should only be called after a successful call to read_motor_config.  
 * 
 * @param name (in) Name of the motor, as in the configuration file.
 * @return (Stepper*) On success, handle to the motor. Otherwise, NULL. 
 */
Stepper* get_motor_by_name(const char* name);

/**
 * @brief Get the handle to an axis defined in the motors configuration file.
 * 
 * Should only be called after a successful call to read_motor_config.  
 * 
 * @param name (in) Name of the axis, as in the configuration file.
 * @return (Stepper*) On success, handle to the axis. Otherwise, NULL. 
 */
Axis* get_axis_by_name(const char* name);

#endif
