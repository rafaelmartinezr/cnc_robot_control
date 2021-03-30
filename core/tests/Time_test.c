#include "Time.h"

static int test_opstime(void)
{
    int retVal = 0;

    struct timespec in_a[3], in_b[3], out[3];
    struct timespec result;

    puts("###### TEST -- ADD TIME ######");

    in_a[0].tv_sec = 123, in_a[0].tv_nsec = 111111111;
    in_b[0].tv_sec = 456, in_b[0].tv_nsec = 888888888;
    out[0].tv_sec = 579, out[0].tv_nsec = 999999999;

    in_a[1].tv_sec = 1, in_a[1].tv_nsec = 999999999;
    in_b[1].tv_sec = 1, in_b[1].tv_nsec = 999999999;
    out[1].tv_sec  = 3, out[1].tv_nsec  = 999999998;

    in_a[2].tv_sec = 0, in_a[2].tv_nsec = 999999999;
    in_b[2].tv_sec = 0, in_b[2].tv_nsec = 000000001;
    out[2].tv_sec  = 1, out[2].tv_nsec  = 000000000;

    for(int i = 0; i < 3; i++){
        DEBUG_PRINT("TEST %d: ", i);
        add_time(&in_a[i], &in_b[i], &result);

        if(result.tv_sec != out[i].tv_sec || result.tv_nsec != out[i].tv_nsec){
            DEBUG_PRINT("FAILED! [result = %ld.%09lu]\n", result.tv_sec, result.tv_nsec);
            retVal = -1;
        } else{
            DEBUG_PRINT("PASSED!\n");
        }
    }

    puts("###### TEST -- SUBSTRACT TIME ######");

    in_a[0].tv_sec = 100, in_a[0].tv_nsec = 777777777;
    in_b[0].tv_sec = 100, in_b[0].tv_nsec = 333333333;
    out[0].tv_sec  = 0,   out[0].tv_nsec  = 444444444;

    in_a[1].tv_sec = 100, in_a[1].tv_nsec = 666666666;
    in_b[1].tv_sec = 50, in_b[1].tv_nsec = 888888888;
    out[1].tv_sec = 49, out[1].tv_nsec  = 777777778;

    in_a[2].tv_sec = 999, in_a[2].tv_nsec = 555555555;
    in_b[2].tv_sec = 100, in_b[2].tv_nsec = 444444444;
    out[2].tv_sec  = 899, out[2].tv_nsec  = 111111111;

    for(int i = 0; i < 3; i++){
        DEBUG_PRINT("TEST %d: ", i);
        sub_time(&in_a[i], &in_b[i], &result);

        if(result.tv_sec != out[i].tv_sec || result.tv_nsec != out[i].tv_nsec){
            DEBUG_PRINT("FAILED! [result = %ld.%09lu]\n", result.tv_sec, result.tv_nsec);
            retVal = -1;
        } else{
            DEBUG_PRINT("PASSED!\n");
        }
    }

    puts("###### TEST -- MULTIPLY TIME ######");
    unsigned int m[3] = {2, 1234, 0};

    in_a[0].tv_sec = 5,  in_a[0].tv_nsec = 123454321;
    out[0].tv_sec  = 10, out[0].tv_nsec  = 246908642;

    in_a[1].tv_sec = 98765,     in_a[1].tv_nsec = 987656789;
    out[1].tv_sec  = 121877228, out[1].tv_nsec  = 768477626;

    clock_gettime(CLOCK_MONOTONIC, &in_a[2]);
    out[2].tv_sec = 0, out[2].tv_nsec = 0;

    for(int i = 0; i < 3; i++){
        DEBUG_PRINT("TEST %d: ", i);
        mul_time(&in_a[i], m[i], &result);

        if(result.tv_sec != out[i].tv_sec || result.tv_nsec != out[i].tv_nsec){
            DEBUG_PRINT("FAILED! [result = %ld.%09lu]\n", result.tv_sec, result.tv_nsec);
            retVal = -1;
        } else{
            DEBUG_PRINT("PASSED!\n");
        }
    }

    puts("###### TEST -- DIVIDE TIME ######");
    unsigned int d[3] = {7, 1234, 0};

    in_a[0].tv_sec = 5, in_a[0].tv_nsec = 123454321;
    out[0].tv_sec  = 0, out[0].tv_nsec  = 731922045;

    in_a[1].tv_sec = 98765, in_a[1].tv_nsec = 987656789;
    out[1].tv_sec  = 80,    out[1].tv_nsec  = 37267144;

    clock_gettime(CLOCK_MONOTONIC, &in_a[2]);
    out[2].tv_sec = 0, out[2].tv_nsec = 0;

    for(int i = 0; i < 3; i++){
        DEBUG_PRINT("TEST %d: ", i);
        div_time(&in_a[i], d[i], &result);

        if(result.tv_sec != out[i].tv_sec || result.tv_nsec != out[i].tv_nsec){
            DEBUG_PRINT("FAILED! [result = %ld.%09lu]\n", result.tv_sec, result.tv_nsec);
            retVal = -1;
        } else{
            DEBUG_PRINT("PASSED!\n");
        }
    }

    return retVal; 
}

static int test_delays(void)
{
    struct timespec start, stop, diff;

    puts("###### TEST -- 50 MS DELAY ######");
    clock_gettime(CLOCK_MONOTONIC, &start);
    Delay_ms(50);
    clock_gettime(CLOCK_MONOTONIC, &stop);
    sub_time(&stop, &start, &diff);
    DEBUG_PRINT("Start time: "), print_time(&start);
    DEBUG_PRINT("Stop time:  "), print_time(&stop);
    DEBUG_PRINT("Diff: "), print_time(&diff);

    puts("###### TEST -- 700 US DELAY ######");
    clock_gettime(CLOCK_MONOTONIC, &start);
    Delay_us(700);
    clock_gettime(CLOCK_MONOTONIC, &stop);
    sub_time(&stop, &start, &diff);
    DEBUG_PRINT("Start time: "), print_time(&start);
    DEBUG_PRINT("Stop time:  "), print_time(&stop);
    DEBUG_PRINT("Diff: "), print_time(&diff);

    puts("###### TEST -- 1000 NS DELAY ######");
    clock_gettime(CLOCK_MONOTONIC, &start);
    Delay_ns(1000);
    clock_gettime(CLOCK_MONOTONIC, &stop);
    sub_time(&stop, &start, &diff);
    DEBUG_PRINT("Start time: "), print_time(&start);
    DEBUG_PRINT("Stop time:  "), print_time(&stop);
    DEBUG_PRINT("Diff: "), print_time(&diff);

    int del = 5000;

    puts("###### TEST -- US DELAY 100 REP WITH DELAY FUNC######");
    clock_gettime(CLOCK_MONOTONIC, &start);
    for(int i = 0; i < 100; i++){
        Delay_us(del);
    }
    clock_gettime(CLOCK_MONOTONIC, &stop);
    sub_time(&stop, &start, &diff);
    DEBUG_PRINT("Diff: "), print_time(&diff);

    puts("###### TEST -- US DELAY 100 REP WITH CLOCK_NANOSLEEP######");
    struct timespec delay = {.tv_sec = 0, .tv_nsec = del*NANO_IN_MICRO};
    clock_gettime(CLOCK_MONOTONIC, &start);
    for(int i = 0; i < 100; i++){
        clock_nanosleep(CLOCK_MONOTONIC, 0, &delay, NULL);
    }
    clock_gettime(CLOCK_MONOTONIC, &stop);
    sub_time(&stop, &start, &diff);
    DEBUG_PRINT("Diff: "), print_time(&diff);

    return 0;
}

int main(int argc, char const * argv[])
{
    test_opstime();
    test_delays();
    return 0;
}
