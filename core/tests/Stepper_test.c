#include "Stepper.h"
#include "GPIO.h"
#include "Time.h"
#include "debug.h"

int main(int argc, char const *argv[])
{
    Stepper* motor_A = stepper_init("motor-A", J21_HEADER_PIN_23, J21_HEADER_PIN_24, HALF, 200, DIRECTION_CLOCKWISE);
    stepper_set_speed(motor_A, 200);

    stepper_step(motor_A, 400);
    DEBUG_PRINT("Waiting...");
    stepper_wait(motor_A);
    DEBUG_PRINT("Finished...");

    stepper_set_direction(motor_A, DIRECTION_COUNTERCLOCKWISE);
    stepper_step(motor_A, 2000);
    DEBUG_PRINT("Stepping...");
    Delay_ms(4000);
    stepper_stop(motor_A);
    DEBUG_PRINT("Stopped");

    Stepper* motor_B = stepper_init("motor-B", J21_HEADER_PIN_19, J21_HEADER_PIN_18, HALF, 200, DIRECTION_COUNTERCLOCKWISE);
    stepper_set_speed(motor_B, 200);

    stepper_step(motor_B, 400);
    DEBUG_PRINT("Waiting...");
    stepper_wait(motor_B);
    DEBUG_PRINT("Finished...");

    stepper_set_direction(motor_B, DIRECTION_CLOCKWISE);
    stepper_step(motor_B, 2000);
    DEBUG_PRINT("Stepping...");
    Delay_ms(4000);
    stepper_stop(motor_B);
    DEBUG_PRINT("Stopped");

    Stepper* axis[] = {motor_A, motor_B};

    DEBUG_PRINT("Stepping multiple");
    stepper_step_multiple(axis, 10000, 2); 
    Delay_ms(4000);
    DEBUG_PRINT("Stop");
    stepper_stop(motor_A);

    stepper_set_direction(motor_A, DIRECTION_CLOCKWISE);
    stepper_set_direction(motor_B, DIRECTION_COUNTERCLOCKWISE);

    DEBUG_PRINT("Stepping multiple");
    stepper_step_multiple(axis, 10000, 2); 
    Delay_ms(4000);
    DEBUG_PRINT("Stop");
    stepper_stop(motor_B);

    while(1)
        Delay_ms(5000);

    return 0;
}
