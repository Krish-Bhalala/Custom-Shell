#define _GNU_SOURCE                     // For fexecve
#include <sys/mman.h>                   // For memfd_create
#include <unistd.h>                     // For read, write, fork, lseek
#include <sys/wait.h>                   // For waitpid

#include <stdlib.h>
#include <stdio.h>
#include <readline/readline.h>          //for the bonus part: reading the command line
#include <readline/history.h>           //for the bonus part: navigating the command line history

#include "nqp_io.h"                     //file system module
#include "nqp_shell.h"

#include <assert.h>
#include <stdint.h>
#include <ctype.h>

//ASSERTION HELPER CONSTANTS
#define MAX_ARGS 164                    
#define MAX_ARG_LENGTH 256              

//IO CONSTANTS
#define READ_BUFFER_SIZE 4096               //1024 * 4 bytes to read from the nqp_file system
#define PIPE_READ_END 0                     //index of pipe's read end
#define PIPE_WRITE_END 1                    //index of pipe's read end

//RETURN CODES
#define COMMAND_NOT_FOUND -404
#define COMMAND_EXECUTION_FAILED -403
#define INVALID_ARGUMENTS -300
#define INSUFFICIENT_ARGUMENTS -301
#define REDIRECTION_FAILED -400
#define INVALID_USECASE -204
#define OPERATION_FAILED -500
#define OPERATION_SUCCEED 500

//LOGGING RELATED GLOBALS
#define LOG_DISABLED -1                     //flag indicating log is disabeled
#include <fcntl.h>                          //for open()
int log_fd = LOG_DISABLED;                  //stores the fd for the log file


//----------------------------------
//CURRENT DIRECTORY OBJECT ROUTINES
//----------------------------------
// Creates a new directory object initialized to the root directory.
Curr_Dir* construct_empty_curr_dir(void){
    // Allocate memory for struct.
    Curr_Dir* cwd = (Curr_Dir*)malloc(sizeof(Curr_Dir));
    assert(NULL != cwd);
    if(NULL == cwd) return NULL;        // Return null if allocation failed.

    // Set the path to the root directory.
    strncpy(cwd->path, "/", MAX_LINE_SIZE);  
    assert(is_valid_path(cwd->path));   // Verify the path is valid.

    // Validate the directory object.
    assert(is_valid_curr_dir(cwd));
    if(!is_valid_curr_dir(cwd)){        //invalid curr_dir, do clean up
        destroy_curr_dir(cwd);  
        return NULL;  
    }

    return cwd;  
}

// Constructs a directory object from a given path.
Curr_Dir* construct_curr_dir(const char* path){
    // Validate the input path.
    assert(is_valid_path(path));  
    if(!is_valid_path(path)) return NULL;       // Return null for invalid paths.

    // Allocate memory for struct
    Curr_Dir* cwd = malloc(sizeof(Curr_Dir));
    if(NULL == cwd) return NULL;                // malloc failure, so return null  
    strncpy(cwd->path, path, MAX_LINE_SIZE);    // Copy the path into struct.

    // Validate the directory object.
    assert(is_valid_curr_dir(cwd));  
    if(!is_valid_curr_dir(cwd)){    //invalid struct ,so free resources.
        destroy_curr_dir(cwd);  
        return NULL;  
    }

    return cwd;  
}

// Frees the memory allocated for a directory object (frre the struct).
void destroy_curr_dir(Curr_Dir* cwd){
    if(NULL == cwd) return;
    free(cwd);
}

// Updates the path stored in a <cwd>.
void set_path(Curr_Dir* cwd,const char* path){
    //Validation checks
    assert(NULL != cwd);       
    assert(NULL != path);
    assert(is_valid_path(path));
    if(NULL == cwd || NULL == path) return; 

    //Update the path
    if(is_valid_curr_dir(cwd)){
        strncpy(cwd->path, path, MAX_LINE_SIZE);
    }
}


//----------
//VALIDATORS
//----------
//String Validator
bool is_valid_string(const char* str){
    if(NULL == str) return false;
    if(strlen(str) < 1) return false;
    if(strlen(str) > MAX_LINE_SIZE) return false;
    return true;
}
//EMPTY String validator
bool is_only_whitespace(const char *str) {
    while (*str) {
        if (!isspace((unsigned char)*str)) {
            return false;
        }
        str++;
    }
    return true;
}
//File path validator
bool is_valid_path(const char* path){
    if(NULL == path) return false;
    if(strlen(path) < 1) return false;              //must have 1 char for root "/"
    if(strlen(path) > MAX_LINE_SIZE) return false;
    if('/' != path[0]) return false;                //must start with root "/"

    //check for back to back '/' chars. //,///,///..., etc {NOT ALLOWED}
    int slash_count = 0;
    for (int i = 0; path[i] != '\0'; i++) {
        if (path[i] == '/') {
            slash_count++;
            if (slash_count >= 2) {
                return false;
            }
        } else {
            slash_count = 0;
        }
    }
    return true;
}
//Current working directory validaotor
bool is_valid_curr_dir(const Curr_Dir *cwd){
    if(NULL == cwd) return false;
    if(!is_valid_path(cwd->path)) return false;     //check if the path is valid
    return true;
}
//Prints NULL terminated strings
void print_string_array(const char *arr[]) {
    for (int i = 0; arr[i] != NULL; i++) {
        printf("%s\n", arr[i]);
        if(NULL == arr[i+1]){
            printf("NULL\n");
        }
        fflush(stdout);
    }
    
}

//--------
//HELPERS
//--------
//Trims leading and trailing spaces
void trim_string(char *str) {
    if(!str) return;
    if(strlen(str) < 1) return;
    int start = 0, end = strlen(str) - 1, i;
    assert(start <= end);
    assert(start >= 0);
    assert(end >= 0);
    // Trim leading whitespace
    while (str[start] == ' ' || str[start] == '\t' || str[start] == '\n') start++;

    // Trim trailing whitespace
    while (end > start && (str[end] == ' ' || str[end] == '\t' || str[end] == '\n')) end--;

    // Shift characters to the beginning of the string
    for (i = 0; i <= end - start; i++) str[i] = str[start + i];

    // Null terminate the string
    str[i] = '\0';
    assert(is_valid_string(str));
}


//---------
//BUILTINS
//---------
//Print Working Directory: prints absolute path of cwd
void command_pwd(const Curr_Dir *cwd) {
    assert(is_valid_curr_dir(cwd));
    if(!is_valid_curr_dir(cwd)) return;         //validation
    char buffer[MAX_LINE_SIZE]; 
    snprintf(buffer, sizeof(buffer), "%s\n", cwd->path);    
    custom_print(buffer);//printing
}

//List: list the content of the files
void command_ls(const Curr_Dir* cwd){
    //cwd validation
    assert(NULL != cwd);
    assert(is_valid_curr_dir(cwd));
    if(!is_valid_curr_dir(cwd)) return;

    nqp_dirent entry = {0};     
    int fd = -1;              
    ssize_t dirents_read;

    //copying the function parameters
    char* curr_path = strdup(cwd->path);
    assert(is_valid_path(curr_path));
    if(!is_valid_path(curr_path)){
        printf("ls: corrupted working directory path- %s != %s", curr_path, cwd->path);
        return;
    }

    //open the file in the mounted file system
    fd = nqp_open(curr_path);
    if ( fd == NQP_FILE_NOT_FOUND ){
        fprintf(stderr, "%s not found\n", curr_path );
        free(curr_path);
        return; //file open failed
    }

    //read the files directory entries
    while ( ( dirents_read = nqp_getdents( fd, &entry, 1 ) ) > 0 ){
        char buffer[MAX_LINE_SIZE];  // Adjust the size as needed
        snprintf(buffer, sizeof(buffer), "%lu %s", entry.inode_number, entry.name); 
        custom_print(buffer);//print its metadata
        if ( entry.type == DT_DIR ){                        //append "/", if its a directory
            custom_print("/");
        }
        putchar('\n');
        free( entry.name );
    }

    //if not a directory then throw error
    if ( dirents_read == -1 ){
        fprintf( stderr, "%s is not a directory\n", curr_path );
    }

    //clean up resources
    nqp_close( fd );
    free(curr_path);
}

/*
 * Change Directory: changes the current working directory to <path> folder inside the cwd
 * "cd .." OR "cd ../" OR "cd ..<anything>" Takes to parent dir
 * "cd /" OR "cd /<anything>" OR "cd" Takes to root dir
 * "cd <path>" takes to that folder in the current directory
 * Other operations are not permitted
 */
void command_cd(const char* path, Curr_Dir* cwd){
    //Input validation
    assert(is_valid_string(path));
    if(!is_valid_string(path)) {
        printf("Error: Invalid path string\n");
        return;
    }
    if(strlen(path) == 0 || path[0] == ' '){ //if path is NOT starting with a non empty character then change to root dir
        strncpy(cwd->path, "/", MAX_LINE_SIZE);
        return;
    }
    if(!is_valid_string(path)) {
        printf("Error: Invalid path string\n");
        return;
    }

    //copying the parameters
    char* curr_path = cwd->path;

    if(strncmp(path,"..",2) == 0){          // "cd .." request
        //CASE 1: Already in root dir, can't go up
        if (strcmp(cwd->path, "/") == 0) {
            //printf("Already at root directory, cannot go up\n");
            return;
        }

        //CASE 2: Parent directory exist, so change the path to it by removing the last word
        for(int i=strlen(curr_path)-1; i>=0; i--){
            if('/' == curr_path[i]){
                if(strcmp(curr_path, "/") != 0){
                    curr_path[i] = '\0';
                }
                break;
            }
            curr_path[i] = '\0';
        }
    }else if(strcmp(path,"/") == 0){        //"cd /" or "cd /something" request
        assert(strlen(path) == 1);
        strncpy(cwd->path,"/",MAX_LINE_SIZE);
    }else{ //change to another directory

        //create new absolute path fot the <path>
        char new_path[MAX_LINE_SIZE] = {0};
        if(curr_path[strlen(curr_path) - 1] != '/'){
            snprintf(new_path, MAX_LINE_SIZE, "%s/%s", curr_path, path);
        }else{
            snprintf(new_path, MAX_LINE_SIZE, "%s%s", curr_path, path);
        }
        assert(is_valid_path(new_path));

        if(!is_valid_path(new_path)) {      //validate the path
            printf("Error: Invalid path %s\n", new_path);
            return;
        }

        //check if the new folder/file exist in mounted file system
        int fd = NQP_FILE_NOT_FOUND;
        fd = nqp_open(new_path);
        if (fd < 0) {   
            printf("ERROR: Directory not found: %s\n", new_path);
            return;
        }

        //check if the found entry is a directory entry
        ssize_t dirents_read;
        nqp_dirent entry = {0};
        dirents_read = nqp_getdents(fd,&entry,1);
        nqp_close(fd);      //free the resources in mounted files system
    
        if (dirents_read < 0) { //dir not found
            printf("ERROR: Is not a directory: %s\n", new_path);
            return;
        }
        
        // Update current working directory, if its a directory entry
        assert(is_valid_path(new_path));
        strcpy(curr_path, new_path);
        assert(is_valid_path(curr_path));
        assert(strncmp(cwd->path, new_path, MAX_LINE_SIZE) <= 0);
    }
    assert(is_valid_curr_dir(cwd));
}


//------------------------
//COMMAND OBJECT ROUTINES
//------------------------
//Constructor: parses the input, split from spaces, create a command object with NULL terminated argument array
Command* command_create(const char* input) {
    //input validation
    assert(input != NULL);
    if (!input) return NULL;

    //allocate the memory
    Command* cmd = (Command*)malloc(sizeof(Command));
    if (!cmd) return NULL;
    
    //initialise the instance vars
    cmd->argc = 0;
    cmd->argv = (char**)malloc(sizeof(char*) * MAX_ARGS);
    if (!cmd->argv) {
        free(cmd);
        return NULL;
    }

    //create copy of the input param for working with it
    char* input_copy = strdup(input);
    if (!input_copy) {
        free(cmd->argv);
        free(cmd);
        return NULL;
    }

    //tokenise the input by empty spaces
    char* token = strtok(input_copy, " \t\n");
    while (token && cmd->argc < MAX_ARGS) {
        cmd->argv[cmd->argc] = strdup(token);       //store the token in the args vector
        if (!cmd->argv[cmd->argc]) {                //if any argument fails
            for (int i = 0; i < cmd->argc; i++) free(cmd->argv[i]);
            free(cmd->argv);
            free(cmd);
            free(input_copy);
            return NULL;
        }
        cmd->argc++;                                //add next argument
        token = strtok(NULL, " \t\n");
    }

    //terminate the args vector by NULL 
    if(NULL != cmd->argv[cmd->argc]){
        printf("%s",input);
        cmd->argv[cmd->argc] = NULL;
    }

    free(input_copy);   //free the input param copy
    return cmd;
}

//Destructor for command object
void command_destroy(Command* cmd) {
    if (!cmd) return;
    for (int i = 0; i < cmd->argc; i++) {
        free(cmd->argv[i]);
    }
    free(cmd->argv);
    free(cmd);
}

//Validator for command object
bool command_is_valid(const Command* cmd) {
    return (cmd && cmd->argv && cmd->argc > 0 && cmd->argc <= MAX_ARGS);
}

//Getter: returns the ith argument of the command
const char* command_get_arg(const Command* cmd, int index) {
    // assert(cmd != NULL);
    // assert(index >= 0);
    // assert(index < cmd->argc);
    if (!cmd || index < 0 || index >= cmd->argc) return NULL;
    return cmd->argv[index];
}

//Printer: for debugging command object, prints the argc and argv
void command_print(const Command* cmd) {
    assert(cmd != NULL);
    if (!cmd) return;
    printf("argc = %d\n", cmd->argc);   //print argc
    printf("argv = [");                 //print argv
    for (int i = 0; i < cmd->argc; i++) {
        printf("\"%s\"", cmd->argv[i]);
        if (i < cmd->argc - 1) printf(", ");
    }
    printf("]\n");
}

// Executor: parses the command object and executes respective commands
bool execute_command(const Command* cmd, Curr_Dir* cwd, char *envp[]) {
    // Make sure the command is valid and has a non-negative argument count
    assert(NULL != cmd);
    assert(cmd->argc >= 0);
    if (!cmd || cmd->argc < 0) return false; // Invalid command
    if (cmd->argc == 0) return true; // No arguments, ask user for the next command
    assert(cmd->argc > 0);

    // Read the first argument (the command name)
    const char* argv_0 = command_get_arg(cmd, 0);
    if (!argv_0) return false; // Command name is missing
    char* command = strdup(argv_0); // Copy the command name into a new string
    assert(command != NULL);
    if (!command) return false; // Memory allocation failed

    // Check if the command matches built-in commands
    if (strcmp(command, "cd") == 0) { // Handle "cd" (change directory)
        const char* argv_1 = command_get_arg(cmd, 1); // Get the target directory
        if (argv_1) {
            char* destination = strdup(argv_1); // Copy the directory name
            assert(NULL != destination);
            command_cd(destination, cwd); // Change to the target directory
            free(destination); // Free memory for the directory name
            return true;
        }
    } else if (strcmp(command, "ls") == 0) { // Handle "ls" (list directory contents)
        command_ls(cwd);
    } else if (strcmp(command, "pwd") == 0) { // Handle "pwd" (print current directory)
        command_pwd(cwd);
    } else { // Not a built-in command, execute it as an external command
        int return_code = -1;
        if ((return_code = import_command_data(cmd, cwd->path, envp)) < 0) {
            // Print error messages
            if (return_code == COMMAND_EXECUTION_FAILED) 
                fprintf(stderr, "Failure executing command: %s\n", argv_0);
            else if (return_code == COMMAND_NOT_FOUND) 
                fprintf(stderr, "execute_command: Command not found in mounted disk: %s\n", argv_0);
            else 
                fprintf(stderr, "Command execution failed with error code {%d} for command: %s\n", return_code, argv_0);
        }
    }

    free(command); // Free memory for the copied command name
    return true;   // Command executed successfully or handled appropriately
}

// Creates a custom NULL-terminated list of arguments for redirection
char** create_arguments_for_redirection(const Command* cmd) {
    // input param validation
    assert(command_is_valid(cmd));
    if (!command_is_valid(cmd)) {
        perror("create_arguments_for_redirection: INVALID ARGUMENTS"); // Print error if invalid
        return NULL; // Return NULL for invalid commands
    }
    
    // Count the number of arguments before "<" (redirection symbol)
    int count = 0;
    int i = 0;
    while (i < cmd->argc && strcmp(command_get_arg(cmd, i), "<") != 0) {
        count++;
        i++;
    }
    assert(strcmp(command_get_arg(cmd, count - 1), "<") != 0); // Ensure last argument before "<" is valid

    // Allocate memory for the new argument list (including NULL terminator)
    char** filtered_args = malloc((count + 1) * sizeof(char *));
    assert(NULL != filtered_args);
    if (!filtered_args) {
        perror("create_arguments_for_redirection: malloc fail"); // Print error if memory allocation fails
        return NULL; // Return NULL on failure
    }

    // Copy arguments before "<" into the new list
    for (int k = 0; k < count; k++) {
        filtered_args[k] = strdup(command_get_arg(cmd, k)); // Duplicate each argument
        if (NULL == filtered_args[k]) { 
            perror("create_arguments_for_redirection: strdup fail"); // Print error if duplication fails
            return NULL; // Return NULL on failure
        }
    }
    assert(strcmp(filtered_args[count - 1], "<") != 0); // Ensure last copied argument is not "<"

    // Add the NULL terminator at the end of the argument list
    filtered_args[count] = NULL;

    assert(strcmp(filtered_args[count - 1], "<") != 0); // Double-check last argument is valid
    assert(filtered_args[count] == NULL); // Ensure NULL terminator is correctly added

    return filtered_args;
}


//------------------------
//PROCESS RELATED ROUTINES
//------------------------
/*
 * import_command_data(): finds the command file in nqp fs, copy it to local memory and executes it. 
 * RETURN CODES:    COMMAND_NOT_FOUND (for any error in opening or create a local copy of that file)
 *                  COMMAND_EXECUTION_FAILED (for any failure in running pipes or fexecve or forking)
 *                  status (for successful completion of the child process)
*/
int import_command_data(const Command* cmd, const char* curr_path, char *envp[]){
    //input params validation and copying
    const char* argv_0 = command_get_arg(cmd,0);
    char* command = strdup(argv_0);
    char* path = strdup(curr_path);
    assert(is_valid_path(path));
    assert(is_valid_string(command));
    if(!is_valid_path(path) || !is_valid_string(command)){
        free(command);
        return COMMAND_NOT_FOUND;
    }
    
    //search the command data file in current working directory
    //first change the command path to start from the root directory
    if(path[strlen(path)-1] != '/'){
        strcat(path,"/");       //make sure the working directory path ends with "/"
    }
    strcat(path,command);       //concatenate the command to that path
    assert(is_valid_path(path));
    
    //open the file in nqp file system
    int nqp_fd = nqp_open(path);
    if(nqp_fd == NQP_FILE_NOT_FOUND){
        free(command);
        return COMMAND_NOT_FOUND;
    }
    assert(nqp_fd >= 0);
    free(path);                 //path is no longer needed so free it

    //creat a new file in local memory
    int mem_fd = memfd_create("FileSystemCode", 0); 
    if(mem_fd == NQP_FILE_NOT_FOUND) {  //memfd create fails, do cleanup
        nqp_close(nqp_fd);
        free(command);
        return COMMAND_NOT_FOUND;
    }
    assert(mem_fd >= 0);

    //read the file's data from the nqp file system and write in the local memory file (i.e. mem_fd file)
    char buffer[READ_BUFFER_SIZE] = {0};
    int bytes_read = 0;
    while( (bytes_read = nqp_read(nqp_fd, &buffer, READ_BUFFER_SIZE)) > 0){
        write(mem_fd, buffer, bytes_read);
        memset(buffer, 0, READ_BUFFER_SIZE);
    }

    //close the file in nqp file system and reset the offset marker in mem_fd file
    nqp_close(nqp_fd);  
    lseek(mem_fd, 0, SEEK_SET);

    //handle the output redirection through a pipe if logging is enable
    int pipefd[2] = {-1, -1};
    if (log_fd != LOG_DISABLED) {   // Create pipe for printing command output
        if (pipe(pipefd) < 0) {     // populate the pipe's fd
            perror("pipe");         // piping failed, do cleanup
            free(command);
            nqp_close(nqp_fd);
            return COMMAND_EXECUTION_FAILED;
        }
    }

    //fork the process for exec-ing the command
    pid_t pid = fork();
    if(0 == pid){   //child process
        char** arguments = cmd->argv;       //pointer to track the new arguments

        //handle the input redirection
        int input_fd = -1;
        input_fd = handle_input_redirection(cmd,curr_path);     //copys the mounted fs's file into local memory and open it
        if (input_fd > 0) { //if input_fd is different than STDIN_FILENO then redirection is enabled
            //redirection operator is there, change the file descriptor of stdin with input_fd
            dup2(input_fd, STDIN_FILENO);
            close(input_fd);

            //remove the arguments after "<" since redirection is enabled and we already piped it with our process's stdin
            char **minimal_args = create_arguments_for_redirection(cmd);
            arguments = minimal_args;
            assert(minimal_args != NULL);
        }
        if(REDIRECTION_FAILED == input_fd){ //there was a redirection operator in args and still operation failed
            printf("Redirection Failed pls try again\n");   //kill the process 
            exit(EXIT_FAILURE);
        }       
        if(INVALID_ARGUMENTS == input_fd){ //invalid args to handle_input_redirection()
            printf("Invalid Arguments passed to redirection command");  //print error but do not kill this process
        }

        // Handle output redirection if logging is enabled
        if (log_fd != LOG_DISABLED) {
            close(pipefd[PIPE_READ_END]);                   // Close read end
            dup2(pipefd[PIPE_WRITE_END], STDOUT_FILENO);    // Copy the pipe read end to stdoutfileno
            close(pipefd[PIPE_WRITE_END]);                  // Free the orig copy of the file entry of logfile in descriptor table
        }

        // Execute the program with modified args
        fexecve(mem_fd, arguments, envp);
        printf("import_command_data::fexecve FAILED\n");
        exit(EXIT_FAILURE);
    } else if (pid > 0) {   // Parent process
        assert(pid != 0 && "fexecve in child failed");   //trigger means fexecve failed. Currently in child process

        // If logging is enabled, read child's output from pipe and write to both stdout and log
        if (log_fd != LOG_DISABLED) {
            close(pipefd[PIPE_WRITE_END]);  // Close write end of the output redirection pipe
            
            //read the output buffers data and write it to the output buffers
            char buffer[READ_BUFFER_SIZE];
            ssize_t n;
            while ((n = read(pipefd[0], buffer, READ_BUFFER_SIZE)) > 0) {
                write(STDOUT_FILENO, buffer, n);    // Write to stdout
                write(log_fd, buffer, n);           // Write to log file
            }
            
            close(pipefd[PIPE_READ_END]);   //close the pipes read end
        }

        // Parent process
        int status;                 //status code for whether waitpid was executed or not
        waitpid(pid, &status, 0);   //wait for child to finish
        
        free(command);              //clean up resources
        return status;
    }

    return COMMAND_EXECUTION_FAILED;        //fork failed, hence return failure
}

/*
 * handle_input_redirection(): find the redirected input file in nqp fs and create and return the local copy's fd of that input file
 * RETURN CODES:    INVALID_ARGUMENTS (if any param is null or invalid)
 *                  REDIRECTION_FAILED (for any error with command format, file creation, malloc, etc.)
 *                  mem_fd (the file descriptor of the newly created local memory file) (SUCCESS CODE)
 *                  STD_FILENO (fallback, if redirection operator isnt present in the cmd args) (SUCCESS CODE)
*/
int handle_input_redirection(const Command* cmd, const char* cwd_path){
    //params validation
    assert(command_is_valid(cmd));
    assert(cwd_path != NULL);
    assert(is_valid_path(cwd_path));
    if(NULL == cmd || NULL == cwd_path) return INVALID_ARGUMENTS;
    if(!command_is_valid(cmd) || !is_valid_path(cwd_path)) return INVALID_ARGUMENTS;

    //loop through the command arguments
    for(int i=0; i<cmd->argc; i++){
        //search for "<" in the arguments
        if(strcmp(command_get_arg(cmd,i),"<") == 0){    //detected redirection operator
            //check if redirection is possible or not, if the command contains "<" operator
            if(cmd->argc < 3){
                printf("ERROR: Not enough arguments for redirection, Minimum 3 arguments required\n");
                return REDIRECTION_FAILED;
            }
            if(i+1 >= cmd->argc){    //making sure the redirection operator isn't the last argument
                printf("ERROR: Last argument should be a filename not the redirection operator\n");
                return REDIRECTION_FAILED;
            }

            //copy the input params for creating the absolute path
            const char* arg_i_plus_one = command_get_arg(cmd,i+1);
            assert(arg_i_plus_one != NULL);
            char* filename = strdup(arg_i_plus_one);    //stores the filename for creating filepath
            assert(NULL != filename);
            char* filepath = strdup(cwd_path);          //stores the absolute path in nqp file system
            assert(NULL != filepath);
            if(NULL == filename || NULL == filepath){
                printf("ERROR: error in finding file from the arguments of the command ");
                command_print(cmd);
                free(filename);
                free(filepath);
                return REDIRECTION_FAILED;
            }
            //create the absolute path of the filename for the nqp file system
            if(filepath[strlen(filepath)-1] != '/'){
                strcat(filepath,"/");   //make sure cwd_path ends with "/"
            }
            strcat(filepath,filename);  //making filepath absolute
            assert(is_valid_path(filepath));

            //open that file in nqp file system
            int input_fd = nqp_open(filepath);
            if(input_fd < 0){   //file creation failed
                printf("Redirection Error: nqp_open input file {%s} not found\n", command_get_arg(cmd,i+1));
                return REDIRECTION_FAILED;
            }
            assert(input_fd >= 0);

            //create a memory file to store the input file
            int mem_fd = memfd_create(filepath, 0);
            if (mem_fd < 0) {
                printf("ERROR: memfd_create failed creating input_redirect");
                nqp_close(input_fd);    //free resources
                return REDIRECTION_FAILED;
            }
            assert(mem_fd >= 0);

            //read the input file and write its content in the memory file
            char buffer[READ_BUFFER_SIZE] = {0};
            ssize_t bytes_read = 0;
            while ((bytes_read = nqp_read(input_fd, buffer, sizeof(buffer))) > 0) {
                write(mem_fd, &buffer, bytes_read);     //write it in the memory file
                memset(buffer, 0, READ_BUFFER_SIZE);    //reset the buffer
            }
            nqp_close(input_fd);            //close the input file in the nqp file system
            lseek(mem_fd, 0, SEEK_SET);     //reset read offset of the memory input file

            return mem_fd;  //return the fd of local memory input file
        }
    }
    //if there is no redirection needed, then return the standard input file num as fall back
    return STDIN_FILENO;
}


//-------------------------
//LOGGING RELATED ROUTINES
//-------------------------
//custom_print(): a wrapper of printf function to write to both stdout and log file
//NOTE: only used as a fallback to non redirected outputs. A basic safety measure to avoid redundant loging
void custom_print(const char *message) {
    //input validation
    assert(message != NULL);
    if (!message) return;
    
    // Write to stdout
    printf("%s",message);
    fflush(stdout);
    
    // Write to log file if output redirection is ON
    if (log_fd != LOG_DISABLED) {
        write(log_fd, message, strlen(message));
    }
}

//free_logs(): closes the log file
void free_logs(){
    if (log_fd != LOG_DISABLED) {
        close(log_fd);
    }
}


//--------------------------------------
//PIPE COMMANDS OBJECT RELATED ROUTINES
//--------------------------------------
//Printer: prints teh entire pipe_commands object for debugging
void print_pipe_commands(const Pipe_Commands* pipe_cmds) {
    //params validation
    if (pipe_cmds == NULL) {
        printf("Pipe_Commands is NULL\n");
        return;
    }
    //printing the count of commands
    printf("Number of commands in pipe: %d\n", pipe_cmds->num_commands);
    //printing the contained commands objects
    for (int i = 0; i < pipe_cmds->num_commands; i++) {
        printf("Command %d:\n", i + 1);
        if (pipe_cmds->commands[i] != NULL) {
            command_print(pipe_cmds->commands[i]);  //passing mssg to command object to print itself
        } else {
            printf("  (NULL command)\n");           //print the NULL commands as well
        }
        
        // Print a separator between commands, except for the last one
        if (i < pipe_cmds->num_commands - 1) {
            printf("  |\n");  // Pipe symbol separate commands
        }
    }
}

//Constructor: parses the line, checks if pipe operator is used properyl, create the command objects with NULL terminated args, and returns the broken down smaller comand
Pipe_Commands* create_Pipe_Commands(const int num_pipes, char* line){
    //params validation
    assert(num_pipes > 0);
    assert(is_valid_string(line));
    if(num_pipes <= 0){
        printf("create_Pipe_Commands: INVALID number of pipes {%d}", num_pipes);
        return NULL;
    }
    if(!is_valid_string(line)){
        printf("create_Pipe_Commands: INVALID input command string {%s}", line);
        return NULL;
    }

    //Allocate memory for the Pipe_Commands object
    Pipe_Commands* pipe_commands_obj = malloc(sizeof(Pipe_Commands));
    assert(NULL != pipe_commands_obj);
    if (!pipe_commands_obj) {
        perror("create_Pipe_Commands:: malloc fail - pipe_commands_obj");
        return NULL;
    }

    // Allocate memory for the array of Command pointers
    pipe_commands_obj->commands = malloc((num_pipes+1) * sizeof(Command*));
    Command** cmd_array = pipe_commands_obj->commands;
    if (!cmd_array) {
        perror("create_Pipe_Commands:: malloc fail - commands array");
        pipe_commands_destroy(pipe_commands_obj);
        return NULL;
    }
    //update the total num of commands
    pipe_commands_obj->num_commands = num_pipes+1;
    assert(pipe_commands_obj->num_commands == num_pipes+1);

    // Split the line by pipe operators and create a command object for each sub commands
    char* token = NULL;
    char cmd_str[MAX_LINE_SIZE];
    int i = 0;
    int count = 0;          //counter to verify if the num_commands field matched with actual number of commands read
    while ((token = strsep(&line, "|")) != NULL && i < num_pipes+1) {
        count++;            //update the number of commands
        assert(num_pipes+1 > i);

        //create a copy of command name for creating the command object
        memset(cmd_str,0,MAX_LINE_SIZE);
        strncpy(cmd_str,token,MAX_LINE_SIZE);

        //make the string nice and validate it
        trim_string(cmd_str);
        assert(is_valid_string(cmd_str));

        // Create a Command object with the token
        cmd_array[i] = command_create(cmd_str);
        if (!cmd_array[i]) {    //something went wrong with the cmd_str 
            fprintf(stderr, "Failed to create Command object for: %s\n", cmd_str);
            // Clean up everything
            pipe_commands_destroy(pipe_commands_obj);
            return NULL;
        }
        if(0 == cmd_array[i]->argc){    //invalid command arguments given in line, must be atleast one arguments init
            fprintf(stderr, "INVALID Command within the pipe\n");
            pipe_commands_destroy(pipe_commands_obj);
            return NULL;
        }

        i++;
    }

    assert(count == pipe_commands_obj->num_commands);
    return pipe_commands_obj;
}

//Destructor: frees up the memory used by the pipe_commands object
void pipe_commands_destroy(Pipe_Commands* pipe) {
    if (pipe == NULL) return;

    // Free individual Command objects
    if (pipe->commands != NULL) {
        for (int i = 0; i < pipe->num_commands; i++) {
            if (pipe->commands[i] != NULL) {
                command_destroy(pipe->commands[i]);     //ask command to destroy itself
            }   
        }
        // Free the array of Command pointers
        free(pipe->commands);
    }
    // Free the Pipe_Commands struct itself
    free(pipe);
}

//Validator: verify the pipe_commands object contains valid num of proper commands
bool pipe_commands_is_valid(const Pipe_Commands* pc) {
    // Check if the struct itself exists
    if (pc == NULL) return false;
    
    // Validate number of commands
    if (pc->num_commands < 1) return false;
    
    // Validate commands array
    if (pc->commands == NULL) return false;
    
    // Validate individual commands
    for (int i = 0; i < pc->num_commands; i++) {
        if (pc->commands[i] == NULL || !command_is_valid(pc->commands[i])) {
            return false;
        }
    }
    // All checks passed
    return true;
}

//Getter: get the ith command of the pipe_command object
Command* pipe_commands_get_command_at(const Pipe_Commands* cmd_list, const int index){
    if (!cmd_list || index < 0 || index >= cmd_list->num_commands) return NULL;
    return cmd_list->commands[index];
}

/*
 * Generic Executor
 * execute_pipes(): runs all the command in the cmd_list with given <envp> and redirects output to given output file
 * NOTE: Only runs the commands present in the current working directory (cwd)
 * RETURN CODE: INVALID_ARGUMENTS (any argument is NULL or invalid structs)
 *              OPERATION_FAILED (pipes setup failed OR malloc fail OR fexecve fail OR fork fail)
 *              OPERATION_SUCCED (all the command in cmd_list worked noice)
 *              REDIRECTION_FAILED (when input redirection file not found OR pipe() failed)
 *              EXIT_FAILURE (when something goes wrong in the child process)
 */
int execute_pipes(Pipe_Commands* cmd_list, const Curr_Dir* cwd, char *envp[], const int output_fd) {
    //params validation
    assert(cmd_list != NULL);
    assert(cwd != NULL);
    assert(output_fd >= 0);
    
    //validate the structs
    if (!pipe_commands_is_valid(cmd_list)) {
        fprintf(stderr, "Invalid pipe commands\n");
        return INVALID_ARGUMENTS;
    }
    
    //get the total num of commands ot process
    int num_commands = cmd_list->num_commands;
    assert(num_commands > 0);
    
    // Safety check, if only one command, just execute it directly. NOTE: this should not trigger as the logic is taken care in the main shell loop
    if (num_commands == 1) {
        Command* cmd = pipe_commands_get_command_at(cmd_list, 0);       //create command
        if (!execute_command(cmd, (Curr_Dir*)cwd, envp)) {              //ask itself to execute itself
            return OPERATION_FAILED;
        }
        return OPERATION_SUCCEED;
    }
    
    // Create pipes for all commands except the last one, as last one is for the output redirection if enabled
    int pipes[num_commands - 1][2];
    for (int i = 0; i < num_commands - 1; i++) {
        if (pipe(pipes[i]) == -1) {         //populate the pipes
            perror("Error creating pipe");
            
            // Close all pipes incase there is an error in initialising  the pipes
            for (int j = 0; j < i; j++) {
                close(pipes[j][PIPE_READ_END]);
                close(pipes[j][PIPE_WRITE_END]);
            }
            return REDIRECTION_FAILED;  //return failure in redirection
        }
    }
    
    // Store all child process IDs
    pid_t child_pids[num_commands];
    
    // setup the pipe for each command and execute each command in the cmd_list
    for (int i = 0; i < num_commands; i++) {
        //get the command obejct
        Command* current_cmd = pipe_commands_get_command_at(cmd_list, i);
        assert(current_cmd != NULL);
        
        pid_t pid = fork();     //fork to create a dedicated process for ith command
        
        if (pid == -1) {        //fork fail, so clean up resources and terminate
            perror("Fork failed");
            
            // Close all pipes
            for (int j = 0; j < num_commands - 1; j++) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }
            
            return OPERATION_FAILED;
        }
        
        if (pid == 0) {  // Child process: Running the ith command

            // Set up input redirection for first command if "<" operator is present
            if (i == 0) {
                // search for input redirection symbol "<", make sure its not the last command
                for (int j = 0; j < current_cmd->argc; j++) {
                    if (strcmp(current_cmd->argv[j], "<") == 0 && j + 1 < current_cmd->argc) { //Found redirection symbol

                        //Get the input filename
                        const char* filename = current_cmd->argv[j + 1];

                        //create absolute path of the input file
                        char inputfile_absolute_path[MAX_LINE_SIZE] = {0};
                        if(cwd->path[strlen(cwd->path) - 1] != '/'){
                            snprintf(inputfile_absolute_path, MAX_LINE_SIZE, "%s/%s", cwd->path, filename);
                        }else{
                            snprintf(inputfile_absolute_path, MAX_LINE_SIZE, "%s%s", cwd->path, filename);
                        }
                        assert(is_valid_path(inputfile_absolute_path)); //validate it 

                        // Open that input file from nqp fs
                        int fd = nqp_open(inputfile_absolute_path);
                        if (fd == NQP_FILE_NOT_FOUND) {
                            fprintf(stderr, "Failed to open file %s for redirection at path {%s}\n", filename, inputfile_absolute_path);
                            exit(EXIT_FAILURE);
                        }
                        
                        // Create local copy of that input file in our local memory
                        int memfd = memfd_create("input_redirect", 0);
                        if (memfd == -1) {
                            perror("memfd_create failed");
                            exit(EXIT_FAILURE);
                        }
                        
                        // Read file content from nqp fs into the local memory file
                        char buffer[READ_BUFFER_SIZE];
                        size_t bytes_read;
                        while ((bytes_read = nqp_read(fd, buffer, READ_BUFFER_SIZE)) > 0) {
                            if (write(memfd, buffer, bytes_read) == -1) {
                                perror("write to memfd failed");
                                exit(EXIT_FAILURE);
                            }
                        }
                        
                        // Close the input file in the nqp fs
                        nqp_close(fd);
                        
                        // Reset the local copy of the input file's offset to 0
                        if (lseek(memfd, 0, SEEK_SET) == -1) {
                            perror("lseek failed");
                            exit(EXIT_FAILURE);
                        }
                        
                        // Redirect stdin with our local memory fd of the input file's copy
                        if (dup2(memfd, STDIN_FILENO) == -1) {
                            perror("dup2 failed for stdin redirection");
                            exit(EXIT_FAILURE);
                        }
                        
                        //close the original copy in the file des table
                        close(memfd);
                        
                        // Create new args without the args after redirection symbol
                        char** filtered_args = create_arguments_for_redirection(current_cmd);
                        
                        // Replace the command arguments, so that we can directly pass it to fexec from our cmd object
                        // first free the oriignal arguments array manually
                        for (int k = 0; k < current_cmd->argc; k++) {
                            free(current_cmd->argv[k]);
                        }
                        free(current_cmd->argv);

                        //now assign the new args
                        current_cmd->argv = filtered_args;
                        
                        // Count new value of argc
                        int new_argc = 0;
                        while (filtered_args[new_argc] != NULL) {
                            new_argc++;
                        }
                        //assign it to our command
                        current_cmd->argc = new_argc;
                        
                        break;  //terminate loop, as im only supporting one direction operator not multiple redirection operator
                    }
                }
            }
            
            // Set up input redirection from previous pipe for 2nd,3rd,4th,5th,.....,nth commands as they will not have the file redirectional input
            if (i > 0) {    //dup the pipe read end of prev command to the stdin of this commmand
                if (dup2(pipes[i-1][PIPE_READ_END], STDIN_FILENO) == -1) {
                    perror("dup2 failed for stdin");
                    exit(EXIT_FAILURE);
                }
            }
            
            // Set up output redirection to next pipe if not last command
            if (i < num_commands - 1) {    // do this for only 1st,2nd,3rd,4th,.....,n-1th command only
                //dup the pipe write end for this command to the stdout
                if (dup2(pipes[i][PIPE_WRITE_END], STDOUT_FILENO) == -1) {
                    perror("dup2 failed for stdout");
                    exit(EXIT_FAILURE);
                }
            } else if (output_fd != STDOUT_FILENO) {    // for the last command redirect to either (stdout) or (both stdout and ouput file)
                if (dup2(output_fd, STDOUT_FILENO) == -1) {     //log file is provided and enabled so attach the pipe-end to log file
                    perror("dup2 failed for stdout to log");
                    exit(EXIT_FAILURE);
                }
            }
            
            // Close all pipe file pipe ends for this command's process
            for (int j = 0; j < num_commands - 1; j++) {
                close(pipes[j][PIPE_READ_END]);
                close(pipes[j][PIPE_WRITE_END]);
            }
            
            // Copy the command data from NQP file system to local file
            // first create teh absolute path
            const char* cmd_name = current_cmd->argv[0];
            char cmd_absolute_path[MAX_LINE_SIZE] = {0};
            if(cwd->path[strlen(cwd->path) - 1] != '/'){
                snprintf(cmd_absolute_path, MAX_LINE_SIZE, "%s/%s", cwd->path, cmd_name);
            }else{
                snprintf(cmd_absolute_path, MAX_LINE_SIZE, "%s%s", cwd->path, cmd_name);
            }
            assert(is_valid_path(cmd_absolute_path));
            
            // Open command file from the nqp fs
            int cmd_fd = nqp_open(cmd_absolute_path);
            if (cmd_fd == NQP_FILE_NOT_FOUND) {
                fprintf(stderr, "Command not found: %s\n", cmd_name);
                exit(EXIT_FAILURE);
            }
            
            // Create a local memory file to copy npq fs data
            int exec_memfd = memfd_create("exec", 0);
            if (exec_memfd == -1) {
                perror("memfd_create failed");
                exit(EXIT_FAILURE);
            }
            
            // Read the command file from nqp fs and copy it to the local memory file
            char buffer[READ_BUFFER_SIZE];
            size_t bytes_read;
            while ((bytes_read = nqp_read(cmd_fd, buffer, READ_BUFFER_SIZE)) > 0) {
                if (write(exec_memfd, buffer, bytes_read) == -1) {
                    perror("write to memfd failed");
                    exit(EXIT_FAILURE);
                }
            }
            
            // Close the command file in the nqp fs to free its OFT for other other processes
            nqp_close(cmd_fd);
            
            // Reset the offset of the memory fd file to 0
            if (lseek(exec_memfd, 0, SEEK_SET) == -1) {
                perror("lseek failed");
                exit(EXIT_FAILURE);
            }
            
            // Execute the local copy of the command with the command's argument
            // TODO: do not forget to pass the envp from that main file
            if (fexecve(exec_memfd, current_cmd->argv, envp) == -1) {
                perror("fexecve failed");
                exit(EXIT_FAILURE);
            }
            
            //fexec failed, terminate the process
            fprintf(stderr, "Exec failed\n");
            exit(EXIT_FAILURE);
        }
        
        // INSIDE PARENT PROCESS:
        // Store child PID, so we can wait for it
        child_pids[i] = pid;
    }
    
    // INSIDE Parent process - close all the remaining pipe ends so all the completed processes actually terminates
    for (int i = 0; i < num_commands - 1; i++) {
        close(pipes[i][PIPE_READ_END]);
        close(pipes[i][PIPE_WRITE_END]);
    }
    
    // Wait for all child processes to complete executing their commands
    for (int i = 0; i < num_commands; i++) {
        waitpid(child_pids[i], NULL, 0);
    }

    return OPERATION_SUCCEED;   //return success code
}


/*
 * execute_pipes_with_logging():
 * wrapper for the execute_pipes(), it sets up 1 pipe to redirect output from the last command of pipes 
 * and print it to both stdout and a log file
 * RETURN CODE: OPERATION_FAILED (pipe creation fails OR )
 *              result (return code from the execute_pipe())
 */
int execute_pipes_with_logging(Pipe_Commands* pipe_cmds, const Curr_Dir* cwd, char *envp[]) {
    //params validation
    assert(pipe_cmds != NULL);
    assert(cwd != NULL);
    assert(log_fd != LOG_DISABLED);
    
    // Create a log pipe to redirect output from the execute_pipe()
    int log_pipe[2];
    if (pipe(log_pipe) == -1) {     //populate the log_pipe
        perror("Failed to create log pipe");
        return OPERATION_FAILED;
    }
    
    // call the execute_pipes() but pass the write end of the log pipe as output_fd
    int result = execute_pipes(pipe_cmds, cwd, envp, log_pipe[PIPE_WRITE_END]);
    
    // Close write end of the pipe once all the commands are done executing
    close(log_pipe[PIPE_WRITE_END]);
    
    // Read from the pipe's read end and write to both stdout and log file
    char buffer[READ_BUFFER_SIZE];
    ssize_t bytes_read;
    while ((bytes_read = read(log_pipe[PIPE_READ_END], buffer, READ_BUFFER_SIZE)) > 0) {
        // Write to stdout
        if (write(STDOUT_FILENO, buffer, bytes_read) != bytes_read) {
            perror("Failed to write to stdout");    //error in writing, so do cleanup
            close(log_pipe[PIPE_READ_END]);
            return OPERATION_FAILED;
        }
        
        // Write to log file
        if (write(log_fd, buffer, bytes_read) != bytes_read) {
            perror("Failed to write to log file");  //error in writing, so do cleanup
            close(log_pipe[PIPE_READ_END]);
            return OPERATION_FAILED;
        }
    }
    
    // Close read end of the pipe to close all the remaining pipes
    close(log_pipe[PIPE_READ_END]);
    
    return result;
}


//-----------------------
//PIPES RELATED ROUTINES
//-----------------------
//calc_num_pipes_marker(): counts the number of pipe operator the in the command
//returns 0: if no marker present or something goes wrong or invalid use of pipes
int calc_num_pipes_marker(const Command* cmd) {
    // parms check
    assert(cmd != NULL && "calc_num_pipes_marker::Command pointer cannot be NULL");
    if (!command_is_valid(cmd)) {
        printf("INVALID: Arguments => ");
        command_print(cmd);
        return 0;
    }

    int pipe_count = 0;
    
    // Iterate through arguments
    for (int i = 0; i < cmd->argc; i++) {
        const char* arg = command_get_arg(cmd, i);
        assert(arg != NULL && "calc_num_pipes_marker::Command argument cannot be NULL");
        
        // Check for pipe character only if argument is a valid string
        if (is_valid_string(arg) && strcmp(arg, "|") == 0) {
            pipe_count++;
        }
    }

    //validate the pipes are used properly
    if(pipe_count > 0 && !validate_pipe_positions(cmd)){    //if pipe exist BUT not used correctly
        pipe_count = INVALID_USECASE; //treat it as pipe not used properly
    }

    return pipe_count;
}

//validate_pipe_positions(): validates whether pipes are used properly syntax wise in the command
//returns false: if there is consecutive pipes without any commands in between OR pipes are at the end/start
bool validate_pipe_positions(const Command* cmd) {
    // params checks
    assert(cmd != NULL && "Command pointer cannot be NULL");
    if (!command_is_valid(cmd)) return false;
    
    //loop throuhg each args
    for (int i = 0; i < cmd->argc; i++) {
        const char* arg = command_get_arg(cmd, i);      //get teh command
        assert(arg != NULL && "Command argument cannot be NULL");

        //look for pipe operator 
        if (strcmp(arg, "|") == 0) {
            
            // Check previous argument exists and is not a pipe operator
            if (i == 0 || !is_valid_string(command_get_arg(cmd, i-1)) || 
                strcmp(command_get_arg(cmd, i-1), "|") == 0) {
                printf("INVALID USE: of pipe operator => ");
                command_print(cmd);
                return false;
            }

            // Check next argument exists and is not a pipe operator
            if (i == cmd->argc-1 || !is_valid_string(command_get_arg(cmd, i+1)) || 
                strcmp(command_get_arg(cmd, i+1), "|") == 0) {
                printf("INVALID USE: of pipe operator => ");
                command_print(cmd);
                return false;
            }
        }
    }

    // If no pipes found, still consider valid 
    return true;
}


//MAIN FUNCTION
int main( int argc, char *argv[], char *envp[] ){
    char line_buffer[MAX_LINE_SIZE] = {0};      //store the user input

    char *volume_label = NULL;
    nqp_error mount_error;

    if ( argc != 2 && argc != 4){
        fprintf( stderr, "Usage: ./nqp_shell volume.img\n" );
        exit( EXIT_FAILURE );
    }

    mount_error = nqp_mount( argv[1], NQP_FS_EXFAT );

    if ( mount_error != NQP_OK ){
        if ( mount_error == NQP_FSCK_FAIL ){
            fprintf( stderr, "%s is inconsistent, not mounting.\n", argv[1] );
        }
        exit( EXIT_FAILURE );
    }

    volume_label = nqp_vol_label( );

    printf( "%s:\\> ", volume_label );

    //LOG INITIALISING
    //make sure there is valid number of args before setting up the log file
    if (argc == 4) {    //must have 4 => ./nqp_shell root.img -o log_file.txt
        if (strcmp(argv[2], "-o") != 0) {
            fprintf(stderr, "INVALID Usage: Must be ./nqp_shell volume.img -o log.txt\n");
            exit(EXIT_FAILURE);
        }
        
        // Open the given log file
        log_fd = open(argv[3], O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (log_fd < 0) {
            perror("Failed to open log file");
            exit(EXIT_FAILURE);
        }
    }

    //TESTING BUILTINS & CURR_DIR
    //test_all();
    //TESTING BUILTINS & CURR_DIR

    //Initialise curr_dir with root directory
    Curr_Dir* cwd = construct_empty_curr_dir();

    //start the shell
    while (true){
        char mssg[MAX_LINE_SIZE] = {0};             //store the message to print before each command
        snprintf(mssg, MAX_LINE_SIZE, "%s:\\> ",volume_label);
        fflush(stdout);
        char* line = readline("");                  //read user input using readline
        custom_print(mssg);                         //print that using the custom print
        strncpy(line_buffer, line, MAX_LINE_SIZE);  //copy the input to safe copy of line in line_buffer for processing

        if (line == NULL) { // EOF (Ctrl+D pressed)
            custom_print("\n");
            break;
        }

        //BONUS PART: Adding the current command to history for navigating the commands with arrows
        add_history(line_buffer);

        //create the command object with the input
        Command* new_command = command_create(line_buffer);
        assert(NULL != new_command);
        if(NULL == new_command) return EXIT_FAILURE;

        if(!command_is_valid(new_command)){     //something is not correct with the user input
            command_destroy(new_command);       //free the resource
            continue;                           //read next command 
        }

        //check if there is any pipe in it
        const int num_pipes = calc_num_pipes_marker(new_command);
        if(num_pipes > 0){ //found atleast 1 pipe
            //create the parsed pipe_commands object
            Pipe_Commands* pipe_cmds = create_Pipe_Commands(num_pipes,line_buffer);
            assert(pipe_commands_is_valid(pipe_cmds));
            if(NULL == pipe_cmds) continue;

            //execute it based on logging condiition
            int return_code;
            if (log_fd != LOG_DISABLED) {       // logging is enabled, and command has pipe init
                return_code = execute_pipes_with_logging(pipe_cmds, cwd, envp);
            } else {                            // logging is disabled, and command has pipe init
                return_code = execute_pipes(pipe_cmds, cwd, envp, STDOUT_FILENO);
            }
            
            //print the error code
            if (return_code != OPERATION_SUCCEED) {
                fprintf(stderr, "Pipe execution failed with code: %d\n", return_code);
            }

            //free up the resources
            pipe_commands_destroy(pipe_cmds);
            command_destroy(new_command);
            continue;   //continue reading next command
        }

        //print error message for invalid use of pipe
        if(num_pipes == INVALID_USECASE){
            printf("pipe operator not used properly\n");
            command_destroy(new_command);   //free up the resources
            continue;                       //read next command
        }

        //Execute the command noramally if pipes are not present
        if(!execute_command(new_command, cwd, envp) && 0 == num_pipes){
            //execution failed
            custom_print("Failure to execute the command:\n");
            command_print(new_command);

            command_destroy(new_command);   //free up the resources
            continue;                       //read next command
        }

        //reset the line buffer for next Command
        memset(line_buffer,0,MAX_LINE_SIZE);
    }

    //free up the resources once EOF is reached or shell is terminated
    assert(NULL != cwd);
    if(NULL != cwd){
        destroy_curr_dir(cwd);
    }
    free_logs();    //close the log related resources
    return EXIT_SUCCESS;
}



//---------------------------------------------------------------------------------------------------------
//TESTING BLOCK STARTS
void test_curr_dir(void) {
    // Test construct_curr_dir()
    Curr_Dir* cwd = construct_empty_curr_dir();
    assert(cwd != NULL);
    assert(strcmp(cwd->path, "/") == 0);
    destroy_curr_dir(cwd);

    // Test construct_curr_dir(char* path)
    cwd = construct_curr_dir("/home");
    assert(cwd != NULL);
    assert(strcmp(cwd->path, "/home") == 0);
    destroy_curr_dir(cwd);

    // Test set_path()
    cwd = construct_empty_curr_dir();
    set_path(cwd, "/usr/local");
    assert(strcmp(cwd->path, "/usr/local") == 0);
    destroy_curr_dir(cwd);
}
void test_validators(void) {
    // Test is_valid_string()
    assert(is_valid_string("hello") == true);
    assert(is_valid_string("") == false);
    assert(is_valid_string(NULL) == false);

    // Test is_only_whitespace()
    assert(is_only_whitespace("   ") == true);
    assert(is_only_whitespace("hello") == false);
    assert(is_only_whitespace(" hello ") == false);

    // Test is_valid_path()
    assert(is_valid_path("/home/user") == true);
    assert(is_valid_path("home/user") == false);
    assert(is_valid_path("") == false);

    // Test is_valid_curr_dir()
    Curr_Dir* cwd = construct_empty_curr_dir();
    assert(is_valid_curr_dir(cwd) == true);
    destroy_curr_dir(cwd);
}
void test_builtins(void) {
    Curr_Dir* cwd = construct_curr_dir("/dictionary-words");

    // Test command_pwd()
    // This will print the current directory, visually verify the output
    printf("\npwd: %s\n", cwd->path);
    command_pwd(cwd);

    // Test command_ls()
    // This will list the contents of the root directory, visually verify the output
    printf("\ncwd: %s\n", cwd->path);
    command_ls(cwd);

    // Test command_cd()
    command_cd("..", cwd);
    assert(strcmp(cwd->path, "/") == 0);
    printf("\ncd .. : %s\n", cwd->path);
    command_ls(cwd);

    command_cd("..", cwd);
    printf("\ncd ..: %s\n", cwd->path);
    assert(strcmp(cwd->path, "/") == 0);

    command_cd("dictionary-words", cwd);
    assert(strcmp(cwd->path, "/dictionary-words") == 0);
    printf("\ncd dictionary-words: %s\n", cwd->path);
    command_ls(cwd);

    destroy_curr_dir(cwd);
}
//COMMAND OBJECT TEST
void test_command_obj(void) {
    printf("Running tests...\n\n");

    // Test 1 Create and validate a simple command
    {
        const char* input = "./code root.img";
        Command* cmd = command_create(input);
        assert(cmd != NULL);
        assert(command_is_valid(cmd));
        assert(cmd->argc == 2);
        assert(strcmp(command_get_arg(cmd, 0), "./code") == 0);
        assert(strcmp(command_get_arg(cmd, 1), "root.img") == 0);
        command_print(cmd);
        command_destroy(cmd);
        printf("Test 1 passed.\n\n");
    }

    // Test 2: Create a command with maximum arguments
    {
        char input[MAX_ARGS * 2] = {0};
        for (int i = 0; i < MAX_ARGS; i++) {
            char arg[4];
            snprintf(arg, sizeof(arg), "a%d", i);
            strcat(input, arg);
            if (i < MAX_ARGS - 1) strcat(input, " ");
        }
        Command* cmd = command_create(input);
        assert(cmd != NULL);
        assert(command_is_valid(cmd));
        assert(cmd->argc == MAX_ARGS);
        command_print(cmd);
        command_destroy(cmd);
        printf("Test 2 passed.\n\n");
    }

    // Test 3: Try to create an invalid command (empty input)
    {
        const char* input = "";
        Command* cmd = command_create(input);
        assert(cmd != NULL);
        assert(!command_is_valid(cmd));
        command_destroy(cmd);
        printf("Test 3 passed.\n\n");
    }

    // Test 4: Test command_get_arg with invalid index
    {
        const char* input = "test command";
        Command* cmd = command_create(input);
        assert(cmd != NULL);
        assert(command_is_valid(cmd));
        assert(command_get_arg(cmd, -1) == NULL);
        assert(command_get_arg(cmd, 2) == NULL);
        command_destroy(cmd);
        printf("Test 4 passed.\n\n");
    }

    printf("All tests passed!\n");
}
void test_all(void){
    test_curr_dir();
    test_validators();
    test_builtins();
    test_command_obj();
}
//TESTING BLOCK ENDS
//---------------------------------------------------------------------------------------------------------


