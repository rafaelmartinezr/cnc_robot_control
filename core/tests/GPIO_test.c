#include "GPIO.h"
#include "Time.h"
#include "debug.h"
#include <string.h>

static void max_freq_test(void)
{
    DEBUG_PRINT("Initializing pin 37...");
    GPIO_Pin* test_pin = GPIO_init_pin(J21_HEADER_PIN_37, GPIO_DIRECTION_OUTPUT, 0);
    if(!test_pin){
        ERROR_PRINT("Error initializing pin 37");
        return;
    } 

    DEBUG_PRINT("Pin 37 initiliazed succesfully!");

    struct timespec tstart, tstop, tdel, taccum;

    memset(&taccum, 0, sizeof(struct timespec));

    const int rep = 500000;

    for(int i = 0; i < rep; i++){
        clock_gettime(CLOCK_MONOTONIC, &tstart);
        GPIO_write(test_pin, 1);
        GPIO_write(test_pin, 0);
        clock_gettime(CLOCK_MONOTONIC, &tstop);
        sub_time(&tstop, &tstart, &tdel);
        add_time(&taccum, &tdel, &taccum);
    }

    div_time(&taccum, rep, &tdel);
    printf("Avg. del: ");
    print_time(&tdel);
    printf("\n");
}

static void multiple_test(void)
{
    GPIO_Controller* main_chip = gpiod_chip_open(GPIO_MAIN_CONTROLLER_PATH);
    DEBUG_PRINT("Main chip: %p", main_chip);

    GPIO_Pin* stepA = gpiod_chip_get_line(main_chip, GPIO_GET_LINE(J21_HEADER_PIN_23));
    DEBUG_PRINT("Step A successful (%p)", stepA);
    GPIO_Pin* stepB = gpiod_chip_get_line(main_chip, GPIO_GET_LINE(J21_HEADER_PIN_19));
    DEBUG_PRINT("Step B successful (%p)", stepB);

    int retval = 0;
    int high[2] = {1,1};
    int low[2] = {0,0};

    struct gpiod_line_bulk bulk = GPIOD_LINE_BULK_INITIALIZER;
    DEBUG_PRINT("Bulk init successfull");
    gpiod_line_bulk_add(&bulk, stepA);
    DEBUG_PRINT("Step A added to bulk successfull");
    gpiod_line_bulk_add(&bulk, stepB);
    DEBUG_PRINT("Step B added to bulk successfull");
    DEBUG_PRINT("bulk.n_lines = %d", bulk.num_lines);
    for(unsigned int i = 0; i < bulk.num_lines; i++)
        DEBUG_PRINT("bulk.line[%d]=(%p)", i, bulk.lines[i]);
    retval = gpiod_line_request_bulk_output(&bulk, "test", high);
    DEBUG_PRINT("Bulk request successfull (%d)", retval);

    while(1){
        gpiod_line_set_value_bulk(&bulk, high);
        Delay_ms(10);
        gpiod_line_set_value_bulk(&bulk, low);
        Delay_ms(10);
    }
}

int main(int argc, char const *argv[])
{
    max_freq_test();
    return 0;
}
