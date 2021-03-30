/*
 * Axis.c
 * 
 * Author: Rafael Martinez
 * Date: 07.03.2021
 */ 

#include "Axis.h"
#include "debug.h"

/**
 * @internal
 * @brief Convert millimeters to microsteps
 * 
 * @param[in] axis Axis handle 
 * @param[in] mm Millimeters
 * @return (int) Amount of steps
 */
inline static unsigned int mm_to_steps(Axis* axis, double mm)
{
    return (unsigned int)(mm * (double)axis->motors[0]->microsteps_per_rotation / axis->mm_per_rotation);
}

/**
 * @internal
 * @brief Convert from steps to millimeters
 * 
 * @param[in] axis Axis handle
 * @param[in] steps Steps
 * @return (double) Millimeters 
 */
inline static double steps_to_mm(Axis* axis, int steps)
{
    return (double)steps * axis->mm_per_rotation / (double)axis->motors[0]->microsteps_per_rotation;
}

/************* PUBLIC API *************/

/**
 * @brief Create and initialize an Axis object.
 * 
 * @param[in] motors List of stepper motor handles linked to the new axis.
 * @param[in] mm_per_rotation Amount of millimeters advanced in a rotation of the motors.
 * @param[in] count Amount of motors in the motors array.
 * @return (Axis*) On success, returns an Axis handle. Otherwise, NULL. 
 */
Axis* axis_init(Stepper* motors[], unsigned int mm_per_rotation, unsigned int count)
{
    Axis* axis = NULL;

    // Parameter validation
    if(motors == NULL){
        ERROR_PRINT("Motor list reference is invalid.");
        goto exit;
    } else if(mm_per_rotation == 0){
        ERROR_PRINT("Millimeters per rotation has an invalid value.");
        goto exit;
    } else if(count == 0 || count > MOTOR_LIST_SIZE_MAX){
        ERROR_PRINT("Motor count is not supported.");
        goto exit;
    }

    // Create axis object
    axis = malloc(sizeof(Axis));
    if(axis == NULL){
        ERROR_PRINT("Error allocating memory for axis object.");
        goto exit;
    }

    memset(axis, 0, sizeof(Axis));

    // Validate input motor list, and assign it to the axis
    for(unsigned int i = 0; i < count; i++){
        if(motors[i] == NULL){
            ERROR_PRINT("Motor reference at index %d is invalid.", i);
            goto failure;
        }

        axis->motors[i] = motors[i];
    }
    
    // Axis member initialization (commented lines are redundant because of the memset)
    axis->num_motors = count;
    // axis->reset_dir = 0;
    axis->mm_per_rotation = (double)mm_per_rotation;
    // axis->position = 0.0f;
    // axis->speed = 0.0f;

    goto exit;

failure:
    free(axis);
    axis = NULL;
exit:
    return axis;
}

/**
 * @brief Set the speed of an axis.
 * 
 * @param[in] axis Handle of the axis to update.
 * @param[in] mm_per_sec New speed in mm/sec (must be positive).
 * @return (int) On success, 0. Otherwise, -1.
 */
int axis_set_speed(Axis* axis, double mm_per_sec)
{
    int retval = -1;

    //Parameter validation
    if(axis == NULL){
        ERROR_PRINT("Axis reference is invalid.");
        return retval;
    } else if(mm_per_sec <= 0){
        ERROR_PRINT("Invalid speed.");
        return retval;
    }

    unsigned int steps_per_sec = mm_to_steps(axis, mm_per_sec);
    DEBUG_PRINT("Vel: %d pps", steps_per_sec);

    if(stepper_set_speed_multiple(axis->motors, steps_per_sec, axis->num_motors) < 0)
        ERROR_PRINT("Could not set new speed for the axis.");
    else{
        axis->speed = mm_per_sec;
        retval = 0; // Only comes here on success
    }

    return retval;
}

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
int axis_set_direction(Axis* axis, direction_rel_t direction)
{
    int retval = -1;

    // Parameter validation
    if(axis == NULL){
        ERROR_PRINT("Axis reference is invalid.");
        goto exit;
    } // direction is validated by stepper_set_direction_rel 

    for(unsigned int i = 0; i < axis->num_motors; i++){
        if(stepper_set_direction_rel(axis->motors[i], direction) < 0){
            ERROR_PRINT("Could not set new direction for a motor of the axis.");
            goto exit;
        }
    }

    retval = 0; // Only comes here on success

exit:
    return retval;
}

/**
 * @brief Move an axis a set distance.
 * 
 * Negative distance implies to move in the opposite direction.
 * 
 * @param[in] axis Handle of the axis to update.
 * @param[in] distance Distance to advance in mm (can be positive or negative).
 * @return (int) On success, 0. Otherwise, -1.
 */
int axis_move(Axis* axis, double distance)
{
    int retval = -1;

    // Parameter validation
    if(axis == NULL){
        ERROR_PRINT("Axis reference is invalid.");
        goto exit;
    } else if(distance == 0.0f){
        DEBUG_PRINT("No distance to run. Returning.");
        retval = 0; // Not an error
        goto exit;
    }

    // If a previous command caused a temporary direction change, reset it
    // Since a call to wait or stop is not assured, its better to have this here
    if(axis->reset_dir){
        axis->reset_dir = 0;
        // BUG: What if previous direction was negative?
        if(axis_set_direction(axis, DIRECTION_POSITIVE) < 0){
            ERROR_PRINT("Could not reset axis direction to original state.");
            goto exit;
        }
    }

    // Change temporarily the direction to the opposite relative direction
    // if distance is negative
    // TODO: Check if bitwise checking and changing sign is more efficient
    if(distance < 0) {
        distance = -distance;
        axis->reset_dir = 1;
        if(axis_set_direction(axis, DIRECTION_NEGATIVE) < 0){
            ERROR_PRINT("Could not set axis to the opposite direction.");
            goto exit;
        }
    }

    unsigned int steps = mm_to_steps(axis, distance);
    DEBUG_PRINT("Distance: %d mm (%d steps)", (int)distance, steps);

    
    if(stepper_step_multiple(axis->motors, steps, axis->num_motors) < 0)
        ERROR_PRINT("Error attempting to move the axis.");
    else
        retval = 0; // Only comes on success

exit:
    return retval;
}

/**
 * @brief Wait until an axis stops moving.
 * 
 * @param[in] axis Handle of the axis to update.
 */
void axis_wait(Axis* axis)
{
    //Parameter validation
    if(axis == NULL){
        ERROR_PRINT("Axis reference is invalid.");
        return;
    }

    stepper_wait(axis->motors[0]);
}

/**
 * @brief Check if axis is ready for new commands.
 * 
 * @param[in] axis Handle to the axis.
 * @return (int) If ready, 1. Otherwise, 0. 
 */
int axis_ready(Axis* axis)
{
    //Parameter validation
    if(axis == NULL){
        ERROR_PRINT("Axis reference is invalid.");
        return 0;
    }

    return stepper_ready(axis->motors[0]);
}

/**
 * @brief Stop an axis from moving.
 * 
 * @param[in] axis Handle of the axis to update.
 */
void axis_stop(Axis* axis)
{
    //Parameter validation
    if(axis == NULL){
        ERROR_PRINT("Axis reference is invalid.");
        return;
    }

    stepper_stop(axis->motors[0]);
}

/**
 * @brief Get the current position of an axis
 * 
 * @param[in] axis Handle of the axis to update.
 * @return (double) On success, current position of the axis in mm. Otherwise, NAN.
 */
double axis_get_position(Axis* axis)
{
    //Parameter validation
    if(axis == NULL){
        ERROR_PRINT("Axis reference is invalid.");
        return NAN;
    }

    unsigned int given_steps = stepper_get_steps(axis->motors[0]);
    axis->position = steps_to_mm(axis, given_steps);
    return axis->position;
}
