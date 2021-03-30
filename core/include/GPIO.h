/**
 * @file GPIO.h
 * @author Rafael Martinez (rafael.martinez@udem.edu)
 * @brief GPIO library public interface.
 * @details This library builds upon the gpiod library (https://git.kernel.org/pub/scm/libs/libgpiod/libgpiod.git).
 *          The public interface consists mainly of the enum to access the pins on the J21 header of the Jetson board,
 *          functions to initialize said pins either in bulk or individually,
 *          and some wrappers with friendly names around functions from the gpiod library.
 * @version 1.0
 * @date 03.07.2021
 * 
 * @copyright Copyright (c) 2021
 */

#ifndef GPIO_H
#define GPIO_H

#include <gpiod.h> // Library for communicating with the GPIO char device driver.
#include <stdio.h>

// Paths to the device drivers of the GPIO pins.
#define GPIO_MAIN_CONTROLLER_PATH "/dev/gpiochip0" /**< @internal*/
#define GPIO_AON_CONTROLLER_PATH "/dev/gpiochip1" /**< @internal*/

/**
 * @brief Flag for pins belonging to the main gpio chip
 */
#define GPIO_MAIN_CONTROLLER_FLAG (1U << 31) 
/**
 * @brief Flag for pins belonging to the aon gpio chip
 */
#define GPIO_AON_CONTROLLER_FLAG  (1U << 30)

/**
 * @brief Constants for accessing the J21 header pins.
 * @details Lowest byte contains the line number of the pin,
 *          highest byte contains a flag indicating to which gpio chip
 *          does the pin belong to. For details on the line numbering,
 *          see https://forums.developer.nvidia.com/t/how-to-configure-a-gpio-on-tx2/52208.
 */
enum j21_hdr_pins{
    J21_HEADER_PIN_7  = GPIO_MAIN_CONTROLLER_FLAG | 76,  /**< Pin 7  */
    J21_HEADER_PIN_8  = GPIO_MAIN_CONTROLLER_FLAG | 144, /**< Pin 8  */
    J21_HEADER_PIN_10 = GPIO_MAIN_CONTROLLER_FLAG | 145, /**< Pin 10 */
    J21_HEADER_PIN_11 = GPIO_MAIN_CONTROLLER_FLAG | 146, /**< Pin 11 */
    J21_HEADER_PIN_12 = GPIO_MAIN_CONTROLLER_FLAG | 72,  /**< Pin 12 */
    J21_HEADER_PIN_13 = GPIO_MAIN_CONTROLLER_FLAG | 77,  /**< Pin 13 */
    J21_HEADER_PIN_16 = GPIO_AON_CONTROLLER_FLAG  | 40,  /**< Pin 16 */
    J21_HEADER_PIN_18 = GPIO_MAIN_CONTROLLER_FLAG | 161, /**< Pin 18 */
    J21_HEADER_PIN_19 = GPIO_MAIN_CONTROLLER_FLAG | 109, /**< Pin 19 */
    J21_HEADER_PIN_21 = GPIO_MAIN_CONTROLLER_FLAG | 108, /**< Pin 21 */
    J21_HEADER_PIN_23 = GPIO_MAIN_CONTROLLER_FLAG | 107, /**< Pin 23 */
    J21_HEADER_PIN_24 = GPIO_MAIN_CONTROLLER_FLAG | 110, /**< Pin 24 */
    J21_HEADER_PIN_29 = GPIO_MAIN_CONTROLLER_FLAG | 78,  /**< Pin 29 */
    J21_HEADER_PIN_31 = GPIO_AON_CONTROLLER_FLAG  | 42,  /**< Pin 31 */
    J21_HEADER_PIN_32 = GPIO_AON_CONTROLLER_FLAG  | 41,  /**< Pin 32 */
    J21_HEADER_PIN_33 = GPIO_MAIN_CONTROLLER_FLAG | 69,  /**< Pin 33 */
    J21_HEADER_PIN_35 = GPIO_MAIN_CONTROLLER_FLAG | 75,  /**< Pin 35 */
    J21_HEADER_PIN_36 = GPIO_MAIN_CONTROLLER_FLAG | 147, /**< Pin 36 */
    J21_HEADER_PIN_37 = GPIO_MAIN_CONTROLLER_FLAG | 68,  /**< Pin 37 */
    J21_HEADER_PIN_38 = GPIO_MAIN_CONTROLLER_FLAG | 74,  /**< Pin 38 */
    J21_HEADER_PIN_40 = GPIO_MAIN_CONTROLLER_FLAG | 73,  /**< Pin 40 */
    INVALID_PIN = -1
};

/**
 * @brief Assert if a pin belongs to the main controller.
 * @param[in] pin J21 header pin constant.
 * @return (int) Positive number if pin belongs to the main chip, 0 otherwise.
 */
#define IS_PIN_IN_MAIN_CONTROLLER(pin) (pin & GPIO_MAIN_CONTROLLER_FLAG)

/**
 * @brief Assert if a pin belongs to the aon controller.
 * @param[in] pin J21 header pin constant.
 * @return (int) Positive number if pin belongs to the aon chip, 0 otherwise.
 */
#define IS_PIN_IN_AON_CONTROLLER(pin)  (pin & GPIO_AON_CONTROLLER_FLAG)

/**
 * @brief Get line number for a pin.
 * @param[in] pin J21 header pin constant.
 * @return (int) Line number for the pin.
 */
#define GPIO_GET_LINE(pin)       (pin & 0x000000FF)

/**
 * @brief Get controller flag for a pin.
 * @param[in] pin J21 header pin constant.
 * @return Either GPIO_MAIN_CONTROLLER_FLAG or GPIO_AON_CONTROLLER_FLAG
 */
#define GPIO_GET_CONTROLLER(pin) (pin & 0xF0000000)

/**
 * @brief Pin type.
 * @details Wraps around the gpiod_line struct, from the gpiod lib. 
 */
typedef struct gpiod_line GPIO_Pin;

/**
 * @brief Pin bulk type.
 * @details Wraps around the gpiod_line_bulk struct, from the gpiod lib. 
 */
typedef struct gpiod_line_bulk GPIO_Bulk;

/**
 * @brief GPIO controller chip type.
 * @details Wraps around the gpiod_chip struct, from the gpiod lib. 
 */
typedef struct gpiod_chip GPIO_Controller;

/**
 * @brief Valid GPIO directions
 */
typedef enum gpio_direction{
    GPIO_DIRECTION_OUTPUT, /**< Output */ 
    GPIO_DIRECTION_INPUT, /**< Input */
    GPIO_DIRECTION_NONE /**< Used for only getting the line. Direction might be set later.*/
} gpio_direction_t;

/**
 * @brief Wrapper around gpiod_line_release.
 */
#define GPIO_release_pin gpiod_line_release

/**
 * @brief Wrapper around gpiod_line_set_value.
 */
#define GPIO_write gpiod_line_set_value

/**
 * @brief Wrapper around gpiod_line_set_value_bulk.
 */
#define GPIO_write_bulk gpiod_line_set_value_bulk

/**
 * @brief Wrapper around gpiod_line_get_value.
 */
#define GPIO_read gpiod_line_get_value

/**
 * @brief Wrapper around gpiod_line_get_value_bulk.
 */
#define GPIO_read_bulk gpiod_line_get_value_bulk

/**
 * @brief Initialize a pin on the J21 GPIO header.
 * 
 * @param[in] pin Symbol pertaining to the pin to initialize (eg. J21_HEADER_PIN_7)
 * @param[in] direction GPIO direction (GPIO_DIRECTION_OUTPUT, GPIO_DIRECTION_INPUT, or GPIO_DIRECTION_NONE)
 * @param[in] init_val Only relevant if direction=GPIO_DIRECTION_OUTPUT. Initial signal value of the pin.
 * @return (GPIO_Pin*) On success, handle to the initialized pin. Otherwise, NULL. 
 */
GPIO_Pin* GPIO_init_pin(unsigned int pin, gpio_direction_t direction, int init_val);

/**
 * @brief Initialize a bulk of pins on the J21 GPIO header.
 * 
 * Pins initialized this way can only be controlled groupally.
 * For controlling pins individually, initialize them with GPIO_init_pin.
 * 
 * @param[in] pins Array of symbols pertaining to the pin to initialize (eg. J21_HEADER_PIN_7).
 * @param[in] direction GPIO direction (GPIO_DIRECTION_OUTPUT, GPIO_DIRECTION_INPUT, or GPIO_DIRECTION_NONE).
 *                       Applied to all pins in the bulk.
 * @param[in] init_vals Only relevant if direction=GPIO_DIRECTION_OUTPUT. Array of initial values to write to the respective pins.
 * @param[in] count Amount of pins to initialize.
 * @return (GPIO_Bulk*) On success, handle to the initialized pin bulk. Otherwise, NULL.
 */
GPIO_Bulk* GPIO_init_bulk(unsigned int pins[], gpio_direction_t direction, int init_vals[], unsigned int count);

/**
 * @brief Get the J21 header pin constant corresponding to a pin number.
 * 
 * @param[in] p Pin number. 
 * @return (int) If p is valid, corresponding J21_HEADER const (eg. p=7 --> J21_HEADER_PIN_7). Otherwise, INVALID_PIN.
 */
int int_to_gpio_pin(int p);

#endif
