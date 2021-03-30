/**
 * @file Axis.h
 * @author Rafael Martinez (rafael.martinez@udem.edu)
 * @brief Axis library public interface.
 * @details An axis is a group of stepper motors that are controlled simultaneously. They are also associated with
 * controlling the displacement along a dimension of the CNC robot. For independent control of stepper motors,
 * or adimensional control, see the Stepper library.
 * @see Stepper.h
 * @version 1.0
 * @date 07.03.2021
 * 
 * @copyright Copyright (c) 2021
 */

#ifndef AXIS_H
#define AXIS_H

#include "Stepper.h"
#include <string.h>
#include <math.h>

#define AXIS_LIST_SIZE_MAX 4 /**< Maximum amount of motors that can be linked to an axis*/
#define AXIS_NAME_LEN 32 /**< Maximum lenght for the name of an axis*/

/**
 * @brief Axis object.
 */
typedef struct axis{
    Stepper* motors[MOTOR_LIST_SIZE_MAX]; /**< Motors linked to the axis.*/
    unsigned int num_motors; /**< Amount of motors in the motors[] array.*/
    int reset_dir; /**< @internal Flag for resetting direction of the axis.*/
    double mm_per_rotation; /**< Millimeters advanced in a single rotation of a motor.*/
    double position; /**< Current position of the axis, in mm, relative to the home position.*/
    double speed; /**< Current speed of the motor, in mm/sec.*/
} Axis;

/**
 * @brief Create and initialize an Axis object.
 * 
 * @param[in] motors List of stepper motor handles linked to the new axis.
 * @param[in] mm_per_rotation Amount of millimeters advanced in a rotation of the motors.
 * @param[in] count Amount of motors in the motors array.
 * @return (Axis*) On success, returns an Axis handle. Otherwise, NULL. 
 */
Axis* axis_init(Stepper* motors[], unsigned int mm_per_rotation, unsigned int count);

/**
 * @brief Set the speed of an axis.
 * 
 * @param[in] axis Handle of the axis to update.
 * @param[in] mm_per_sec New speed in mm/sec (must be positive).
 * @return (int) On success, 0. Otherwise, -1.
 */
int axis_set_speed(Axis* axis, double mm_per_sec);

/**
 * @brief Set the direction of an axis. 
 * 
 * DIRECTION_POSITIVE means the axis will move in the positive direction.
 * DIRECTION_NEGATIVE means the axis will move in the negative direction.
 * 
 * @param[in] axis Handle of the axis to update.
 * @param[in] direction New direction, either DIRECTION_POSITIVE or DIRECTION_NEGATIVE.
 * @return (int) On success, 0. Otherwise, -1.
 */
int axis_set_direction(Axis* axis, direction_rel_t direction);

/**
 * @brief Move an axis a set distance.
 * 
 * Negative distance implies to move in the opposite direction.
 * 
 * @param[in] axis Handle of the axis to update.
 * @param[in] distance Distance to advance in mm (can be positive or negative).
 * @return (int) On success, 0. Otherwise, -1.
 */
int axis_move(Axis* axis, double distance);

/**
 * @brief Wait until an axis stops moving.
 * 
 * @param[in] axis Handle of the axis to update.
 */
void axis_wait(Axis* axis);

/**
 * @brief Check if axis is ready for new commands.
 * 
 * @param[in] axis Handle to the axis.
 * @return (int) If ready, 1. Otherwise, 0. 
 */
int axis_ready(Axis* axis);

/**
 * @brief Stop an axis from moving.
 * 
 * @param[in] axis Handle of the axis to update.
 */
void axis_stop(Axis* axis);

/**
 * @brief Get the current position of an axis
 * 
 * @param[in] axis Handle of the axis to update.
 * @return (double) On success, current position of the axis in mm. Otherwise, NAN.
 */
double axis_get_position(Axis* axis);

#endif
