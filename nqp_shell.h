#pragma once
#define MAX_LINE_SIZE 256
#include <stdbool.h>
#include <string.h>

//CURRENT DIRECTORY STRUCT
typedef struct{
    char path[MAX_LINE_SIZE];
} Curr_Dir;

// COMMAND OBJECT
typedef struct {
    int argc;
    char** argv;
} Command;
void remove_redirection_marker(Command* cmd);

Curr_Dir* construct_empty_curr_dir(void);
Curr_Dir* construct_curr_dir(const char* path);
void destroy_curr_dir(Curr_Dir* cwd);
void set_path(Curr_Dir* cwd,const char* path);

//PROCESS RELATED ROUTINES
int import_command_data(const Command* cmd, const char* path, char *envp[]);
int handle_input_redirection(const Command* cmd, const char* cwd_path);

//validators
bool is_valid_string(const char* str);
bool is_only_whitespace(const char *str);
bool is_valid_path(const char* path);
bool is_valid_curr_dir(const Curr_Dir *cwd);
void print_string_array(const char *arr[]);

//builtins
void command_cd(const char* path, Curr_Dir* cwd);
void command_ls(const Curr_Dir* cwd);
void command_pwd(const Curr_Dir *cwd);

