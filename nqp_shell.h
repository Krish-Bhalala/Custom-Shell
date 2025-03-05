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

Curr_Dir* construct_empty_curr_dir(void);
Curr_Dir* construct_curr_dir(char* path);
void destroy_curr_dir(Curr_Dir* cwd);
void set_path(Curr_Dir* cwd, char* path);

//PROCESS RELATED ROUTINES
bool import_command_data(Command* cmd, const char* path, char *envp[]);

//validators
bool is_valid_string(char* str);
bool is_only_whitespace(const char *str);
bool is_valid_path(char* path);
bool is_valid_curr_dir(Curr_Dir *cwd);

//builtins
void command_cd(char* path, Curr_Dir* cwd);
void command_ls(Curr_Dir* cwd);
void command_pwd(Curr_Dir *cwd);

