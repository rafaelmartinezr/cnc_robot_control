#include "Axis.h"
#include "Time.h"
#include "debug.h"

int main(int argc, char const *argv[])
{
    Stepper* motor_left = stepper_init("motor-left", J21_HEADER_PIN_23, J21_HEADER_PIN_24, HALF, 200, DIRECTION_COUNTERCLOCKWISE);
    Stepper* motor_right = stepper_init("motor-right", J21_HEADER_PIN_19, J21_HEADER_PIN_18, HALF, 200, DIRECTION_CLOCKWISE);

    Stepper* motors[] = {motor_left, motor_right};
    Axis* x_axis = axis_init(motors, 40, 2);

    stepper_set_direction(motor_left, DIRECTION_CLOCKWISE);
    stepper_set_direction(motor_right, DIRECTION_COUNTERCLOCKWISE);

    axis_set_speed(x_axis, 20.0f);
    axis_move(x_axis, 100.0f);

    Delay_ms(15000);

    axis_move(x_axis, 500.0f);
    while(1)
        Delay_ms(1000);

    return 0;
}

