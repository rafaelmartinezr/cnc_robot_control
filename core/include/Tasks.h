/**
 * @file Tasks.h
 * @author Rafael Martinez (rafael.martinez@udem.edu)
 * @brief Task management library public interface.
 * @details Library for creating new tasks and stopping current ones. Tasks are threads spawned from
 *          another thread, that execute a given routine. An internal linked list keeps track of the
 *          current tasks of the program. A tasks can be canceled asyncronously, which stops and kills
 *          it, and removes its reference from the linked list. Tasks can also be looked up based on their
 *          assigned named.
 * @version 1.0
 * @date 07.03.2021
 * 
 * @copyright Copyright (c) 2021
 */

#ifndef TASKS_H
#define TASKS_H

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <string.h>

/**
 * @brief Maximum stack size assignable to a task.
 */
#define MAX_STACK_SIZE (1024*1024)

/**
 * @brief Maximum name length for a task.
 */
#define TASK_NAME_LEN  32

/**
 * @brief Function pointer for tasks. 
 * @details Signature is void func(void*)
 */
typedef void (*Task_t)(void*);

/**
 * @brief Type for task IDs.
 */
typedef pthread_t Task_id_t;

/**
 * @brief Create and start a new task.
 * @param[in] name       String. Name of the task.
 * @param[in] stack_size Size in bytes for the stack of the new task.
 * @param[in] entry_func Entry function for the task.
 * @param[in] arg        Argument passed to the new task.
 * @return (Task_id_t) On success: Numerical id of the new task. On failure: 0. 
 */
Task_id_t CreateTask(const char* name, size_t stack_size, Task_t entry_func, void* arg);

/**
 * @brief Get the id of a task with a given name.
 * 
 * @param[in] name Name of the task to lookup.
 * @return (Task_id_t) On success, ID of the task. Otherwise, 0. 
 */
Task_id_t Task_get_id_by_name(const char* name);

/**
 * @brief Kill a task.
 * 
 * @param[in] task_id Id of the task to kill.
 */
void Task_kill(Task_id_t task_id);

#endif
