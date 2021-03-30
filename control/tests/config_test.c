//#define NDEBUG

#include "Stepper.h"
#include "Axis.h"
#include "Time.h"
#include "config.h"
#include "ipc.h"
#include "debug.h"

int main(int argc, char const *argv[])
{
    if(read_motor_config() != 0){
        ERROR_PRINT("Error reading " BASE_PATH MOTOR_CONFIG_NAME);
        exit(-1);
    }

    DEBUG_PRINT("Configuration read successfully");

    Axis* x_axis = get_axis_by_name("x-axis");
    if(x_axis == NULL){
        ERROR_PRINT("Error getting axis handle.");
        exit(-1);
    }
    
    DEBUG_PRINT("Axis obtained successfully");
    
    double speed = 30.0f;
    double distance = -1200.f;

    DEBUG_PRINT("Moving axis %s at %.02f mm/s for %.02f mm.", "x-axis", speed, distance);

    axis_set_speed(x_axis, speed);
    axis_move(x_axis, distance);
    while(1){
        printf("Position: %.02f mm\r", axis_get_position(x_axis));
        fflush(stdout);
        Delay_ms(50);
    }

    return 0;
}
