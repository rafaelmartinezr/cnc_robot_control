#define NDEBUG

#include "config.h"
#include "debug.h"

// TODO: Handle the case of repeated names for motors and axes.

// Absolute path of the config file.
#define MOTOR_CONFIG_PATH BASE_PATH MOTOR_CONFIG_NAME

#define PARAM_MAX_LEN 32
#define VALUE_MAX_LEN 32
#define ERROR_STR_LEN 64

// Helper struct to store the info read for a motor.
struct motor_config{
    char name[MOTOR_NAME_LEN];
    int step_pin;
    int dir_pin;
    unsigned int microstep;
    unsigned int steps_rot;
    unsigned int direction;
    Stepper* motor;
};

// Helper struct to store the info read for an axis.
struct axis_config{
    char name[AXIS_NAME_LEN];
    unsigned int mm_rot;
    unsigned int count;
    struct motor_config* motors[MOTOR_LIST_SIZE_MAX];
    Axis* axis;
};

// Valid states for the state machine.
enum states{
    READ_LINE,
    READ_PARAM,
    READ_IDENTIFIER,
    SET_IDENTIFIER,
    CHECK_PARAM,
    READ_VALUE,
    SET_PARAM,
    READ_MOTOR_LIST,
    CLEANUP,
    FINISHED,
    ERROR
};

// Valid type identifiers in the objects in the config file.
enum identifiers{
    MOTOR,
    AXIS,
    INVALID_TYPE
};

// Valid parameters for the objects in the config file.
enum params{
    MOTOR_NAME,
    STEP_PIN,
    DIR_PIN,
    STEPS_ROT,
    DIRECTION,
    MICROSTEP,
    AXIS_NAME,
    MOTOR_LIST,
    MM_ROT,
    INVALID_PARAM
};

// Parameter list data for motor objects
static const char* motor_params[] = {"name", "step_pin", "dir_pin", "steps_per_rotation", "direction", "microstep"};
static const enum params motor_params_id[] = {MOTOR_NAME, STEP_PIN, DIR_PIN, STEPS_ROT, DIRECTION, MICROSTEP}; //Corresponding symbol for the string in motor_params
static const int motor_params_len[] = {4, 8, 7, 18, 9, 9};  //Lenght of corresponding string in motor_params, without the NULL terminator. 
static const int motor_params_count = sizeof(motor_params)/sizeof(char*);

// Parameter list data for axis objects
static const char* axis_params[] = {"name", "motors", "mm_per_rotation"};
static const enum params axis_params_id[] = {AXIS_NAME, MOTOR_LIST, MM_ROT}; //Corresponding symbol for the string in axis_params
static const int axis_params_len[] = {4, 6, 15}; //Lenght of corresponding string in axis_params, without the NULL terminator. 
static const int axis_params_count = sizeof(axis_params)/sizeof(char*);

// Current state of the state machine.
static volatile enum states motor_config_state;
// Buffer for the error message.
static char err_str[ERROR_STR_LEN];

// Static list for configuration of motor objects read from the config file.
static struct motor_config motor_list[MOTOR_LIST_SIZE_MAX];
static int motor_list_len = 0;
// Static list for configuration of motor objects read from the config file.
static struct axis_config axis_list[MOTOR_LIST_SIZE_MAX];
static int axis_list_len = 0;

/**
 * @brief Check if an entry in the motor_list is valid.
 * 
 * @param node (in) Pointer to the entry to check.
 * @return (int) If valid, 1. Otherwise, 0. 
 */
static inline int validate_motor_node(struct motor_config* node)
{
    return node->dir_pin != 0 && 
           node->step_pin != 0 &&
           node->direction != DIRECTION_INVALID &&
           node->name[0] != 0 &&
           node->steps_rot != 0 &&
           node->microstep != 0;
}

/**
 * @brief Check if an entry in the axis_list is valid.
 * 
 * @param node (in) Pointer to the entry to check.
 * @return (int) If valid, 1. Otherwise, 0. 
 */
static inline int validate_axis_node(struct axis_config* node)
{
    return node->count > 0 &&
           node->mm_rot > 0 &&
           node->name != NULL;
}

/**
 * @brief Convert a string with a motor direction to the corresponding constant. 
 * 
 * @param s (in) String to convert.
 * @return (int) If s = "counterclockwise" -> DIRECTION_CLOCKWISE; respective behaivor for s="clockwise".
 *               Invalid values of s return DIRECTION_INVALID
 */
static int str_to_dir(const char* s)
{
    int rv = strncmp(s, "counterclockwise", 16);
    if(rv == 0)
        return DIRECTION_COUNTERCLOCKWISE;
    
    rv = strncmp(s, "clockwise", 16);
    if(rv == 0)
        return DIRECTION_CLOCKWISE;
    
    return DIRECTION_INVALID;
}

/**
 * @brief Convert a string to a positive integer.
 * 
 * @param s (in) String to convert.
 * @return (int) If s is valid (only contains numbers), the corresponding number is returned. Otherwise, -1.
 */
static int str_to_int(const char* s)
{
    DEBUG_PRINT("%s", s);

    int flag = 1;

    // Validate string
    for(int i = 0; s[i] != '\0' && flag; i++){
        flag &= isdigit(s[i]) ? 1 : 0;
        DEBUG_PRINT("%d", flag);
    }

    // Convert string only if it is valid
    if(flag)
        return atoi(s);
    else
        return -1;
}

/**
 * @brief Find entry in the motor_list with a given name.
 * 
 * @param name (in) String. Name to search for.
 * @return (struct motor_config*) On success (entry exists), pointer to the entry in the list. Otherwise, NULL.
 */
static struct motor_config* get_motor_node_by_name(const char* name)
{  
    struct motor_config* ret = NULL;
    struct motor_config* node = NULL;

    for(int i = 0; i < motor_list_len; i++){
        node = &motor_list[i];
        DEBUG_PRINT("%s ?= %s", name, node->name);
        if(!strncmp(name, node->name, MOTOR_NAME_LEN)){
            ret = node;
            break;
        }
    }

    return ret;
}

/**
 * @brief Find entry in the axis_list with a given name.
 * 
 * @param name (in) String. Name to search for.
 * @return (struct axis_config*) On success (entry exists), pointer to the entry in the list. Otherwise, NULL.
 */
static struct axis_config* get_axis_node_by_name(const char* name)
{  
    struct axis_config* ret = NULL;
    struct axis_config* node = NULL;

    for(int i = 0; i < motor_list_len; i++){
        node = &axis_list[i];
        DEBUG_PRINT("%s ?= %s", name, node->name);
        if(!strncmp(name, node->name, MOTOR_NAME_LEN)){
            ret = node;
            break;
        }
    }

    return ret;
}

/**
 * @brief Routine for the READ_LINE state of the state machine.
 * 
 * Reads a line from the config file.
 * Buffer for the line is malloc()'d, so it needs to be free()'d after use.
 * 
 * @param file (in) File handle.
 * @param buff (out) Pointer to char*. Set to the address of the line buffer on return.
 * @param len  (out) Pointer to int. Set to the size of the buffer on return.
 */
void state_read_line(FILE* file, char** buff, size_t* len)
{
    DEBUG_PRINT("%s", __FUNCTION__);

    errno = 0;
    ssize_t rv = getline(buff, len, file);
    if(errno != 0){
        // Error on getline.
        strncpy(err_str, "Error reading line from " MOTOR_CONFIG_NAME, ERROR_STR_LEN-1);
        motor_config_state = ERROR;
    } else if (rv < 0){
        // EOF reached.
        motor_config_state = FINISHED;
    } else{
        // Line read successfully.
        DEBUG_PRINT("Read line: %s", *buff);
        motor_config_state = READ_PARAM;
    }
}

/**
 * @brief Routine for the READ_PARAM state of the state machine.
 * 
 * Read the next param to be set.
 * 
 * @param line_buff (in) Buffer containing the current line.
 * @param line_len (in) Size of line_buff.
 * @param param_buff (out) Buffer in which to store the read param.
 * @param param_len (out) On return, set to the size of the string in param_buff (without the NULL terminator).
 * @param offset (in-out) At call, current position of the cursor on the line. At return, set to the new position of the cursor.
 */
static void state_read_param(const char* line_buff, const int line_len, char* param_buff, int* param_len, int* offset)
{
    DEBUG_PRINT("%s", __FUNCTION__);
    
    DEBUG_PRINT("offset: %d", *offset);

    char c;
    *param_len = 0;

    for(; *offset < line_len; (*offset)++){
        c = line_buff[*offset];

        // Only lowercase characters and '_' are valid in a param.
        if(islower(c) || c == '_'){
            if(*param_len < PARAM_MAX_LEN - 1){
                param_buff[(*param_len)++] = c;
                continue;
            } else{
                strncpy(err_str, "Param identifier has exceeded max length in " MOTOR_CONFIG_NAME, ERROR_STR_LEN-1);
                motor_config_state = ERROR;
                break;
            }

        // Param is finished, move on to read the value.
        } else if(c == '='){
            motor_config_state = CHECK_PARAM;
            break;
        
        // This is not a param, this is a type identifier.
        } else if(c == '['){
            motor_config_state = READ_IDENTIFIER;
            break;

        // Found the start of a comment or the end of the line.
        } else if(c == '#' || isblank(c) || c == '\n'){
            motor_config_state = CLEANUP;
            break;

        // Invalid character found.
        } else{
            snprintf(err_str, ERROR_STR_LEN-1, "Invalid char (%c [0x%02hhx]) at param in " MOTOR_CONFIG_NAME, c, c);
            motor_config_state = ERROR;
            break;
        }
    }

    (*offset)++;
    param_buff[*param_len] = 0;
}

/**
 * @brief Routine for the READ_IDENTIFIER state of the state machine.
 * 
 * Read the next type identifier to be set.
 * 
 * @param line_buff (in) Buffer containing the current line.
 * @param line_len (in) Size of line_buff.
 * @param param_buff (out) Buffer in which to store the read type identifier.
 * @param param_len (out) On return, set to the size of the string in param_buff (without the NULL terminator).
 * @param offset (in-out) At call, current position of the cursor on the line. At return, set to the new position of the cursor.
 */
static void state_read_identifier(const char* line_buff, const int line_len, char* param_buff, int* param_len, int* offset)
{
    DEBUG_PRINT("%s", __FUNCTION__);
    DEBUG_PRINT("offset: %d", *offset);
    
    char c;
    *param_len = 0;

    for(; *offset < line_len; (*offset)++){
        c = line_buff[*offset];

        // Only lowercase characters are valid for type identifiers.
        if(islower(c)){
            if(*param_len < PARAM_MAX_LEN - 1){
                param_buff[(*param_len)++] = c;
                continue;
            } else{
                strncpy(err_str, "Type identifier has exceeded max length in " MOTOR_CONFIG_NAME, ERROR_STR_LEN-1);
                motor_config_state = ERROR;
                break;
            }

        // Reached the end of the identifier, move on to set it.
        } else if(c == ']'){
            motor_config_state = SET_IDENTIFIER;
            break;
        
        // Invalid character found.
        } else{
            snprintf(err_str, ERROR_STR_LEN-1, "Invalid char '%c' (0x%02hhx) at type identifier in " MOTOR_CONFIG_NAME, c, c);
            motor_config_state = ERROR;
            break;
        }
    }

    (*offset)++;
    param_buff[*param_len] = 0;
}

/**
 * @brief Routine for the SET_IDENTIFIER state of the state machine.
 * 
 * Start a new config entry in the list of the respective type read, if it is valid.
 * Params and values read from now on are assigned to this new entry.
 * 
 * @param id_buff (in) String. Contains the type identifier read from the file,
 * @param id (out) Set to the constant respective to the type identifier in id_buff, if it is valid. 
 *                 Otherwise, set to INVALID_TYPE.
 */
static void state_set_identifier(const char* id_buff, enum identifiers* id)
{
    DEBUG_PRINT("%s", __FUNCTION__);
    
    int rv;

    rv = strncmp(id_buff, "motor", 5);
    if(rv == 0){
        *id = MOTOR;
        motor_list_len++;
        motor_config_state = CLEANUP;
        return;
    }

    rv = strncmp(id_buff, "axis", 4);
    if(rv == 0){
        *id = AXIS;
        axis_list_len++;
        motor_config_state = CLEANUP;
        return;
    }

    snprintf(err_str, ERROR_STR_LEN-1, "Invalid type identifier (%s) used in " MOTOR_CONFIG_NAME, id_buff);
    *id = INVALID_TYPE;
    motor_config_state = ERROR;
    return;
}

/**
 * @brief Routine for the CHECK_PARAM state of the state machine.
 * 
 * Validate param string read from the config file.
 * 
 * @param param_buff (in) Buffer containing the param string.
 * @param id (in) Type identifier of the object currently being configured.
 * @param param_id (out) If param is valid, set to the corresponding constant. Otherwise, set to INVALID_PARAM.
 */
static void state_check_param(const char* param_buff, const enum identifiers id, enum params* param_id)
{
    DEBUG_PRINT("%s", __FUNCTION__);

    const char** param_list = NULL;
    const int* param_len = NULL;
    int list_len = 0;
    const enum params* id_list = NULL;
    
    //If current identifier type is valid, set the respective parameters for the validation loop.
    if(id == MOTOR){
        param_list = motor_params;
        param_len = motor_params_len;
        list_len = motor_params_count;
        id_list = motor_params_id;
    } else if(id == AXIS) {
        param_list = axis_params;
        param_len = axis_params_len;
        list_len = axis_params_count;
        id_list = axis_params_id;
    
    // If identifier is invalid, error.
    } else{
        strncpy(err_str, "Last type identifier is invalid or not defined", ERROR_STR_LEN-1);
        motor_config_state = ERROR;
        *param_id = INVALID_PARAM;
        goto exit;
    }

    // Check if param=="motors", which is a special case, and has its own state for reading the list.
    int match = !strncmp(param_buff, "motors", 6);
    if(match){
        motor_config_state = READ_MOTOR_LIST;
        *param_id = MOTOR_LIST;
        goto exit;
    }

    // Validation loop. 
    // Check if param to set is valid for the given type identifier.
    for(int i = 0; i < list_len; i++){
        match = !strncmp(param_buff, param_list[i], param_len[i]);
        DEBUG_PRINT("%s ?= %s (%d)", param_buff, param_list[i], match);
        if(match){
            motor_config_state = READ_VALUE;
            *param_id = id_list[i];
            goto exit;
        }
    }

    // Only comes here if no match was found
    char* identifier_str = (id == MOTOR) ? "motor" : "axis";
    snprintf(err_str, ERROR_STR_LEN-1, "%s is not a valid parameter for type %s, in " MOTOR_CONFIG_NAME, param_buff, identifier_str);
    *param_id = INVALID_PARAM;
    motor_config_state = ERROR;

exit:
    return;
}

/**
 * @brief Routine for the READ_VALUE state of the state machine.
 * 
 * Read the value to assign to the current param being set.
 * 
 * @param line_buff (in) Buffer containing the current line.
 * @param line_len (in) Size of line_buff.
 * @param value_buff (out) Buffer in which to store the read value.
 * @param value_len (out) On return, set to the size of the string in value_buff (without the NULL terminator).
 * @param offset (in-out) At call, current position of the cursor on the line. At return, set to the new position of the cursor.
 */
static void state_read_value(const char* line_buff, const int line_len, char* value_buff, int* value_len, int* offset)
{
    DEBUG_PRINT("%s", __FUNCTION__);
    DEBUG_PRINT("offset: %d", *offset);

    char c;
    *value_len = 0;

    for(; *offset < line_len; (*offset)++){
        c = line_buff[*offset];

        // Numbers, lower- and uppercase characters, '-' and '_' are valid characters for a value.
        if(isalnum(c) || c == '-' || c == '_'){
            if(*value_len < PARAM_MAX_LEN - 1){
                value_buff[(*value_len)++] = c;
                continue;
            } else{
                strncpy(err_str, "Value has exceeded max length in " MOTOR_CONFIG_NAME, ERROR_STR_LEN-1);
                motor_config_state = ERROR;
                break;
            }

        // Found the start of a comment or the end of the line.
        } else if(isblank(c) || c == '#' || c == '\n'){
            motor_config_state = SET_PARAM;
            break;
        
        // Found an invalid character.
        } else{
            snprintf(err_str, ERROR_STR_LEN-1, "Invalid char (%c [0x%02hhx]) at type identifier in " MOTOR_CONFIG_NAME, c, c);
            motor_config_state = ERROR;
            break;
        }
    }

    (*offset)++;
    value_buff[*value_len] = 0;
}

/**
 * @brief Routine for the SET_PARAM state of the state machine.
 * 
 * Sets the value assigned to the current param.
 * 
 * @param param_id (in) Id of the param to set, according to the params enum.
 * @param value_buff (in) Buffer containing the string with the value to assign.
 */
static void state_set_param(const enum params param_id, const char* value_buff)
{
    DEBUG_PRINT("%s", __FUNCTION__);

    int temp = 0;

    motor_config_state = CLEANUP; // Next state is cleanup, unless an error occurs.

    switch(param_id){

        case MOTOR_NAME:
            // Set the name of the motor. String passed as is.
            strncpy(motor_list[motor_list_len-1].name, value_buff, MOTOR_NAME_LEN - 1);
            motor_list[motor_list_len-1].name[MOTOR_NAME_LEN-1] = 0;
            break;

        case STEP_PIN:
            // Validate the string and convert to a number if valid.
            temp = str_to_int(value_buff);
            if(temp > 0){
                // Get corresponding gpio pin constant for the given pin number. 
                temp = int_to_gpio_pin(temp);
                if(temp != (int)INVALID_PIN){
                    motor_list[motor_list_len-1].step_pin = temp; // Set motor's step pin.
                } else{
                    // Error if pin number is invalid.
                    snprintf(err_str, ERROR_STR_LEN-1, "%s is not a valid value for step_pin.", value_buff);
                    motor_config_state = ERROR;
                }
            } else{
                // Error if string is invalid.
                snprintf(err_str, ERROR_STR_LEN-1, "%s is not a valid numerical value.", value_buff);
                motor_config_state = ERROR;
            }
            break;
        
        case DIR_PIN:
            // Validate the string and convert to a number if valid.
            temp = str_to_int(value_buff);
            if(temp > 0){
                // Get corresponding gpio pin constant for the given pin number. 
                temp = int_to_gpio_pin(temp);
                if(temp != (int)INVALID_PIN){
                    motor_list[motor_list_len-1].dir_pin = temp; // Set motor's direction pin.
                } else{
                    // Error if pin number is invalid.
                    snprintf(err_str, ERROR_STR_LEN-1, "%s is not a valid value for dir_pin.", value_buff);
                    motor_config_state = ERROR;
                }
            } else{
                // Error if string is invalid.
                snprintf(err_str, ERROR_STR_LEN-1, "%s is not a valid numerical value.", value_buff);
                motor_config_state = ERROR;
            }
            break;
        
        case STEPS_ROT:
            // Validate the string and convert to a number if valid.
            temp = str_to_int(value_buff);
            if(temp > 0){
                motor_list[motor_list_len-1].steps_rot = temp; // Set motor's steps per rotation.
            } else{
                // Error if string is invalid.
                snprintf(err_str, ERROR_STR_LEN-1, "%s is not a valid value for steps_per_roation.", value_buff);
                motor_config_state = ERROR;
            }
            break;
        
        case DIRECTION:
            // Validate the string and convert to a motor direction if valid.
            temp = str_to_dir(value_buff);
            if(temp != DIRECTION_INVALID){
                motor_list[motor_list_len-1].direction = temp; // Set motor's direction.
            } else{
                // Error if string is invalid.
                snprintf(err_str, ERROR_STR_LEN-1, "%s is not a valid direction.", value_buff);
                motor_config_state = ERROR;
            }
            break;
        
        case MICROSTEP:
            // Validate the string and convert to a number if valid.
            temp = str_to_int(value_buff);
            if(temp > 0 && is_valid_microstep(temp)){
                motor_list[motor_list_len-1].microstep = temp; // Set motor's microstep configuration.
            } else{
                // Error if string is invalid.
                snprintf(err_str, ERROR_STR_LEN-1, "%s is not a valid value for microstep.", value_buff);
                motor_config_state = ERROR;
            }
            break;
        
        case AXIS_NAME:
            // Set the name of the axis. String passed as is.
            strncpy(axis_list[axis_list_len-1].name, value_buff, AXIS_NAME_LEN - 1);
            axis_list[axis_list_len-1].name[AXIS_NAME_LEN-1] = 0;
            break;
        
        case MOTOR_LIST:
            /* special case, handled by a specific state routine */
            break;
        
        case MM_ROT:
            // Validate the string and convert to a number if valid.
            temp = str_to_int(value_buff);
            if(temp > 0){
                axis_list[axis_list_len-1].mm_rot = temp; // Set axis' mm per rotation
            } else{
                // Error if string is invalid.
                snprintf(err_str, ERROR_STR_LEN-1, "%s is not a valid numerical value.", value_buff);
                motor_config_state = ERROR;
            }
            break;
        
        case INVALID_PARAM:
            // Invalid parameter was given. Error. 
            strncpy(err_str, "Invalid parameter was set.", ERROR_STR_LEN-1);
            motor_config_state = ERROR;
            break;
    }
}

/**
 * @brief Routine for the READ_MOTOR_LIST state of the state machine.
 * 
 * @param line_buff (in) Buffer containing the current line.
 * @param line_len (in) Size of line_buff.
 * @param value_buff (out) Buffer in which to store the list.
 * @param offset 
 */
static void state_read_motor_list(const char* line_buff, const int line_len, char* value_buff, int* offset)
{
    DEBUG_PRINT("%s", __FUNCTION__);

    int stop = 0;
    int error = 0;
    char c = 0;
    int value_offset = 0;

    // Stop after reading all motor names in the list 
    while(!stop){
        /******* Start of parser *******/
        // Read until next comma or full stop.
        for(value_offset = 0; *offset < line_len; (*offset)++){
            c = line_buff[*offset];
            
            // Motor names may contain numbers, letters, '-' and '_'
            if(isalnum(c) || c == '-' || c == '_'){
                if(value_offset < PARAM_MAX_LEN - 1){
                    value_buff[value_offset++] = c;
                    continue;
                } else{
                    strncpy(err_str, "Value has exceeded max length in " MOTOR_CONFIG_NAME, ERROR_STR_LEN-1);
                    error = 1;
                    break;
                }
            
            // Motor name separator (comma) found, stop parser and continue to setter.
            } else if(c == ','){
                break;
            
            // Found the start of a comment or the end of the line. Move on to setter, and full stop thereafter.
            }else if(isblank(c) || c == '#' || c == '\n'){
                motor_config_state = CLEANUP;
                stop = 1;
                break;
            
            // Invalid character found. Full stop, no setter.
            } else{
                snprintf(err_str, ERROR_STR_LEN-1, "Invalid char (%c [0x%02hhx]) at type identifier in " MOTOR_CONFIG_NAME, c, c);
                error = 1;
                break;
            }
        }
        
        (*offset)++;
        value_buff[value_offset] = 0;
        /******* End of parser *******/

        /******* Start of setter *******/
        // If name was read successfully, search for it in the motor_list
        // and (if found) set it in the list of the current axis entry.
        if(!error){
            if(value_offset > 0){
                struct motor_config* motor_node = get_motor_node_by_name(value_buff);
                if(motor_node != NULL){
                    // Set respective motor handle in the axis' motor_list, and increase its size.
                    struct axis_config* axis_node = &axis_list[axis_list_len-1];
                    axis_node->motors[axis_node->count++] = motor_node;
                
                } else{
                    // Given motor name was not found in the motor_list. Error.
                    snprintf(err_str, ERROR_STR_LEN-1, "Motor %s not found before axis definition in " MOTOR_CONFIG_NAME, value_buff);
                    motor_config_state = ERROR;
                    stop = 1;
                }

            } else{
                // Value string empty, but no previous error? Line abruptly ended. Error. 
                snprintf(err_str, ERROR_STR_LEN-1, "Abrupt end in a motor list in " MOTOR_CONFIG_NAME);
                motor_config_state = ERROR;
                stop = 1;
            }

        } else{
            // Error parsing.
            motor_config_state = ERROR;
            stop = 1;
        }
        /******* End of setter *******/
    }
}

/**
 * @brief State machine for parsing the config file.
 * 
 * @param config_file (in) Handle to the config file. Must have been previously opened.
 * @return (int)  On success, 0. Otherwise, 1, indicating error.
 */
static int motor_config_state_machine(FILE* config_file)
{
    int stop = 0;
    int error = 0;

    // Variables to store line information
    char* line_buff = NULL;
    size_t line_size = 0;
    int offset = 0;

    // Variables to store param information
    char param[PARAM_MAX_LEN];
    int param_size = 0;
    enum params param_id = INVALID_PARAM;

    // Variables to store value information
    char value[VALUE_MAX_LEN];
    int value_size = 0;
    enum identifiers last_id = INVALID_TYPE;

    // The state machine
    motor_config_state = CLEANUP;
    while(!stop){
        switch(motor_config_state){
            case READ_LINE:
                state_read_line(config_file, &line_buff, &line_size);
                break;

            case READ_PARAM:
                state_read_param(line_buff, line_size, param, &param_size, &offset);
                break;

            case READ_IDENTIFIER:
                state_read_identifier(line_buff, line_size, param, &param_size, &offset);
                break;

            case SET_IDENTIFIER:
                state_set_identifier(param, &last_id);
                break;

            case CHECK_PARAM:
                state_check_param(param, last_id, &param_id);
                break;

            case READ_VALUE:
                state_read_value(line_buff, line_size, value, &value_size, &offset);
                break;

            case SET_PARAM:
                state_set_param(param_id, value);
                break;

            case READ_MOTOR_LIST:
                state_read_motor_list(line_buff, line_size, value, &offset);
                break;

            // On a terminating state, at least the line buffer should be free()'d
            // and the stop flag set.
            case FINISHED:
                free(line_buff);
                line_buff = NULL;
                stop = 1;
                break;

            case CLEANUP:
                free(line_buff);
                line_buff = NULL;
                line_size = 0;
                offset = 0;
                memset(param, 0, PARAM_MAX_LEN);
                memset(value, 0, VALUE_MAX_LEN);
                memset(err_str, 0, ERROR_STR_LEN);
                param_size = 0;
                value_size = 0;
                motor_config_state = READ_LINE;
                break;

            case ERROR:
                free(line_buff);
                line_buff = NULL;
                fputs(err_str, stderr);
                fputs("\n", stderr);
                stop = 1;
                error = 1;
                break;
        }
    }

    return error;
}

/**
 * @brief Initialize all the motors and axes defined in the config file.
 * 
 * Should be called after the state machine finishes successfully. 
 * 
 * @return (int) On success, 0. Otherwise, -1. 
 */
static int init_motors(void)
{
    int retval = 0;

    Stepper* motors[MOTOR_LIST_SIZE_MAX];

    //First, initialize all valid motors in the list
    for(int i  = 0; i < motor_list_len && retval == 0; i++){
        struct motor_config* node = &motor_list[i];
        if(validate_motor_node(node)){
            node->motor = stepper_init(node->name, node->step_pin, node->dir_pin, node->microstep, node->steps_rot, node->direction);
            if(node->motor == NULL){
                ERROR_PRINT("Error initializing a motor from " MOTOR_CONFIG_NAME "\n");
                retval = -1;
                goto exit;
            }
        } else{
            ERROR_PRINT("A motor in " MOTOR_CONFIG_NAME " is not fully configured.\n");
            retval = -1;
            goto exit;
        }
    }

    //Then, initialize all valid axes in the list
    for(int i = 0; i < axis_list_len; i++){
        struct axis_config* axis_node = &axis_list[i];
        if(validate_axis_node(axis_node)){
            for(unsigned int j = 0; j < axis_node->count; j++)
                motors[j] = axis_node->motors[j]->motor;
            axis_node->axis = axis_init(motors, axis_node->mm_rot, axis_node->count);
            if(axis_node->axis == NULL){
                ERROR_PRINT("Error initializing an axis from " MOTOR_CONFIG_NAME "\n");
                retval = -1;
                goto exit;
            }
        } else{
            ERROR_PRINT("An axis in " MOTOR_CONFIG_NAME " is not fully configured.\n");
            retval = -1;
            goto exit;
        }
    }

exit:
    return retval;
}

/**************** PUBLIC FUNCTIONS ****************/

/**
 * @brief Read the motor configuration file and initialized all configured objects.
 * 
 * See config.h for a description on the configuration file.
 * 
 * @return (int) On success, 0. Otherwise, -1.  
 */
int read_motor_config(void)
{
    int retval = -1;

    // Open the motor config file
    FILE* config_file = fopen(MOTOR_CONFIG_PATH, "r");
    if(config_file == NULL){
        perror("Error opening " MOTOR_CONFIG_PATH);
        return retval;
    }

    // Initialize lists
    memset(motor_list, 0, sizeof(motor_list));
    memset(axis_list, 0, sizeof(axis_list));

    for(int i = 0; i < MOTOR_LIST_SIZE_MAX; i++) 
        motor_list[i].direction = DIRECTION_INVALID; // 0 is valid direction constant, thus must be changed to DIRECTION_INVALID

    // State machine
    int error = motor_config_state_machine(config_file);
    
    // Initialize motors and axes
    if(!error)  
        retval = init_motors();
    else
        retval = -error; // Error is returned as +1 from motor_config_state_machine().

    fclose(config_file);

    return retval;
}

/**
 * @brief Get the handle to a motor defined in the motors configuration file.
 * 
 * Should only be called after a successful call to read_motor_config.  
 * 
 * @param name (in) Name of the motor, as in the configuration file.
 * @return (Stepper*) On success, handle to the motor. Otherwise, NULL. 
 */
Stepper* get_motor_by_name(const char* name)
{
    Stepper* ret = NULL;

    // Parameter validation
    if(name == NULL){
        DEBUG_PRINT("Name is invalid.");
        goto exit;
    }

    // Lookup and return the first motor that coincides with the name
    struct motor_config* node = get_motor_node_by_name(name);
    if(node != NULL)
        ret = node->motor;
    else{
        DEBUG_PRINT("No motor with the name %s was found.", name);
    }

exit:
    return ret;
}

/**
 * @brief Get the handle to an axis defined in the motors configuration file.
 * 
 * Should only be called after a successful call to read_motor_config.  
 * 
 * @param name (in) Name of the axis, as in the configuration file.
 * @return (Stepper*) On success, handle to the axis. Otherwise, NULL. 
 */
Axis* get_axis_by_name(const char* name)
{
    Axis* ret = NULL;

    //Parameter validation
    if(name == NULL){
        DEBUG_PRINT("Name is invalid.");
        goto exit;
    }
    
    // Lookup and return the first motor that coincides with the name
    struct axis_config* node = get_axis_node_by_name(name);
    if(node != NULL)
        ret = node->axis;
    else{
        DEBUG_PRINT("No axis with the name %s was found.", name);
    }

exit:
    return ret;
}
