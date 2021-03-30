/*
 * GPIO.c
 * 
 * Author: Rafael Martinez
 * Date: 07.03.2021
 */

#define NDEBUG

#include "GPIO.h"
#include "debug.h"

#define CONSUMER_NAME "PEF"

static struct gpiod_chip* main_chip = NULL;
static struct gpiod_chip* aon_chip = NULL;

/**
 * @brief Initialize the GPIO chip to which a pin belongs to.
 * 
 * @param[in] pin J21_HEADER pin const. (eg. J21_HEADER_PIN_7)
 * @return (int) On success, 0. Otherwise, -1.
 */
static int gpio_init_controller(unsigned int pin)
{
    int retval = -1;

    struct gpiod_chip** chip = NULL;
    const char* chip_path = NULL;
    const char* chip_name = NULL;

    // Determine which chip to initialize.
    switch(GPIO_GET_CONTROLLER(pin)){
        case GPIO_AON_CONTROLLER_FLAG:
            chip = &aon_chip;
            chip_path = GPIO_AON_CONTROLLER_PATH;
            chip_name = "AON";
            break;

        case GPIO_MAIN_CONTROLLER_FLAG:
            chip = &main_chip;
            chip_path = GPIO_MAIN_CONTROLLER_PATH;
            chip_name = "MAIN";
            break;

        default:
            ERROR_PRINT("Specified pin doesn't have a chip associated.");
            goto exit;
    }

    // Attempt to initialize chip, if it has not been initialized already.
    if(*chip == NULL){
        DEBUG_PRINT("Initializing %s gpio chip (%s)...", chip_name, chip_path);

        *chip = gpiod_chip_open(chip_path);
        DEBUG_PRINT("chip: %p", *chip);
        if(*chip == NULL){
            ERROR_PRINT("Error initializing %s chip (%s). Verify you are running as root/sudo.", chip_name, chip_path);
            goto exit;
        }
    }

    DEBUG_PRINT("%s chip initialized successfully.", chip_name);
    retval = 0; // Only comes here on success.

exit:
    return retval;
}

/**************** PUBLIC FUNCTIONS ****************/

/**
 * @brief Initialize a pin on the J21 GPIO header.
 * 
 * @param[in] pin Symbol pertaining to the pin to initialize (eg. J21_HEADER_PIN_7)
 * @param[in] direction GPIO direction (GPIO_DIRECTION_OUTPUT, GPIO_DIRECTION_INPUT, or GPIO_DIRECTION_NONE)
 * @param[in] init_val Only relevant if direction=GPIO_DIRECTION_OUTPUT. Initial signal value of the pin.
 * @return (GPIO_Pin*) On success, handle to the initialized pin. Otherwise, NULL. 
 */
GPIO_Pin* GPIO_init_pin(unsigned int pin, gpio_direction_t direction, int init_val)
{
    GPIO_Pin* line = NULL;

    // Parameter validation
    // pin value is validated by gpio_init_controller.
    // direction value is validated by the switch statement.
    // init_val is not validated, but values != 0 are interpreted as 1.
    
    // Initialize controller for the pin. If it is already initialized, function returns inmediately.
    if(gpio_init_controller(pin)){
        ERROR_PRINT("Error initializing corresponding controller chip for pin %d.", GPIO_GET_LINE(pin));
        goto exit;
    }

    DEBUG_PRINT("main=%p\naon=%p", main_chip, aon_chip);
    DEBUG_PRINT("%d", (GPIO_GET_CONTROLLER(pin) == GPIO_MAIN_CONTROLLER_FLAG));

    // Register the corresponding line for a pin in its controller.
    line = gpiod_chip_get_line((GPIO_GET_CONTROLLER(pin) == GPIO_MAIN_CONTROLLER_FLAG) ? main_chip : aon_chip, GPIO_GET_LINE(pin));
    if(line == NULL){
        ERROR_PRINT("Error reclaiming pin %d.", GPIO_GET_LINE(pin));
        goto exit;
    }

    // Request the bulk from driver
    int retval = 0;
    switch (direction){
        case GPIO_DIRECTION_INPUT:
            retval = gpiod_line_request_input(line, CONSUMER_NAME);
            break;
        case GPIO_DIRECTION_OUTPUT:
            retval = gpiod_line_request_output(line, CONSUMER_NAME, init_val);
            break;
        case GPIO_DIRECTION_NONE: // Pin might be used by the caller to create a bulk later.
            retval = 0;
            break;
        default:
            ERROR_PRINT("Invalid direction.");
            retval = -1;
            break;
    }

    if(retval == 0){
        DEBUG_PRINT("Pin %d initialized successfully.", GPIO_GET_LINE(pin));
        goto exit;
    } else
        ERROR_PRINT("Error requesting line.");

error:
    gpiod_line_release(line);
    line = NULL;
exit:
    return line;
}

/**
 * @brief Initialize a bulk of pins on the J21 GPIO header.
 * 
 * Pins initialized this way can only be controlled groupally.
 * For controlling pins individually, initialize them with GPIO_init_pin.
 * 
 * @param[in] pins Array of symbols pertaining to the pin to initialize (eg. J21_HEADER_PIN_7).
 * @param[in] direction GPIO direction (GPIO_DIRECTION_OUTPUT, GPIO_DIRECTION_INPUT, or GPIO_DIRECTION_NONE).
 *                       Applied to all pins in the bulk.
 * @param[in] init_vals Only relevant if direction=GPIO_DIRECTION_OUTPUT. Array of initial signal value for the respective pins.
 * @param[in] count Amount of pins to initialize.
 * @return (GPIO_Bulk*) On success, handle to the initialized pin bulk. Otherwise, NULL.
 */
GPIO_Bulk* GPIO_init_bulk(unsigned int pins[], gpio_direction_t direction, int init_vals[], unsigned int count)
{
    GPIO_Bulk* bulk = NULL;

    // Parameter validation
    // pins content is validated by gpio_init_controller
    // init_vals contents are not validated, but values != 0 are interpreted as 1.
    if(pins == NULL){
        ERROR_PRINT("Pin list reference invalid.");
        goto exit;
    } else if (init_vals == NULL && direction == GPIO_DIRECTION_OUTPUT){
        ERROR_PRINT("Initial values list reference invalid.");
        goto exit;
    } else if(count == 0 || count > GPIOD_LINE_BULK_MAX_LINES){
        ERROR_PRINT("Amount of pins is not supported.");
        goto exit;
    }

    // Create bulk object
    bulk = malloc(sizeof(GPIO_Bulk));
    if(bulk == NULL){
        ERROR_PRINT("Error allocating bulk object.");
        goto exit;
    }

    gpiod_line_bulk_init(bulk);

    // Add the pins to the bulk
    for(unsigned int i = 0; i < count; i++){
        unsigned int pin = pins[i];

        // Initialize controller for the pins. If it is already initialized, function returns inmediately.
        if(gpio_init_controller(pin)){
            ERROR_PRINT("Error initializing corresponding controller chip for pin %d.", GPIO_GET_LINE(pin));
            goto error;
        }

        // Register the corresponding line for a pin in its controller.
        GPIO_Pin* line = gpiod_chip_get_line((GPIO_GET_CONTROLLER(pin) == GPIO_MAIN_CONTROLLER_FLAG) ? main_chip : aon_chip, GPIO_GET_LINE(pin));
        if(line == NULL){
            ERROR_PRINT("Error reclaiming pin %d.", GPIO_GET_LINE(pin));
            goto error;
        }

        gpiod_line_bulk_add(bulk, line);
    }

    // Request the bulk from driver
    int retval = 0;
    switch (direction){
        case GPIO_DIRECTION_INPUT:
            retval = gpiod_line_request_bulk_input(bulk, CONSUMER_NAME);
            break;
        case GPIO_DIRECTION_OUTPUT:
            retval = gpiod_line_request_bulk_output(bulk, CONSUMER_NAME, init_vals);
            break;
        default:
            ERROR_PRINT("Invalid GPIO direction.");
            retval = -1;
            break;
    }

    if(retval == 0){
        DEBUG_PRINT("Bulk initialized successfully.");
        goto exit;
    } else 
        ERROR_PRINT("Error requesting bulk.");

error:
    gpiod_line_release_bulk(bulk);
    free(bulk);
    bulk = NULL;
exit:
    return bulk;
}

/**
 * @brief Get the J21 header pin constant corresponding to a pin number.
 * 
 * @param[in] p Pin number. 
 * @return (int) If p is valid, corresponding J21_HEADER const (eg. p=7 --> J21_HEADER_PIN_7). Otherwise, INVALID_PIN.
 */
int int_to_gpio_pin(int p)
{
    int rv = 0;
    switch(p){
        case 7:
            rv = J21_HEADER_PIN_7;
            break;
        case 8:
            rv = J21_HEADER_PIN_8;
            break;
        case 10:
            rv = J21_HEADER_PIN_10;
            break;
        case 11:
            rv = J21_HEADER_PIN_11;
            break;
        case 12:
            rv = J21_HEADER_PIN_12;
            break;
        case 13:
            rv = J21_HEADER_PIN_13;
            break;
        case 16:
            rv = J21_HEADER_PIN_16;
            break;
        case 18:
            rv = J21_HEADER_PIN_18;
            break;
        case 19:
            rv = J21_HEADER_PIN_19;
            break;
        case 21:
            rv = J21_HEADER_PIN_21;
            break;
        case 23:
            rv = J21_HEADER_PIN_23;
            break;
        case 24:
            rv = J21_HEADER_PIN_24;
            break;
        case 29:
            rv = J21_HEADER_PIN_29;
            break;
        case 31:
            rv = J21_HEADER_PIN_31;
            break;
        case 32:
            rv = J21_HEADER_PIN_32;
            break;
        case 33:
            rv = J21_HEADER_PIN_33;
            break;
        case 35:
            rv = J21_HEADER_PIN_35;
            break;
        case 36:
            rv = J21_HEADER_PIN_36;
            break;
        case 37:
            rv = J21_HEADER_PIN_37;
            break;
        case 38:
            rv = J21_HEADER_PIN_38;
            break;
        case 40:
            rv = J21_HEADER_PIN_40;
            break;
        default:
            rv = INVALID_PIN;
            break;
    }

    return rv;
}
