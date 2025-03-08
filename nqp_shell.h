#pragma once
#define MAX_LINE_SIZE 256
#include <stdbool.h>
#include <string.h>

//CURRENT DIRECTORY STRUCT
typedef struct{
    char path[MAX_LINE_SIZE];
} Curr_Dir;
Curr_Dir* construct_empty_curr_dir(void);
Curr_Dir* construct_curr_dir(const char* path);
void destroy_curr_dir(Curr_Dir* cwd);
void set_path(Curr_Dir* cwd,const char* path);

// COMMAND OBJECT
typedef struct {
    int argc;
    char** argv;
} Command;
//constructor
Command* command_create(const char* input);
//destructor
void command_destroy(Command* cmd);
//validator
bool command_is_valid(const Command* cmd);
//getter
const char* command_get_arg(const Command* cmd, int index);
//for debugging command object
void command_print(const Command* cmd);
//instance methods
bool execute_command(const Command* cmd, Curr_Dir* cwd, char *envp[]);

typedef struct{
    int num_commands;   //total number of commands in the pipe
    Command** commands; //array of commands
}Pipe_Commands;
Pipe_Commands* create_Pipe_Commands(const int num_pipes, char* line);
void pipe_commands_destroy(Pipe_Commands* pipe);
bool pipe_commands_is_valid(const Pipe_Commands* pc);
int execute_pipes(Pipe_Commands* cmd_list, const Curr_Dir* cwd, char *envp[], const int output_fd);
Command* pipe_commands_get_command_at(const Pipe_Commands* cmd_list, const int idx);

//PROCESS RELATED ROUTINES
int import_command_to_memfd(const char* path);
int import_command_data(const Command* cmd, const char* path, char *envp[]);
int handle_input_redirection(const Command* cmd, const char* cwd_path);

//PIPES RELATED ROUTINES
int calc_num_pipes_marker(const Command* cmd);
bool validate_pipe_positions(const Command* cmd);

//validators
bool is_valid_string(const char* str);
bool is_only_whitespace(const char *str);
bool is_valid_path(const char* path);
bool is_valid_curr_dir(const Curr_Dir *cwd);
void print_string_array(const char *arr[]);

//HELPERS
void trim_string(char *str);

//builtins
void command_cd(const char* path, Curr_Dir* cwd);
void command_ls(const Curr_Dir* cwd);
void command_pwd(const Curr_Dir *cwd);

