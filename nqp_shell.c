#define _GNU_SOURCE    // For fexecve
#include <sys/mman.h>  // For memfd_create
#include <unistd.h>    // For read, write, fork, lseek
#include <sys/wait.h>  // For waitpid

#include <stdio.h>
#include <stdlib.h>

#include "nqp_io.h"
#include "nqp_shell.h"

#include <assert.h>
#include <stdint.h>
#include <ctype.h>


//COMMAND RELATED CONSTANTS
#define MAX_ARGS 64
#define MAX_ARG_LENGTH 256
#define READ_BUFFER_SIZE 4096  //1024 * 4


//CURRENT DIRECTORY STRUCT
// typedef struct{
//     char path[MAX_LINE_SIZE];
// } Curr_Dir;
Curr_Dir* construct_empty_curr_dir(void){
    //create struct
    Curr_Dir* cwd = (Curr_Dir*)malloc(sizeof(Curr_Dir));
    assert(NULL != cwd);
    if(NULL == cwd) return NULL;

    //initialize with root directory path
    strncpy(cwd->path, "/", MAX_LINE_SIZE);
    assert(is_valid_path(cwd->path));

    //verify it
    assert(is_valid_curr_dir(cwd));
    if(!is_valid_curr_dir(cwd)){    //invalid struct hence free it
        destroy_curr_dir(cwd);
        return NULL;
    }

    //return it
    return cwd;
}
Curr_Dir* construct_curr_dir(char* path){
    //input validation
    assert(is_valid_path(path));
    if(!is_valid_path(path)) return NULL;

    //create struct
    Curr_Dir* cwd = malloc(sizeof(Curr_Dir));
    strncpy(cwd->path, path, MAX_LINE_SIZE);

    //verify it
    assert(is_valid_curr_dir(cwd));
    if(NULL == cwd) return NULL;
    if(!is_valid_curr_dir(cwd)){    //invalid struct hence free it
        destroy_curr_dir(cwd);
        return NULL;
    }

    //return it
    return cwd;
}
void destroy_curr_dir(Curr_Dir* cwd){
    if(NULL == cwd) return;
    free(cwd);
}
void set_path(Curr_Dir* cwd, char* path){
    assert(NULL != cwd);
    assert(NULL != path);
    assert(is_valid_path(path));
    if(NULL == cwd || NULL == path) return;
    if(is_valid_curr_dir(cwd)){
        strncpy(cwd->path, path, MAX_LINE_SIZE);
    }
}

//VALIDATORS
bool is_valid_string(char* str){
    if(NULL == str) return false;
    if(strlen(str) < 1) return false;
    if(strlen(str) > MAX_LINE_SIZE) return false;
    return true;
}
bool is_only_whitespace(const char *str) {
    while (*str) {
        if (!isspace((unsigned char)*str)) {
            return false;
        }
        str++;
    }
    return true;
}
bool is_valid_path(char* path){
    if(NULL == path) return false;
    if(strlen(path) < 1) return false;
    if(strlen(path) > MAX_LINE_SIZE) return false;
    if('/' != path[0]) return false;
    if(strlen(path) < 1) return false;
    //if('/' != path[strlen(path)-1]) return false;
    return true;
}
bool is_valid_curr_dir(Curr_Dir *cwd){
    if(NULL == cwd) return false;
    if(!is_valid_path(cwd->path)) return false;
    return true;
}


//BUILTINS
/*
command_pwd()
*/
void command_pwd(Curr_Dir *cwd) {
    assert(is_valid_curr_dir(cwd));
    if(!is_valid_curr_dir(cwd)) return;
    printf("%s\n", cwd->path);
}

/*
command_ls()
*/
void command_ls(Curr_Dir* cwd){
    assert(NULL != cwd);
    assert(is_valid_curr_dir(cwd));
    if(!is_valid_curr_dir(cwd)) return;

    nqp_dirent entry = {0};
    int fd = -1;
    ssize_t dirents_read;
    char* curr_path = cwd->path;

    //open the file
    fd = nqp_open(curr_path);
    if ( fd == NQP_FILE_NOT_FOUND ){
        fprintf(stderr, "%s not found\n", curr_path );
        return; //file open failed
    }

    //read its directory entries
    while ( ( dirents_read = nqp_getdents( fd, &entry, 1 ) ) > 0 ){
        //print its metadata
        printf( "%lu %s", entry.inode_number, entry.name );
        if ( entry.type == DT_DIR ){
            putchar('/');
        }
        putchar('\n');
        free( entry.name );
    }
    if ( dirents_read == -1 ){
        fprintf( stderr, "%s is not a directory\n", curr_path );
    }
    nqp_close( fd );
}

/*
 * command_cd()
 * ".." OR "../" OR ""..<anything>"" Takes to parent dir
 * "/" OR "/<anything>" Takes to root dir
 * "<folder>" takes to that folder in the current directory
 */
void command_cd(char* path, Curr_Dir* cwd){
    //Input validation
    assert(is_valid_string(path));
    if(!is_valid_string(path)) {
        printf("Error: Invalid path string\n");
        return;
    }
    if(strlen(path) == 0 || path[0] == ' '){
        //if path is NOT starting with a non empty character then change to root dir
        strncpy(cwd->path, "/", MAX_LINE_SIZE);
        //printf("Changed to root directory\n");
        return;
    }
    if(!is_valid_string(path)) {
        printf("Error: Invalid path string\n");
        return;
    }

    char* curr_path = cwd->path;

    //check for ".." request
    if(strncmp(path,"..",2) == 0){
        //CASE 1: Already in root dir, can't go up
        if (strcmp(cwd->path, "/") == 0) {
            //printf("Already at root directory, cannot go up\n");
            return;
        }

        //CASE 2: Parent directory exist, so change the path to 
        for(int i=strlen(curr_path)-1; i>=0; i--){
            if('/' == curr_path[i]){
                if(strcmp(curr_path, "/") != 0){
                    curr_path[i] = '\0';
                }
                break;
            }
            curr_path[i] = '\0';
        }
        //printf("Changed to parent directory: %s\n", curr_path);
    }else if(strcmp(path,"/") == 0){
        assert(strlen(path) == 1);
        //printf("Changed to root directory\n");
    }else{ //change to another directory
        //create new path
        char new_path[MAX_LINE_SIZE] = {0};
        strncpy(new_path,curr_path,MAX_LINE_SIZE);
        strcat(new_path,path);
        if(!is_valid_path(new_path)) {
            printf("Error: Invalid path %s\n", new_path);
            return;
        }

        //check if the new path exist in file system
        int fd = NQP_FILE_NOT_FOUND;
        fd = nqp_open(new_path);
        if (fd < 0) {   //folder not found
            printf("ERROR: Directory not found: %s\n", new_path);
            return;
        }

        //check if the found entry is a directory entry
        ssize_t dirents_read;
        nqp_dirent entry = {0};
        dirents_read = nqp_getdents(fd,&entry,1);
        nqp_close(fd);
    
        if (dirents_read < 0) {
            printf("ERROR: Is not a directory: %s\n", new_path);
            return;
        }
        
        // Update current working directory, if its a directory entry
        assert(is_valid_path(new_path));
        strcpy(curr_path, new_path);
        assert(is_valid_path(curr_path));
        assert(strncmp(cwd->path, new_path, MAX_LINE_SIZE) <= 0);
        //printf("Changed to directory: %s\n", curr_path);
    }
    assert(is_valid_curr_dir(cwd));
}


//COMMAND OBJECT ROUTINES
//constructor
Command* command_create(const char* input) {
    assert(input != NULL);
    if (!input) return NULL;

    Command* cmd = (Command*)malloc(sizeof(Command));
    if (!cmd) return NULL;
    
    cmd->argc = 0;
    cmd->argv = (char**)malloc(sizeof(char*) * MAX_ARGS);
    if (!cmd->argv) {
        free(cmd);
        return NULL;
    }

    char* input_copy = strdup(input);
    if (!input_copy) {
        free(cmd->argv);
        free(cmd);
        return NULL;
    }

    char* token = strtok(input_copy, " \t\n");
    while (token && cmd->argc < MAX_ARGS) {
        cmd->argv[cmd->argc] = strdup(token);
        if (!cmd->argv[cmd->argc]) {    //if any argument fails
            for (int i = 0; i < cmd->argc; i++) free(cmd->argv[i]);
            free(cmd->argv);
            free(cmd);
            free(input_copy);
            return NULL;
        }
        cmd->argc++;    //add next argument
        token = strtok(NULL, " \t\n");
    }

    free(input_copy);   //free the input copy
    return cmd;
}

//destructor
void command_destroy(Command* cmd) {
    if (!cmd) return;
    for (int i = 0; i < cmd->argc; i++) {
        free(cmd->argv[i]);
    }
    free(cmd->argv);
    free(cmd);
}

//validator
bool command_is_valid(const Command* cmd) {
    return (cmd && cmd->argv && cmd->argc > 0 && cmd->argc <= MAX_ARGS);
}

//getter
const char* command_get_arg(const Command* cmd, int index) {
    // assert(cmd != NULL);
    // assert(index >= 0);
    // assert(index < cmd->argc);
    if (!cmd || index < 0 || index >= cmd->argc) return NULL;
    return cmd->argv[index];
}

//for debugging command object
void command_print(const Command* cmd) {
    assert(cmd != NULL);
    if (!cmd) return;
    printf("argc = %d\n", cmd->argc);
    printf("argv = [");
    for (int i = 0; i < cmd->argc; i++) {
        printf("\"%s\"", cmd->argv[i]);
        if (i < cmd->argc - 1) printf(", ");
    }
    printf("]\n");
}

bool execute_command(Command* cmd, Curr_Dir* cwd, char *envp[]){
    assert(NULL != cmd);
    assert(cmd->argc >= 0);
    if(!cmd || cmd->argc < 0) return false;
    if(cmd->argc == 0) return true; //to ask user for next command
    assert(cmd->argc > 0);

    //read the command
    const char* argv_0 = command_get_arg(cmd,0);
    if(!argv_0) return false;
    char* command = strdup(argv_0);
    assert(command != NULL);
    if(!command) return false;  //failure in fetching the command

    //match it with builtins
    if(strcmp(command,"cd") == 0){  //call cd
        const char* argv_1 = command_get_arg(cmd,1);
        if(argv_1){
            char* destination = strdup(argv_1);
            assert(NULL != destination);
            command_cd(destination,cwd);
            free(destination);
            return true;
        }
    }else if(strcmp(command,"ls") == 0){  //call cd
        command_ls(cwd);
    }else if(strcmp(command,"pwd") == 0){  //call cd
        command_pwd(cwd);
    }else{
        import_command_data(cmd,cwd->path, envp);
    }

    free(command);
    return true;
}

//PROCESS RELATED ROUTINES
bool import_command_data(Command* cmd, const char* curr_path, char *envp[]){
    const char* argv_0 = command_get_arg(cmd,0);
    char* command = strdup(argv_0);
    char* path = strdup(curr_path);
    assert(is_valid_path(path));
    assert(is_valid_string(command));
    if(!is_valid_path(path) || !is_valid_string(command)){
        free(command);
        return false;
    }
    //search the command data file in current working directory
    strcat(path,command);

    //free the command string
    free(command);
    
    //open the file in nqp file system
    int nqp_fd = nqp_open(path);
    if(nqp_fd == NQP_FILE_NOT_FOUND) return false;
    assert(nqp_fd >= 0);

    //write the executable into new memory space
    int mem_fd = memfd_create("FileSystemCode", 0); 
    if(mem_fd == NQP_FILE_NOT_FOUND) {
        nqp_close(nqp_fd);
        return false;
    }
    assert(mem_fd >= 0);

    //read the file's data from the nqp file system and write in the mem_fd file
    char buffer[READ_BUFFER_SIZE] = {0};
    int bytes_read = 0;
    while( (bytes_read = nqp_read(nqp_fd, &buffer, READ_BUFFER_SIZE)) > 0){
        write(mem_fd, buffer, bytes_read);
        memset(buffer, 0, READ_BUFFER_SIZE);
    }

    //close the file in nqp file system and reset the offset marker in mem_fd file
    nqp_close(nqp_fd);  
    lseek(mem_fd, 0, SEEK_SET);

    //split the process
    pid_t pid = fork();
    if(0 == pid){   //child process
        // Execute program
        fexecve(mem_fd, cmd->argv, envp);
        printf("fexecve FAILED");
        exit(EXIT_FAILURE);
    }
    assert(pid != 0);   //trigger means fexecve failed. Currently in child process

    // Parent process
    //int status; //status code for whether waitpid was executed or not
    wait(NULL);   
    return true;
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

//MAIN FUNCTION
int main( int argc, char *argv[], char *envp[] ){
    printf("%d, %s, %s", argc, argv[0], envp[0]);
    char line_buffer[MAX_LINE_SIZE] = {0};
    char *volume_label = NULL;
    nqp_error mount_error;

    (void) envp;

    if ( argc != 2 ){
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

    //TESTING
    //test_all();
    //TESTING

    //Initialise curr_dir with root directory
    Curr_Dir* cwd = construct_empty_curr_dir();
    while ( fgets( line_buffer, MAX_LINE_SIZE, stdin ) != NULL ){
        printf( "%s:\\> ", volume_label );

        //Parse the command
        Command* new_command = command_create(line_buffer);
        assert(NULL != new_command);
        if(NULL == new_command) return EXIT_FAILURE;

        //Execute the command
        if(!execute_command(new_command, cwd, envp)){
            assert(false);
            printf("Failure to execute the command:\n");
            command_print(new_command);
            return EXIT_FAILURE;
        }
    }

    //free up the resources
    assert(NULL != cwd);
    if(NULL != cwd){
        destroy_curr_dir(cwd);
    }
    return EXIT_SUCCESS;

}

