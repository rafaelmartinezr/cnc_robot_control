/*
 * Tasks.c
 * 
 * Author: Rafael Martinez
 * Date: 07.03.2021
 */

#define NDEBUG

#include "Tasks.h"
#include "debug.h"

typedef struct task_info{
    Task_t entry_func;
    pthread_t thread_id;
    size_t stack_size;
    char name[TASK_NAME_LEN];
    void* arg;
} task_info_t;

typedef struct task_node{
    task_info_t* task;
    struct task_node* next;
} task_node_t;

// Pointer to the head node of the task_list
static task_node_t* task_list = NULL;

#ifndef NDEBUG
/**
 * @brief Print node locations and links between nodes of the task_list
 * 
 * For debugging purposes only.
 */
static void print_list(void)
{
    task_node_t* current = task_list;

    DEBUG_PRINT("List start: %p", current);

    while(current != NULL){
        DEBUG_PRINT("Node %p -> %p", current, current->next);
        current = current->next;
    }
}
#endif

/**
 * @brief Add new node to the task_list 
 * 
 * @param[in] task Pointer to the task_info struct of the new task (previously initialized).
 */
static void list_insert_task(task_info_t* task)
{   
    // Create new node
    task_node_t* new_node = malloc(sizeof(task_node_t));
    memset(new_node, 0x00, sizeof(task_node_t));
    new_node->task = task;
    new_node->next = NULL;

    if(task_list == NULL){
        // if task list is empty, make new_node the head
        task_list = new_node;
    } else{
        // else, add new_node to the end of the list
        task_node_t* current = task_list;
        while(current->next != NULL)
            current = current->next;
        current->next = new_node;
    }

#ifndef NDEBUG
    print_list();
#endif
}

/**
 * @brief Find a specified task in the task_list
 * 
 * @param[in] thread_id ID of the task to find
 * @return (task_info_t*) Pointer to the task_info struct associated with the task found
 *                        NULL if no task is found.
 */
static task_info_t* list_find_task(pthread_t thread_id)
{
    task_node_t* current = task_list;
    // search for the specified task
    while(current != NULL && current->task->thread_id != thread_id)
        current = current->next;

    // if not found or list is empty, return null, else return result
    return (current == NULL) ? NULL : current->task;
}

/**
 * @brief Delete a task from the task_list
 * 
 * @param[in] thread_id ID of the task to delete
 */
static void list_delete_task(pthread_t thread_id)
{
    task_node_t* current = task_list;
    task_node_t* previous = NULL;

    // Search for corresponding entry
    while(current != NULL && current->task->thread_id != thread_id){
        previous = current;
        current = current->next;
    }

    // If entry was found, delete it
    if(current != NULL){
        if(previous == NULL)
            // If entry is the head, reset pointer to the start of the list 
            task_list = current->next;
        else
            // If entry is in the middle of the list, link previous node to the next node
            previous->next = current->next;

        free(current->task);
        free(current);
    }

#ifndef NDEBUG
    print_list();
#endif
}

/**
 * @brief Entry point for new threads. Calls user-specified entry function after registering the thread in the task_list.
 * 
 * @param[in] arg Pointer to the task_info struct associated with the new thread.
 * @return void* Does not matter 
 */
static void* thread_main(void* arg)
{
    task_info_t* task = arg;

    // Finish setting up the task
    task->thread_id = pthread_self();
    pthread_setname_np(pthread_self(), task->name);
    
    // Go to the entry point
    task->entry_func(task->arg);

    // After thread finishes, delete it from the list
    list_delete_task(pthread_self());

    return NULL;
}

/************************ PUBLIC API ************************/

/**
 * @brief Create and start a new task.
 * @param[in] name       String. Name of the task.
 * @param[in] stack_size Size in bytes for the stack of the new task.
 * @param[in] entry_func Entry function for the task.
 * @param[in] arg        Argument passed to the new task.
 * @return (Task_id_t) On success: Numerical id of the new task. On failure: 0. 
 */
Task_id_t CreateTask(const char* name, size_t stack_size, Task_t entry_func, void* arg)
{
    pthread_t thread_id = 0;

    // Parameter validation
    if(name == NULL){
        ERROR_PRINT("Name string is invalid.");
        goto exit;
    } else if(stack_size == 0 || stack_size > MAX_STACK_SIZE){
        ERROR_PRINT("Stack size is invalid.");
        goto exit;
    } else if(entry_func == NULL){
        ERROR_PRINT("Invalid entry point for the task.");
        goto exit;
    }

    // Setup the thread
    pthread_attr_t attribs;
    pthread_attr_init(&attribs);
    pthread_attr_setstacksize(&attribs, stack_size);

    // Create new linked list entry
    task_info_t* info = malloc(sizeof(task_info_t));
    if(info == NULL){
        ERROR_PRINT("Error allocating memory.");
        goto exit;
    }

    memset(info, 0, sizeof(task_info_t));

    // Initialize node (commented lines are redundant because of the memset)
    info->entry_func = entry_func;
    strncpy(info->name, name, TASK_NAME_LEN-1);
    info->stack_size = stack_size;
    info->arg = arg;
    // info->thread_id = 0;

    list_insert_task(info);

    // Create the thread
    if(pthread_create(&thread_id, &attribs, thread_main, (void*)info) != 0){
        ERROR_PRINT("Error creating new thread.");
        thread_id = 0;
    }

exit:
    return thread_id;
}

/**
 * @brief Get the id of a task with a given name.
 * 
 * @param[in] name Name of the task to lookup.
 * @return (Task_id_t) On success, ID of the task. Otherwise, 0. 
 */
Task_id_t Task_get_id_by_name(const char* name)
{
    Task_id_t task_id = 0;

    // Parameter validation
    if(name == NULL){
        ERROR_PRINT("Name string is invalid.");
        goto exit;
    }

    // Go through list until entry is found
    task_node_t* current = task_list;
    while(current != NULL && strncmp(name, current->task->name, TASK_NAME_LEN))
        current = current->next;

    if(current != NULL)
        task_id = current->task->thread_id;
    else{
        ERROR_PRINT("Task named '%s' not found.", name);
    }

exit:
    return task_id;
}

/**
 * @brief Kill a task.
 * 
 * @param[in] task_id Id of the task to kill.
 */
void Task_kill(Task_id_t task_id)
{
    // Parameter validation
    if(task_id == 0)
        return;
        
    // Remove task from task_list
    list_delete_task(task_id);
    // Kill it asynchronously
    pthread_cancel(task_id);
}
