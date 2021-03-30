/*
 * Time.c
 * 
 * Author: Rafael Martinez
 * Date: 07.03.2021
 */

//#define NDEBUG

#include "Time.h"
#include "debug.h"

/**
 * @brief Millisecond delay
 * 
 * @param[in] ms Time to pause in milliseconds
 */
void Delay_ms(long ms)
{
    if(ms < 0)
        return; 
    
    time_t s = ms / MILLI_IN_SECOND;
    long ns = (ms % MILLI_IN_SECOND) * NANO_IN_MILLI;

    struct timespec del = {.tv_nsec = ns, .tv_sec = s};
    clock_nanosleep(CLOCK_MONOTONIC, 0, &del, NULL);
}

/**
 * @brief Microsecond delay
 * 
 * @param[in] us Time to pause in microseconds
 */
void Delay_us(long us)
{
    if(us < 0)
        return;

    time_t s = us / MICRO_IN_SECOND;
    long ns = (us % MICRO_IN_SECOND) * NANO_IN_MICRO;

    struct timespec del = {.tv_nsec = ns, .tv_sec = s};
    clock_nanosleep(CLOCK_MONOTONIC, 0, &del, NULL);
}

/**
 * @brief Nanosecond delay
 * 
 * @param[in] ns Time to pause in nanoseconds
 */
void Delay_ns(long ns)
{
    if(ns < 0)
        return;

    time_t s = ns / NANO_IN_SECOND;
    ns = ns % NANO_IN_SECOND;

    struct timespec del = {.tv_nsec = ns, .tv_sec = s};
    clock_nanosleep(CLOCK_MONOTONIC, 0, &del, NULL);
}

/**
 * @brief Add two timespec structs
 * 
 * @param[in] ta Time A
 * @param[in] tb Time B
 * @param[out] result Time A+B
 */
void add_time(const struct timespec* ta, const struct timespec* tb, struct timespec* result)
{
    long ns = ta->tv_nsec + tb->tv_nsec;
    time_t s = ta->tv_sec + tb->tv_sec;
    if(ns > NANO_IN_SECOND){
        ns -= NANO_IN_SECOND;
        s++;
    }
    result->tv_nsec = ns, result->tv_sec = s;
}

/**
 * @brief Substract two timespec structs. It is assumed that ta > tb.
 * 
 * @param[in] ta Time A
 * @param[in] tb Time B
 * @param[out] result Time A-B
 */
void sub_time(const struct timespec* ta, const struct timespec* tb, struct timespec* result)
{
    time_t sec = ta->tv_sec - tb->tv_sec;
    long ns = ta->tv_nsec - tb->tv_nsec;
    if(ns < 0){
        ns += NANO_IN_SECOND;
        sec--;
    }

    result->tv_nsec = ns,result->tv_sec = sec;
}

/**
 * @brief Multiply timespec by a positive integer
 * 
 * @param[in] ta Time
 * @param[in] b Positive integer
 * @param[out] result ta * b, in a timespec struct 
 */
void mul_time(const struct timespec* ta, unsigned int b, struct timespec* result)
{
    if(b == 0){
        result->tv_sec = 0, result->tv_nsec = 0;
        return;
    }

    unsigned long s = ta->tv_sec * b;
    unsigned long ns = ta->tv_nsec * b;

    if(ns > NANO_IN_SECOND){
        s += ns / NANO_IN_SECOND;
        ns -= ns % NANO_IN_SECOND;
    }

    result->tv_sec = (time_t)s, result->tv_nsec = (long)ns;
}

/**
 * @brief Divide a timespec by a positive integer. Returns 0 if b == 0 
 * 
 * @param[in] ta Time
 * @param[in] b Positive integer
 * @param[out] result ta / b, in a timespec struct
 */
void div_time(const struct timespec* ta, unsigned int b, struct timespec* result)
{
    unsigned long s, ns;

    if(b == 0){
        s = 0;
        ns = 0;
    } else{
        s = ta->tv_sec / b;
        ns  = ta->tv_sec % b;
        ns = ns * NANO_IN_SECOND + ta->tv_nsec;
        ns /= b;
    }

    result->tv_sec = s; result->tv_nsec = ns;
}

/**
 * @brief Convert timespec struct to double
 * !!!WARNING!!!: Depending on the relative magnitudes of the second and nanosecond
 * fields in the timespec struct, conversion may result in a loss of precision.
 * @param[in] ta Time to convert
 * @return (double) Converted time 
 */
double time_to_double(const struct timespec* ta)
{
    return (double)ta->tv_nsec/(double)NANO_IN_SECOND + (double)ta->tv_sec;
}

/**
 * @brief Initialize a timestruct with a time in microseconds
 * 
 * @param[in,out] ts Pointer to the struct to initialize 
 * @param[in] us Time in microseconds
 */
void microsec_to_timespec(struct timespec* ts, unsigned int us)
{
    long ns = us * NANO_IN_MICRO;
    time_t s = 0;

    if(ns > NANO_IN_SECOND){
        s = ns % NANO_IN_SECOND;
        ns -= s * NANO_IN_SECOND;
    }

    ts->tv_nsec = ns;
    ts->tv_sec = s;
}

/**
 * @brief Print in human-readable form a timespec
 * 
 * @param[in] t Time to print
 */
void print_time(const struct timespec* t)
{
    printf("%ld.%09ld", t->tv_sec, t->tv_nsec);
}

/**
 * @brief Save in a buffer a timespec in human-readable form
 * 
 * @param[in] t Time to save
 * @param[out] buf Buffer in which to store the time
 * @param[in] buf_size Size of the buffer in bytes
 */
void snprint_time(const struct timespec* t, char* buf, size_t buf_size)
{
    snprintf(buf, buf_size, "%ld.%09ld", t->tv_sec, t->tv_nsec);
}
