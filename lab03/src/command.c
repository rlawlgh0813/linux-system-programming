#include <stdio.h>
#include <string.h>

#include "header.h"

void cmd_loop(void){
    char line[MAX_LINE];
    char *argv[MAX_ARG];
    char *token;
    int argc;

    while(1){
        // prompt
        printf("20211407> ");
		fflush(stdout);

        // input
        if(fgets(line, sizeof(line), stdin) == NULL) break;

        // continue : blank line
        line[strcspn(line,"\n")] = '\0';
		if(line[0] == '\0') continue;

        // tokenize
        argc = 0;
        token = strtok(line, " \t\n");
        while(token && argc < MAX_ARG){
            argv[argc++] = token;
            token = strtok(NULL, " \t\n");
        }
        argv[argc] = NULL;

        // call control function
		if(strcmp(argv[0], "tree") == 0){
            cmd_tree(argc, argv);
        }else if(strcmp(argv[0], "print") == 0){
            cmd_print(argc, argv);
        }else if(strcmp(argv[0], "help") == 0){
            cmd_help((argc > 1) ? argv[1] : NULL);
        }else if(strcmp(argv[0], "exit") == 0){
            break;
        }else{
            cmd_help(NULL);
        }
    }
}
