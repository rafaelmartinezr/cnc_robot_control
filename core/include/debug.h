/**
 * @file debug.h
 * @author Rafael Martinez (rafael.martinez@udem.edu)
 * @brief Debug utilities.
 * @version 1.0
 * @date 03.07.2021
 * 
 * @copyright Copyright (c) 2021
 */

#ifndef DEBUG_H
#define DEBUG_H

#include <stdio.h>

/**
 * @internal
 * @brief Symbol/name to string
 */
#define STRINGIFY(x) #x 

/**
 * @brief Convert symbol to string.
 * @param[in] x Symbol 
 */
#define TOSTRING(x) STRINGIFY(x) 

/**
 * @brief Printf-like macro for printing debug messages.
 * 
 * Newline inserted automatically at the end.
 * Can be disabled at local file scope by defining NDEBUG at the beginning of the source file.
 * If disabled, statements are discarded from the source file at compilation time.
 * 
 * @param[in] fmt String, can include format specifiers. 
 */
#ifndef NDEBUG
#define DEBUG_PRINT(fmt, ...) printf(__FILE__":"TOSTRING(__LINE__)": "fmt"\n", ##__VA_ARGS__)
#else
#define DEBUG_PRINT(fmt, ...) 
#endif

/**
 * @brief Printf-like macro for printing error messages.
 * 
 * This macro can not be disabled.
 * 
 * @param[in] fmt String, can include format specifiers.
 */
#define ERROR_PRINT(fmt, ...) fprintf(stderr, "In file %s, function %s:%d: " fmt "\n", __FILE__, __FUNCTION__, __LINE__, ##__VA_ARGS__)

#endif
