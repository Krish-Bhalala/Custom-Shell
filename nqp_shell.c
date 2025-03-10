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
    printf("%s\n", cwd->path);                  //printing
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
        printf( "%lu %s", entry.inode_number, entry.name ); //print its metadata
        if ( entry.type == DT_DIR ){                        //append "/", if its a directory
            putchar('/');
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
int import_command_to_memfd(const char* path) {
    // Check if the path is valid
    if (!path) {
        printf("INVALID file path to search in mounted filesystem\n");
        return COMMAND_NOT_FOUND;
    }
    assert(path != NULL); 
    if(!is_valid_path(path)){
        printf("INVALID PATH: {%s}\n",path);
        return COMMAND_NOT_FOUND;
    }
    assert(is_valid_path(path)); 
    
    // Try to open the file in the nqp filesystem
    int nqp_fd = nqp_open(path);
    if (nqp_fd == NQP_FILE_NOT_FOUND) {
        printf("import_command_to_memfd:Command Not Found in mounted filesystem {%s}\n", path);
        return COMMAND_NOT_FOUND;
    }
    
    // Make a new file in memory to store the command
    int mem_fd = memfd_create(path, 0);
    if (mem_fd < 0) {   // If making the memory file failed
        nqp_close(nqp_fd);
        return COMMAND_EXECUTION_FAILED;
    }
    
    // Copy the command from the nqp file to the memory file
    lseek(mem_fd, 0, SEEK_SET);  // Go to the start of the memory file
    char buffer[READ_BUFFER_SIZE] = {0};
    ssize_t bytes_read;
    while ((bytes_read = nqp_read(nqp_fd, buffer, READ_BUFFER_SIZE)) > 0) {
        write(mem_fd, buffer, bytes_read);  // Write what we read to the memory file
        memset(buffer, 0, READ_BUFFER_SIZE);    // Clear the buffer for next use
    }
    
    // Clean up and prepare the memory file for use
    nqp_close(nqp_fd);  // Close the nqp file
    lseek(mem_fd, 0, SEEK_SET);  // Go back to the start of the memory file
    
    return mem_fd;  // Return the memory file descriptor
}
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
    strcat(path,command);   //concatenate the command to that path
    
    //open the file in nqp file system
    int nqp_fd = nqp_open(path);
    if(nqp_fd == NQP_FILE_NOT_FOUND){
        free(command);
        return COMMAND_NOT_FOUND;
    }
    assert(nqp_fd >= 0);
    free(path); //path is no longer needed so free it

    //creat a new file in local memory
    int mem_fd = memfd_create("FileSystemCode", 0); 
    if(mem_fd == NQP_FILE_NOT_FOUND) {
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
        //verify arguments
        char** arguments = cmd->argv;

        //handle the input redirection
        int input_fd = -1;
        input_fd = handle_input_redirection(cmd,curr_path);     //copy the mounted fs's file into local memory and open it
        if (input_fd > 0) { //if input_fd is different than STDIN_FILENO then redirection is enabled
            //if redirection is required, change the file descriptor of stdin with input_fd
            dup2(input_fd, STDIN_FILENO);
            close(input_fd);

            //remove the arguments after "<" since redirection is enabled and we already piped it with our process's stdin
            char **minimal_args = create_arguments_for_redirection(cmd);    
            arguments = minimal_args;
            assert(minimal_args != NULL);
        }
        if(REDIRECTION_FAILED == input_fd){ //there was a redirection operator in args and it failed
            printf("Redirection Failed pls try again\n");   //terminate the process 
            exit(EXIT_FAILURE);
        }       
        if(INVALID_ARGUMENTS == input_fd){ //invalid args to handle_input_redirection()
            printf("Invalid Arguments passed to redirection command");
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
            close(pipefd[PIPE_WRITE_END]);  // Close write end
            
            //read the output buffers data and write it to the output buffers
            char buffer[READ_BUFFER_SIZE];
            ssize_t n;
            while ((n = read(pipefd[0], buffer, READ_BUFFER_SIZE)) > 0) {
                write(STDOUT_FILENO, buffer, n);    // Write to stdout
                write(log_fd, buffer, n);   // Write to log file
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
int handle_input_redirection(const Command* cmd, const char* cwd_path){
    assert(command_is_valid(cmd));
    assert(cwd_path != NULL);
    assert(is_valid_path(cwd_path));
    if(NULL == cmd || NULL == cwd_path) return INVALID_ARGUMENTS;
    if(!command_is_valid(cmd) || !is_valid_path(cwd_path)) return INVALID_ARGUMENTS;

    //loop through the command arguments
    for(int i=0; i<cmd->argc; i++){
        //search for "<" in the arguments
        if(strcmp(command_get_arg(cmd,i),"<") == 0){ //detected redirection operator
            //check if redirection is possible or not, if the command contains "<" operator
            if(cmd->argc < 3){
                printf("ERROR: Not enough arguments for redirection, Minimum 3 arguments required\n");
                return REDIRECTION_FAILED;
            }
            if(i+1 >= cmd->argc){    //making sure the redirection operator isn't the last argument
                printf("ERROR: Last argument should be a filename not the redirection operator\n");
                return REDIRECTION_FAILED;
            }

            //open the input file
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
            if(input_fd < 0){
                printf("Redirection Error: nqp_open input file {%s} not found\n", command_get_arg(cmd,i+1));
                return REDIRECTION_FAILED;
            }
            assert(input_fd >= 0);

            //create a memory file to store the input file
            int mem_fd = memfd_create(filepath, 0);
            if (mem_fd < 0) {
                printf("ERROR: memfd_create can't create input_redirect");
                nqp_close(input_fd);    //free resources
                return REDIRECTION_FAILED;
            }
            assert(mem_fd >= 0);

            //read the input file and write it in the memory file
            char buffer[READ_BUFFER_SIZE] = {0};
            ssize_t bytes_read = 0;
            while ((bytes_read = nqp_read(input_fd, buffer, sizeof(buffer))) > 0) {
                write(mem_fd, &buffer, bytes_read);     //write it in the memory file
                memset(buffer, 0, READ_BUFFER_SIZE);    //reset the buffer
            }
            nqp_close(input_fd);            //close the input file in the nqp file system
            lseek(mem_fd, 0, SEEK_SET);     //reset read offset of the memory input file

            //printf("handle_input_redirection: memfd = {%d}\n", mem_fd);
            return mem_fd;  //return the fd of memory input file
        }
    }
    //if there is no redirection needed, then return the standard input file num
    return STDIN_FILENO;
}



//LOGGING RELATED ROUTINES
void custom_print(const char *message) {
    assert(message != NULL);
    if (!message) return;
    
    // Write to stdout
    printf("%s",message);
    fflush(stdout);
    
    // Write to log file if enabled
    if (log_fd != LOG_DISABLED) {
        write(log_fd, message, strlen(message));
    }
}
void free_logs(){
    if (log_fd != LOG_DISABLED) {
        close(log_fd);
    }
}



//PIPE COMMANDS OBJECT
// typedef struct{
//     int num_commands;   //total number of commands in the pipe
//     Command** commands; //array of commands
// }Pipe_Commands;
void print_pipe_commands(const Pipe_Commands* pipe_cmds) {
    if (pipe_cmds == NULL) {
        printf("Pipe_Commands is NULL\n");
        return;
    }

    printf("Number of commands in pipe: %d\n", pipe_cmds->num_commands);
    
    for (int i = 0; i < pipe_cmds->num_commands; i++) {
        printf("Command %d:\n", i + 1);
        if (pipe_cmds->commands[i] != NULL) {
            command_print(pipe_cmds->commands[i]);  // Assuming this function exists
        } else {
            printf("  (NULL command)\n");
        }
        
        // Print a separator between commands, except for the last one
        if (i < pipe_cmds->num_commands - 1) {
            printf("  |\n");  // Pipe symbol to visually separate commands
        }
    }
}
Pipe_Commands* create_Pipe_Commands(const int num_pipes, char* line){
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
    pipe_commands_obj->num_commands = num_pipes+1;
    assert(pipe_commands_obj->num_commands == num_pipes+1);

    // Split the line by pipe operators and create a command object for each sub string
    //char *token = strtok(line, "|");
    char* token = NULL;
    char cmd_str[MAX_LINE_SIZE];
    int i = 0;
    int count = 0;
    //printf("TOKEN: {%s}\n",token);
    while ((token = strsep(&line, "|")) != NULL && i < num_pipes+1) {
        count++;
        assert(num_pipes+1 > i);

        //create a copy of command for creating the command
        memset(cmd_str,0,MAX_LINE_SIZE);
        strncpy(cmd_str,token,MAX_LINE_SIZE);

        trim_string(cmd_str);
        assert(is_valid_string(cmd_str));
        //printf("TOKEN: {%s}\n",cmd_str);

        // Create a Command object with the token
        cmd_array[i] = command_create(cmd_str);
        if (!cmd_array[i]) {
            fprintf(stderr, "Failed to create Command object for: %s\n", cmd_str);

            // Clean up on failure
            pipe_commands_destroy(pipe_commands_obj);
            return NULL;
        }
        if(0 == cmd_array[i]->argc){    //invalid command arguments given in line
            fprintf(stderr, "INVALID Command within the pipe\n");
            pipe_commands_destroy(pipe_commands_obj);
            return NULL;
        }

        i++;
    }

    assert(count == pipe_commands_obj->num_commands);
    return pipe_commands_obj;    //! change to pipe commands object
}
void pipe_commands_destroy(Pipe_Commands* pipe) {
    if (pipe == NULL) return;

    // Free individual Command objects
    if (pipe->commands != NULL) {
        for (int i = 0; i < pipe->num_commands; i++) {
            if (pipe->commands[i] != NULL) {
                // Assuming Command has a destructor
                command_destroy(pipe->commands[i]);
            }
        }
        // Free the array of Command pointers
        free(pipe->commands);
    }

    // Free the Pipe_Commands struct itself
    free(pipe);
}
bool pipe_commands_is_valid(const Pipe_Commands* pc) {
    // Check if the struct itself exists
    if (pc == NULL) return false;
    
    // Validate number of commands
    if (pc->num_commands < 1) return false;
    
    // Validate commands array
    if (pc->commands == NULL) return false;
    
    // Validate individual commands
    for (int i = 0; i < pc->num_commands; i++) {
        if (pc->commands[i] == NULL || !command_is_valid(pc->commands[i])) {    //!guess gault
            return false;
        }
    }
    
    // All checks passed
    return true;
}
Command* pipe_commands_get_command_at(const Pipe_Commands* cmd_list, const int index){
    if (!cmd_list || index < 0 || index >= cmd_list->num_commands) return NULL;
    return cmd_list->commands[index];
}
/*
int execute_pipes(Pipe_Commands* cmd_list, const Curr_Dir* cwd, char *envp[], const int output_fd){
    assert(pipe_commands_is_valid(cmd_list));
    assert(is_valid_curr_dir(cwd));
    if(NULL == cmd_list || NULL == cwd || !is_valid_curr_dir(cwd)){
        printf("execute_pipes: NULL commmands arguments\n");
        return INVALID_ARGUMENTS;
    }
    assert(cmd_list->num_commands >= 2);    //there should be atleast 2 commands for every pipe
    if(cmd_list->num_commands < 2){
        printf("execute_pipes: INSUFFICIENT number of commands to support pipe\n");
        return INVALID_ARGUMENTS;
    }

    // initialize the pipes
    int pipes[(cmd_list->num_commands)-1][2];
    for (int i = 0; i < cmd_list->num_commands - 1; i++) {
        if (pipe(pipes[i]) == -1) {
            perror("execute_pipes:pipe creation failed");
            // Clean up any pipes already created
            for (int j = 0; j < i; j++) {
                close(pipes[j][PIPE_READ_END]);
                close(pipes[j][PIPE_WRITE_END]);
            }
            return OPERATION_FAILED;
        }
    }

    // Array to store all child process IDs
    pid_t child_pids[cmd_list->num_commands];

    // Set up the pipes and
    // Execute each command in the pipeline
    for (int i = 0; i < cmd_list->num_commands; i++) {
        //create the command from the cmd_list
        Command* cmd = pipe_commands_get_command_at(cmd_list,i);
        printf("CHILD: execute_pipe: command {%d} {%s}\n",i,cmd->argv[0]);
        fflush(stdout);
        if (!cmd){
            printf("execute_pipe::ERROR in processing pipe command at position %d\n",i);
            continue;
        }
        //store the command name
        const char* cmd_name = command_get_arg(cmd, 0);
        if (!cmd_name) continue;
        
        // fork a process for running ith cmd
        pid_t pid = fork();
        
        if (pid < 0) { // Fork failed
            perror("fork failed");
            // Clean up pipes
            for (int j = 0; j < cmd_list->num_commands - 1; j++) {
                close(pipes[j][PIPE_READ_END]);
                close(pipes[j][PIPE_WRITE_END]);
            }
            // Clean up any child processes already started
            for (int j = 0; j < i; j++) {
                kill(child_pids[j], SIGTERM);
                waitpid(child_pids[j], NULL, 0);    //confirm that the child terminated
            }
            return OPERATION_FAILED;
        } else if (pid == 0) { // Child process: running that command
            // Set up input redirection for the first command if present any
            if (i == 0) {   // the first command for redirection
                int input_fd = handle_input_redirection(cmd, cwd->path);
                if (input_fd == REDIRECTION_FAILED || input_fd == INVALID_ARGUMENTS) {
                    exit(EXIT_FAILURE); //kill the process
                } else if (input_fd != STDIN_FILENO) {  //redirection from non stdin file
                    dup2(input_fd, STDIN_FILENO);
                    close(input_fd);                //close the copy of fd of input file
                }
            } else { // Not the first command, read from previous pipe
                dup2(pipes[i-1][PIPE_READ_END], STDIN_FILENO);
            }
            
            // Set up output redirection for all but the last command
            printf("LINE 906: i={%d} cmd_list->num_commands-1={%d}\n",i,cmd_list->num_commands - 1);
                command_print(cmd_list->commands[i]);
                fflush(stdout);
            if (i < cmd_list->num_commands - 1) {
                dup2(pipes[i][PIPE_WRITE_END], STDOUT_FILENO);
                printf("LINE 907: i = {%d}, pipid{%d}==stdoutfileno{%d}\n",i,pipes[i][PIPE_WRITE_END],STDOUT_FILENO);
                fflush(stdout);
            } else if (i == cmd_list->num_commands - 1 && output_fd > 1) { //this is the last command and logging is enabled
                printf("LINE 911: i==last command {%d}\n",i);
                command_print(cmd_list->commands[i]);
                fflush(stdout);
                // set up a pipe back to the parent for logging
                int log_pipe[2];
                if (pipe(log_pipe) < 0) {
                    perror("execute_pipes::log pipe creation failed");
                    exit(EXIT_FAILURE);
                }
                
                // Fork again for the last command with logging
                pid_t log_pid = fork();
                if (log_pid < 0) {
                    perror("execute_pipes::log fork failed");
                    exit(EXIT_FAILURE);
                } else if (log_pid == 0) { // Grandchild process - execute the command
                    dup2(log_pipe[PIPE_WRITE_END], STDOUT_FILENO);
                    close(log_pipe[PIPE_READ_END]);
                    close(log_pipe[PIPE_WRITE_END]);
                    
                    // Close all other pipes
                    for (int j = 0; j < cmd_list->num_commands - 1; j++) {
                        close(pipes[j][PIPE_READ_END]);
                        close(pipes[j][PIPE_WRITE_END]);
                    }
                    
                    //Create the absolute path for executing the last command
                    char cmd_path[MAX_LINE_SIZE] = {0};
                    if(cwd->path[strlen(cwd->path) - 1] != '/'){
                        snprintf(cmd_path, MAX_LINE_SIZE, "%s/%s", cwd->path, cmd_name);
                    }else{
                        snprintf(cmd_path, MAX_LINE_SIZE, "%s%s", cwd->path, cmd_name);
                    }
                    
                    // Create a mem_fd file with the nqp fs file data
                    // if(!is_valid_path(cmd_path)){
                    //     fflush(stdout);
                    //     printf("903: INVALID PATH: {%s}\n",cmd_path);   //! fix invalid cmd_path
                    //     fflush(stdout);
                    // }
                    int mem_fd = import_command_to_memfd(cmd_path);
                    if (mem_fd < 0) {
                        fprintf(stderr, "Command not found in the mouted file system: {%s} error code: {%d} path: {%s}\n", cmd_name, mem_fd, cmd_path);
                        exit(EXIT_FAILURE);
                    }
                    
                    for (int i = 0; cmd->argv[i] != NULL; i++) {
                        printf("%s\n", cmd->argv[i]);
                        if(NULL == cmd->argv[i+1]){
                            printf("NULL\n");
                        }
                        fflush(stdout);
                    }
                    // Execute the command
                    fexecve(mem_fd, cmd->argv, envp);
                    perror("fexecve failed");
                    exit(EXIT_FAILURE);
                } else { // Child process - read output and duplicate to stdout and log
                    close(log_pipe[PIPE_WRITE_END]);
                    
                    //Create a buffer for reading
                    char buffer[READ_BUFFER_SIZE] = {0};
                    ssize_t bytes_read;
                    lseek(log_pipe[PIPE_READ_END],0,SEEK_SET);  //make sure the read offset is resetted
                    
                    // Read from the log pipe and write to stdout and log_fd
                    while ((bytes_read = read(log_pipe[PIPE_READ_END], buffer, READ_BUFFER_SIZE)) > 0) {
                        write(STDOUT_FILENO, buffer, bytes_read);   //write to stdout
                        write(output_fd, buffer, bytes_read);       //write to output file
                    }
                    
                    // Wait for the grandchild to die
                    waitpid(log_pid, NULL, 0);
                    exit(EXIT_SUCCESS);
                }
            }
            
            // Close all pipes in child
            for (int j = 0; j < cmd_list->num_commands - 1; j++) {
                close(pipes[j][PIPE_READ_END]);
                close(pipes[j][PIPE_WRITE_END]);
            }
            
            // Execute the command
            // Create the absolute file path of the command's files to read from the nqp fs
            char cmd_path[MAX_LINE_SIZE] = {0};
            if(cwd->path[strlen(cwd->path) - 1] != '/'){
                snprintf(cmd_path, MAX_LINE_SIZE, "%s/%s", cwd->path, cmd_name);
            }else{
                snprintf(cmd_path, MAX_LINE_SIZE, "%s%s", cwd->path, cmd_name);
            }
            
            // Read the command file's data from nqp fs and write it into the local memory file
            int mem_fd = -1;
            mem_fd = import_command_to_memfd(cmd_path);
            if (mem_fd < 0) {
                fprintf(stderr, "Pipes: Command not found: {%s} err code: {%d} path: {%s}\n ", cmd_name, mem_fd, cmd_path);
                exit(EXIT_FAILURE);
            }
            if (lseek(mem_fd, 0, SEEK_SET) == -1) { //reset the memfd
                perror("memfd lseek failed");
                exit(EXIT_FAILURE);
            }
            
            // Execute the command
            printf("PIPE: memfd {%d}\n", mem_fd);
            for (int i = 0; cmd->argv[i] != NULL; i++) {
                printf("%s\n", cmd->argv[i]);
                if(NULL == cmd->argv[i+1]){
                    printf("NULL\n");
                }
                fflush(stdout);
            }
            //char* nullenvp[] = {0};
            fexecve(mem_fd, cmd->argv, envp);
            close(mem_fd);
            perror("fexecve failed");
            exit(EXIT_FAILURE);
        } else { // Parent process
            child_pids[i] = pid;
        }
    }

    // Parent process - close all pipes
    for (int i = 0; i < cmd_list->num_commands - 1; i++) {
        close(pipes[i][PIPE_READ_END]);
        close(pipes[i][PIPE_WRITE_END]);
    }

    // Wait for all children to complete
    for (int i = 0; i < cmd_list->num_commands; i++) {
        waitpid(child_pids[i], NULL, 0);
    }
    
    return OPERATION_SUCCEED;
}

int execute_pipes(Pipe_Commands* cmd_list, const Curr_Dir* cwd, char *envp[], const int output_fd) {
    printf("%d\n",output_fd);
    if (NULL == cmd_list || NULL == cwd || !is_valid_curr_dir(cwd)) {
        printf("execute_pipes: NULL commmands arguments\n");
        return INVALID_ARGUMENTS;
    }

    int num_commands = cmd_list->num_commands;
    if (num_commands <= 1) {
        printf("execute_pipes: INSUFFICIENT number of commands to support pipe\n");
        return INVALID_ARGUMENTS;
    }

    // Create pipes between commands
    int pipes[num_commands - 1][2];
    for (int i = 0; i < num_commands - 1; i++) {
        if (pipe(pipes[i]) == -1) {
            perror("execute_pipes:pipe creation failed");
            // Clean up any pipes already created
            for (int j = 0; j < i; j++) {
                close(pipes[j][PIPE_READ_END]);
                close(pipes[j][PIPE_WRITE_END]);
            }
            return OPERATION_FAILED;
        }
    }

    // Array to store child process IDs
    pid_t child_pids[num_commands];

    for (int i = 0; i < num_commands; i++) {
        Command* cmd = pipe_commands_get_command_at(cmd_list, i);
        if (!cmd) {
            printf("execute_pipe::ERROR in processing pipe command at position %d\n", i);
            continue;
        }

        const char* cmd_name = command_get_arg(cmd, 0);
        if (!cmd_name) continue;

        // Fork child process
        pid_t pid = fork();
        if (pid < 0) {  //fork failed
            perror("fork failed");
            // Clean up pipes and processes
            for (int j = 0; j < num_commands - 1; j++) {
                close(pipes[j][PIPE_READ_END]);
                close(pipes[j][PIPE_WRITE_END]);
            }
            for (int j = 0; j < i; j++) {
                kill(child_pids[j], SIGTERM);
                waitpid(child_pids[j], NULL, 0);
            }
            return OPERATION_FAILED;
        }

        if (pid == 0) {// Child process
            // Handle input redirection
            if (i > 0) {
                dup2(pipes[i-1][PIPE_READ_END], STDIN_FILENO);
            } else {
                int input_fd = handle_input_redirection(cmd, cwd->path);
                if (input_fd != REDIRECTION_FAILED && input_fd != INVALID_ARGUMENTS) {
                    if (input_fd != STDIN_FILENO) {
                        dup2(input_fd, STDIN_FILENO);
                        close(input_fd);
                    }
                }
            }

            // Handle output redirection
            if (i < num_commands - 1) {
                dup2(pipes[i][PIPE_WRITE_END], STDOUT_FILENO);
            }

            // Close unused pipe ends
            for (int j = 0; j < num_commands - 1; j++) {
                if (j != i && j != i-1) {
                    close(pipes[j][PIPE_READ_END]);
                    close(pipes[j][PIPE_WRITE_END]);
                }
            }

            // Execute command
            char cmd_path[MAX_LINE_SIZE] = {0};
            if(cwd->path[strlen(cwd->path) - 1] != '/'){
                snprintf(cmd_path, MAX_LINE_SIZE, "%s/%s", cwd->path, cmd_name);
            }else{
                snprintf(cmd_path, MAX_LINE_SIZE, "%s%s", cwd->path, cmd_name);
            }

            int mem_fd = import_command_to_memfd(cmd_path);
            if (mem_fd < 0) {
                fprintf(stderr, "execute_pipes: Command not found: %s\n", cmd_name);
                exit(EXIT_FAILURE);
            }

            fexecve(mem_fd, cmd->argv, envp);
            perror("fexecve failed");
            exit(EXIT_FAILURE);
        } else {
            // Parent process
            child_pids[i] = pid;
        }
    }

    // Parent process: Close all pipes
    for (int i = 0; i < num_commands - 1; i++) {
        close(pipes[i][PIPE_READ_END]);
        close(pipes[i][PIPE_WRITE_END]);
    }

    // Wait for all children
    for (int i = 0; i < num_commands; i++) {
        waitpid(child_pids[i], NULL, 0);
    }

    return OPERATION_SUCCEED;
}
*/
/**
 * @param cmd_list The list of commands to execute
 * @param cwd The current working directory
 * @param envp Environment variables
 * @param output_fd File descriptor for output (STDOUT_FILENO or log file)
 * @return Status code indicating success or failure
 */
int execute_pipes(Pipe_Commands* cmd_list, const Curr_Dir* cwd, char *envp[], const int output_fd) {
    assert(cmd_list != NULL);
    assert(cwd != NULL);
    assert(output_fd >= 0);
    
    if (!pipe_commands_is_valid(cmd_list)) {
        fprintf(stderr, "Invalid pipe commands\n");
        return INVALID_ARGUMENTS;
    }
    
    int num_commands = cmd_list->num_commands;
    assert(num_commands > 0);
    
    // Special case: if only one command, just execute it directly
    if (num_commands == 1) {
        Command* cmd = pipe_commands_get_command_at(cmd_list, 0);
        if (!execute_command(cmd, (Curr_Dir*)cwd, envp)) {
            return OPERATION_FAILED;
        }
        return OPERATION_SUCCEED;
    }
    
    // Create pipes for all commands except the last one
    int pipes[num_commands - 1][2];
    for (int i = 0; i < num_commands - 1; i++) {
        if (pipe(pipes[i]) == -1) {
            perror("Error creating pipe");
            
            // Close any pipes we've already created
            for (int j = 0; j < i; j++) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }
            
            return REDIRECTION_FAILED;
        }
    }
    
    // Array to store all child process IDs
    pid_t child_pids[num_commands];
    
    // Run all commands with appropriate piping
    for (int i = 0; i < num_commands; i++) {
        Command* current_cmd = pipe_commands_get_command_at(cmd_list, i);
        assert(current_cmd != NULL);
        
        pid_t pid = fork();
        
        if (pid == -1) {
            perror("Fork failed");
            
            // Close all pipes
            for (int j = 0; j < num_commands - 1; j++) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }
            
            return OPERATION_FAILED;
        }
        
        if (pid == 0) {  // Child process
            // Set up input redirection for first command if needed
            if (i == 0) {
                // Check for input redirection symbol "<"
                for (int j = 0; j < current_cmd->argc; j++) {
                    if (strcmp(current_cmd->argv[j], "<") == 0 && j + 1 < current_cmd->argc) {
                        // Found redirection symbol and filename
                        const char* filename = current_cmd->argv[j + 1];
                        char inputfile_absolute_path[MAX_LINE_SIZE] = {0};
                        if(cwd->path[strlen(cwd->path) - 1] != '/'){
                            snprintf(inputfile_absolute_path, MAX_LINE_SIZE, "%s/%s", cwd->path, filename);
                        }else{
                            snprintf(inputfile_absolute_path, MAX_LINE_SIZE, "%s%s", cwd->path, filename);
                        }
                        assert(is_valid_path(inputfile_absolute_path));

                        // Open the file from our file system
                        int fd = nqp_open(inputfile_absolute_path);
                        if (fd == NQP_FILE_NOT_FOUND) {
                            fprintf(stderr, "Failed to open file %s for redirection at path {%s}\n", filename, inputfile_absolute_path);
                            exit(EXIT_FAILURE);
                        }
                        
                        // Create memory-backed file descriptor
                        int memfd = memfd_create("input_redirect", 0);
                        if (memfd == -1) {
                            perror("memfd_create failed");
                            exit(EXIT_FAILURE);
                        }
                        
                        // Read file content into memory
                        char buffer[READ_BUFFER_SIZE];
                        size_t bytes_read;
                        while ((bytes_read = nqp_read(fd, buffer, READ_BUFFER_SIZE)) > 0) {
                            if (write(memfd, buffer, bytes_read) == -1) {
                                perror("write to memfd failed");
                                exit(EXIT_FAILURE);
                            }
                        }
                        
                        // Close the NQP file
                        nqp_close(fd);
                        
                        // Reset the mem fd offset to the beginning
                        if (lseek(memfd, 0, SEEK_SET) == -1) {
                            perror("lseek failed");
                            exit(EXIT_FAILURE);
                        }
                        
                        // Redirect stdin to our memory fd
                        if (dup2(memfd, STDIN_FILENO) == -1) {
                            perror("dup2 failed for stdin redirection");
                            exit(EXIT_FAILURE);
                        }
                        
                        close(memfd);
                        
                        // Create new args without the redirection symbols
                        char** filtered_args = create_arguments_for_redirection(current_cmd);
                        
                        // Replace the command arguments
                        for (int k = 0; k < current_cmd->argc; k++) {
                            free(current_cmd->argv[k]);
                        }
                        free(current_cmd->argv);
                        
                        current_cmd->argv = filtered_args;
                        
                        // Count new argc
                        int new_argc = 0;
                        while (filtered_args[new_argc] != NULL) {
                            new_argc++;
                        }
                        current_cmd->argc = new_argc;
                        
                        break;
                    }
                }
            }
            
            // Set up stdin (input) from previous pipe if not first command
            if (i > 0) {
                if (dup2(pipes[i-1][0], STDIN_FILENO) == -1) {
                    perror("dup2 failed for stdin");
                    exit(EXIT_FAILURE);
                }
            }
            
            // Set up stdout (output) to next pipe if not last command
            if (i < num_commands - 1) {
                if (dup2(pipes[i][1], STDOUT_FILENO) == -1) {
                    perror("dup2 failed for stdout");
                    exit(EXIT_FAILURE);
                }
            } else if (output_fd != STDOUT_FILENO) {
                // Last command and output_fd is specified (for logging)
                if (dup2(output_fd, STDOUT_FILENO) == -1) {
                    perror("dup2 failed for stdout to log");
                    exit(EXIT_FAILURE);
                }
            }
            
            // Close all pipe file descriptors
            for (int j = 0; j < num_commands - 1; j++) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }
            
            // Execute the command using NQP file system
            const char* cmd_name = current_cmd->argv[0];
            char cmd_absolute_path[MAX_LINE_SIZE] = {0};
            if(cwd->path[strlen(cwd->path) - 1] != '/'){
                snprintf(cmd_absolute_path, MAX_LINE_SIZE, "%s/%s", cwd->path, cmd_name);
            }else{
                snprintf(cmd_absolute_path, MAX_LINE_SIZE, "%s%s", cwd->path, cmd_name);
            }
            assert(is_valid_path(cmd_absolute_path));
            
            // Open and read the executable from the file system
            int cmd_fd = nqp_open(cmd_absolute_path);
            if (cmd_fd == NQP_FILE_NOT_FOUND) {
                fprintf(stderr, "Command not found: %s\n", cmd_name);
                exit(EXIT_FAILURE);
            }
            
            // Create a memory-backed file for the executable
            int exec_memfd = memfd_create("exec", 0);
            if (exec_memfd == -1) {
                perror("memfd_create failed");
                exit(EXIT_FAILURE);
            }
            
            // Read the executable content into memory
            char buffer[READ_BUFFER_SIZE];
            size_t bytes_read;
            while ((bytes_read = nqp_read(cmd_fd, buffer, READ_BUFFER_SIZE)) > 0) {
                if (write(exec_memfd, buffer, bytes_read) == -1) {
                    perror("write to memfd failed");
                    exit(EXIT_FAILURE);
                }
            }
            
            // Close the NQP file
            nqp_close(cmd_fd);
            
            // Rewind the memory fd to the beginning
            if (lseek(exec_memfd, 0, SEEK_SET) == -1) {
                perror("lseek failed");
                exit(EXIT_FAILURE);
            }
            
            // Execute the command
            if (fexecve(exec_memfd, current_cmd->argv, envp) == -1) {
                perror("fexecve failed");
                exit(EXIT_FAILURE);
            }
            
            // Should never reach here
            fprintf(stderr, "Exec failed\n");
            exit(EXIT_FAILURE);
        }
        
        // Store child PID for later waiting
        child_pids[i] = pid;
    }
    
    // Parent process - close all pipe file descriptors
    for (int i = 0; i < num_commands - 1; i++) {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }
    
    // Wait for all child processes to complete
    for (int i = 0; i < num_commands; i++) {
        int status;
        waitpid(child_pids[i], &status, 0);
        
        if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
            fprintf(stderr, "Command %d exited with status %d\n", i, WEXITSTATUS(status));
        }
    }
    
    return OPERATION_SUCCEED;
}
/**
 * Sets up a pipe to redirect output from the last command in a pipeline
 * and duplicate it to both stdout and a log file
 * 
 * @param pipe_cmds The pipe commands structure
 * @param cwd Current working directory
 * @param envp Environment variables
 * @param log_fd File descriptor for the log file
 * @return Status code indicating success or failure
 */
int execute_pipes_with_logging(Pipe_Commands* pipe_cmds, const Curr_Dir* cwd, char *envp[]) {
    assert(pipe_cmds != NULL);
    assert(cwd != NULL);
    assert(log_fd != LOG_DISABLED);
    
    // Create a pipe to capture output from the last command
    int log_pipe[2];
    if (pipe(log_pipe) == -1) {
        perror("Failed to create log pipe");
        return OPERATION_FAILED;
    }
    
    // Execute the pipe commands with the write end of our log pipe as output
    int result = execute_pipes(pipe_cmds, cwd, envp, log_pipe[1]);
    
    // Close write end of the pipe now that all commands have been started
    close(log_pipe[1]);
    
    // Read from the pipe and write to both stdout and log file
    char buffer[READ_BUFFER_SIZE];
    ssize_t bytes_read;
    
    while ((bytes_read = read(log_pipe[0], buffer, READ_BUFFER_SIZE)) > 0) {
        // Write to stdout
        if (write(STDOUT_FILENO, buffer, bytes_read) != bytes_read) {
            perror("Failed to write to stdout");
            close(log_pipe[0]);
            return OPERATION_FAILED;
        }
        
        // Write to log file
        if (write(log_fd, buffer, bytes_read) != bytes_read) {
            perror("Failed to write to log file");
            close(log_pipe[0]);
            return OPERATION_FAILED;
        }
    }
    
    // Close read end of the pipe
    close(log_pipe[0]);
    
    return result;
}

//PIPES RELATED ROUTINES
int calc_num_pipes_marker(const Command* cmd) {
    // Defensive checks for NULL and validity
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
    if(pipe_count > 0 && !validate_pipe_positions(cmd)){    //if pipe exist BUT not used correctly
        pipe_count = INVALID_USECASE; //treat it as pipe not used properly
    }
    return pipe_count;
}
bool validate_pipe_positions(const Command* cmd) {
    // Defensive checks
    assert(cmd != NULL && "Command pointer cannot be NULL");
    if (!command_is_valid(cmd)) return false;
    
    for (int i = 0; i < cmd->argc; i++) {
        const char* arg = command_get_arg(cmd, i);
        assert(arg != NULL && "Command argument cannot be NULL");

        if (strcmp(arg, "|") == 0) {
            
            // Check previous argument exists and is non-pipe
            if (i == 0 || !is_valid_string(command_get_arg(cmd, i-1)) || 
                strcmp(command_get_arg(cmd, i-1), "|") == 0) {
                printf("INVALID USE: of pipe operator => ");
                command_print(cmd);
                return false;
            }

            // Check next argument exists and is non-pipe
            if (i == cmd->argc-1 || !is_valid_string(command_get_arg(cmd, i+1)) || 
                strcmp(command_get_arg(cmd, i+1), "|") == 0) {
                printf("INVALID USE: of pipe operator => ");
                command_print(cmd);
                return false;
            }
        }
    }

    // If no pipes found, still considered valid
    return true;
}



//MAIN FUNCTION
int main( int argc, char *argv[], char *envp[] ){
    char line_buffer[MAX_LINE_SIZE] = {0};

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
    if (argc == 4) {    //must have 4 => ./nqp_shell root.img -o log_file.txt
        if (strcmp(argv[2], "-o") != 0) {
            fprintf(stderr, "INVALID Usage: Must be ./nqp_shell volume.img [-o log.txt]\n");
            exit(EXIT_FAILURE);
        }
        
        // Open log file
        log_fd = open(argv[3], O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (log_fd < 0) {
            perror("Failed to open log file");
            exit(EXIT_FAILURE);
        }
    }

    //TESTING
    //test_all();
    //TESTING

    //Initialise curr_dir with root directory
    Curr_Dir* cwd = construct_empty_curr_dir();
    while (true){
        char mssg[MAX_LINE_SIZE] = {0};
        snprintf(mssg, MAX_LINE_SIZE, "%s:\\> ",volume_label);
        char* line = readline("");
        custom_print(mssg);
        strncpy(line_buffer, line, MAX_LINE_SIZE);

        if (line == NULL) { // EOF (Ctrl+D pressed)
            custom_print("\n");
            break;
        }

        //BONUS PART: Adding the current command to history
        add_history(line_buffer);

        //Parse the command
        Command* new_command = command_create(line_buffer);
        assert(NULL != new_command);
        if(NULL == new_command) return EXIT_FAILURE;

        if(!command_is_valid(new_command)){
            //printf("\nINVALID COMMAND\n");
            command_destroy(new_command);
            continue;   //read next command
        }

        //check if there is any pipe in it
        const int num_pipes = calc_num_pipes_marker(new_command);
        if(num_pipes > 0){
            //printf("Pipes detected\n");

            //create the parsed pipe command arguments
            Pipe_Commands* pipe_cmds = create_Pipe_Commands(num_pipes,line_buffer);
            assert(pipe_commands_is_valid(pipe_cmds));
            if(NULL == pipe_cmds) continue;

            int return_code;
            if (log_fd != LOG_DISABLED) { // logging is enabled, and command has pipe init
                return_code = execute_pipes_with_logging(pipe_cmds, cwd, envp);
            } else { // logging is disabled, and command has pipe init
                return_code = execute_pipes(pipe_cmds, cwd, envp, STDOUT_FILENO);
            }
            
            if (return_code != OPERATION_SUCCEED) {
                fprintf(stderr, "Pipe execution failed with code: %d\n", return_code);
            }

            pipe_commands_destroy(pipe_cmds);
            command_destroy(new_command);
            continue;   //continue reading next command
        }
        if(num_pipes == INVALID_USECASE){
            printf("pipe operator not used properly\n");

            command_destroy(new_command);
            continue;
        }

        //Execute the command if pipes are not present
        if(!execute_command(new_command, cwd, envp) && 0 == num_pipes){
            assert(false);
            custom_print("Failure to execute the command:\n");
            command_print(new_command);

            command_destroy(new_command);
            continue;
        }

        //reset the line buffer for next Command
        memset(line_buffer,0,MAX_LINE_SIZE);
    }

    //free up the resources
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
    // This will print the current directory, visually verify it's "/home/some"
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

    // Test 1: Create and validate a simple command
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
