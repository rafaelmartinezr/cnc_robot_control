/**
 * @file Time.h
 * @author Rafael Martinez (rafael.martinez@udem.edu)
 * @brief Time library public interface.
 * @details Library with functions for generating blocking delays, and doing arithmetic operations
 *          on timespec structs. It also offers some additional utilities for printing timespec structs
 *          and converting an int to a timespec.
 * @version 1.0
 * @date 03.07.2021
 * 
 * @copyright Copyright (c) 2021
 */

#ifndef DELAYS_H
#define DELAY_H

#include <time.h>
#include <stdio.h>

/**
 * @brief Amount of nanoseconds in a second
 */
#define NANO_IN_SECOND  ((unsigned int)(1000000000))  
/**
 * @brief Amount of microseconds in a second
 */
#define MICRO_IN_SECOND ((unsigned int)(1000000))     
/**
 * @brief Amount of milliseconds in a second
 */
#define MILLI_IN_SECOND ((unsigned int)(1000))        
/**
 * @brief Amount of nanoseconds in a microsecond
 */
#define NANO_IN_MICRO   ((unsigned int)(1000))        
/**
 * @brief Amount of nanoseconds in a millisecond
 */ 
#define NANO_IN_MILLI   ((unsigned int)(1000000))     


/**
 * @brief Millisecond delay
 * 
 * @param[in] ms Time to pause in milliseconds
 */
void Delay_ms(long ms);

/**
 * @brief Microsecond delay
 * 
 * @param[in] us Time to pause in microseconds
 */
void Delay_us(long us);

/**
 * @brief Nanosecond delay
 * 
 * @param[in] ns Time to pause in nanoseconds
 */
void Delay_ns(long ns);

/**
 * @brief Add two timespec structs
 * 
 * @param[in] ta Time A
 * @param[in] tb Time B
 * @param[out] result Time A+B
 */
void add_time(const struct timespec* ta, const struct timespec* tb, struct timespec* result);

/**
 * @brief Substract two timespec structs. It is assumed that ta > tb.
 * 
 * @param[in] ta Time A
 * @param[in] tb Time B
 * @param[out] result Time A-B
 */
void sub_time(const struct timespec* ta, const struct timespec* tb, struct timespec* result);

/**
 * @brief Multiply timespec by a positive integer
 * 
 * @param[in] ta Time
 * @param[in] b Positive integer
 * @param[out] result ta * b, in a timespec struct 
 */
void mul_time(const struct timespec* ta, unsigned int b, struct timespec* result);

/**
 * @brief Divide a timespec by a positive integer. Returns 0 if b == 0 
 * 
 * @param[in] ta Time
 * @param[in] b Positive integer
 * @param[out] result ta / b, in a timespec struct
 */
void div_time(const struct timespec* ta, unsigned int b, struct timespec* result);

/**
 * @brief Convert timespec struct to double
 * 
 * !!!WARNING!!!: Depending on the relative magnitudes of the second and nanosecond
 * fields in the timespec struct, conversion may result in a loss of precision.
 * 
 * @param[in] ta Time to convert
 * @return (double) Converted time 
 */
double time_to_double(const struct timespec* ta);

/**
 * @brief Initialize a timestruct with a time in microseconds
 * 
 * @param[in,out] ts Pointer to the struct to initialize 
 * @param[in] us Time in microseconds
 */
void microsec_to_timespec(struct timespec* ts, unsigned int us);

/**
 * @brief Print in human-readable form a timespec
 * 
 * @param[in] t Time to print
 */
void print_time(const struct timespec* t);

/**
 * @brief Save in a buffer a timespec in human-readable form
 * 
 * @param[in] t Time to save
 * @param[out] buf Buffer in which to store the time
 * @param[in] buf_size Size of the buffer in bytes
 */
void snprint_time(const struct timespec* t, char* buf, size_t buf_size);

#endif
