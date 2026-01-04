#include <stdio.h>
#include "header.h"

// prototype
void all_help();
void tree_help();
void print_help();
void help_help();
void exit_help();

// main command function
void cmd_help(char *arg)
{
	if(arg == NULL){
		all_help();
        printf("\n");
	}else if(strcmp(arg, "tree") == 0){
        tree_help();
        printf("\n");
    }else if(strcmp(arg, "print") == 0){
        print_help();
        printf("\n");
    }else if(strcmp(arg, "help") == 0){
        help_help();
        printf("\n");
    }else if(strcmp(arg, "exit") == 0){
        exit_help();
        printf("\n");
    }else{
        printf("invalid command -- \'asd\'\n");
        all_help();
        printf("\n");
        printf("\n");
    }
}

// all_help: Display help for all supported commands.
void all_help(){
    tree_help();
    print_help();
    help_help();
    exit_help();
}

// tree_help: Usage instructions for the 'tree' command.
void tree_help(){
    printf("  > tree <PATH> [OPTION]... : display the diractory structure if <PATH> is a directory\n");
    printf("    -r : display the directory structure recursively if <PATH> is a directory\n");
    printf("    -s : display the directory structure if <PATH> is a directory, including the size of each file\n");
    printf("    -p : display the directory structure if <PATH> is a directory, including the permissions of each directory and file\n");
}

// print_help: Usage instructions for the 'print' command.
void print_help(){
    printf("  > print <PATH> [OPTION]... : print the contents on the standard output if <PATH> is file\n");
    printf("    -n <line_number> : print only the first <line_number> lines of its contents on the standard output if <PATH> is file\n");
}

// help_help: Usage instructions for the 'help' command itself.
void help_help(){
    printf("  > help [COMMAND] : show commands for program\n");
}

// exit_help: Usage instructions for the 'exit' command.
void exit_help(){
    printf("  > exit : exit program\n");
}
