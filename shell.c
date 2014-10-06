/*
 * =====================================================================================
 *
 *       Filename:  shell.c
 *
 *    Description:  C-Shell Implementation
 *
 *        Version:  1.0
 *        Created:  10/01/2014
 *       Compiler:  gcc
 *
 *         Author:  Pablo Alonso 
 *
 * =====================================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#define MAXBUFFSIZE 2048
#define MAXCOMMSIZE 512

/* 
 * Checks for the PS1 env variable and uses it if it exists as command prompt
 */
void printPrompt(void);

/* 
 * Allocates memory safely, printing an error if it fails
 */
void *secureMalloc(size_t sizeToAlloc);

char **parseCommands(char *commandInput);

/*
 * 0: none
 * 1: stdin
 * 2: stdout
 * 3: append
 */
void execute(char **argv, char* string, int io);

char **getParams(char *params);

void runCommands(char **normalizedCommands, int sizeNC);

int sizeOfArray(char **array){
    int size = 0;
    while(array[size]!=NULL) size++;
    return size;
}

int main(int argc, char *argv[]){

    size_t bufferSize = MAXBUFFSIZE;
    char **normalizedCommands;
    char *cmdInput;
    int sizeNC;

    while (1){
        // print prompt
        printPrompt();

        // get & parse command
        getline(&cmdInput, &bufferSize, stdin); // get input

        normalizedCommands = parseCommands(cmdInput);
        sizeNC = sizeOfArray(normalizedCommands);

        // execute
        runCommands(normalizedCommands, sizeNC);

    }

    return 0;
}

void printPrompt(void){
    char* PS1 = getenv("PS1");
    if (PS1) printf("%s :: ", PS1);
    else printf("♞ :: ");
}

void *secureMalloc(size_t sizeToAlloc){
    void *mem = malloc(sizeToAlloc);
    // memory allocation error check
    if(!mem){
        perror("Failed to allocate memory.");
        exit(EXIT_FAILURE);
    }
    return mem;
}

char **parseCommands(char *commandInput){

    char *token = strtok(commandInput, " \n");
    char *tmpStr = secureMalloc(100 * sizeof(char));
    char *tmpHandler = secureMalloc(10 * sizeof(char));
    char **parsedArray = secureMalloc(MAXBUFFSIZE * sizeof(char*));
    int argPos = 0;

    while(token != NULL){
        tmpHandler = strrchr (token, '\n');
        if (tmpHandler) *tmpHandler = 0;

        if(strcmp(token, "<")==0){
            parsedArray[argPos++] = tmpStr;
            parsedArray[argPos++] = "<";
            tmpStr = secureMalloc(100 * sizeof(char*));
        } else if (strcmp(token, ">") == 0){
            parsedArray[argPos++] = tmpStr;
            parsedArray[argPos++] = ">";
            tmpStr = secureMalloc(100 * sizeof(char*));
        } else if (strcmp(token, ">>") == 0){
            parsedArray[argPos++] = tmpStr;
            parsedArray[argPos++] = ">>";
            tmpStr = secureMalloc(100 * sizeof(char*));
        } else {
            strcat(tmpStr,token);
            strcat(tmpStr," ");
        }
        token = strtok(NULL," ");
    }
    parsedArray[argPos++] = tmpStr;

    char **normalizedCommands = secureMalloc(argPos * sizeof(char*));
    for(size_t i=0; i<argPos; i++){
        normalizedCommands[i] = parsedArray[i];
    }

    free(parsedArray);
    return normalizedCommands;
}

char **getParams(char *params){
    char **commandTokens = secureMalloc(MAXCOMMSIZE * sizeof(char*));
    char *token = strtok(params, " \n");
    int arrayPos = 0;
    char *tmp = secureMalloc(10 * sizeof(char));

    // We tokenize the command and arguments, and we eliminate the line-break at the end of the command.
    while(token != NULL){
        // Remove the end-line chars:
        tmp = strrchr (token, '\n');
        if (tmp) *tmp = 0;

        // Add the token to the array string and then gets the next argument
        printf("part: (%s)\n",token);
        commandTokens[arrayPos++] = token;
        token = strtok(NULL, " ");
    }
    return commandTokens;
}

void execute(char **argv, char *filename, int io){
    int pid, status, fd;
    pid = fork();
    // At this point I check if my PID is 0 (child) or if it's not 0 (parent)
    if(pid == 0){ // if I am the child
        switch(io){
            case 0:
                break;
            case 1: // stdin
                printf("filename: (%s)\n", filename);
                fd = open (filename, O_RDONLY);
                if(fd < 0){
                    perror("file descriptor stdin error");
                    exit(1);
                }
                if (dup2(fd, STDIN_FILENO) < 0) perror(NULL);
                if (close(fd) < 0) perror(NULL);
            case 2: // stdout
                fd = open (filename, O_WRONLY|O_CREAT|O_TRUNC, 0644);
                if(fd < 0){
                    perror("file descriptor stdout replace error");
                    exit(1);
                }
                if (dup2(fd, STDOUT_FILENO) < 0) perror(NULL);
                if (close(fd) < 0) perror(NULL);
            case 3: // append out
                fd = open (filename, O_WRONLY|O_CREAT|O_APPEND, 0644);
                if(fd < 0){
                    perror("file descriptor stdout append error");
                    exit(1);
                }
                if (dup2(fd, STDOUT_FILENO) < 0) perror(NULL);
                if (close(fd) < 0) perror(NULL);
            default:
                break;
        }
        execvp(*argv, argv);
        exit(1);
    } else { // If I am the parent, wait until my child is done executing
        wait(&status);
        free(argv);
    }
}

/*
 * 0: none
 * 1: stdin
 * 2: stdout
 * 3: append
 */
void runCommands(char **normalizedCommands, int sizeNC){

    int commandNum = 0;
    char **tmpArgv;

    if(normalizedCommands[commandNum+1]!=NULL){
        if(normalizedCommands[commandNum+2]!=NULL){
            tmpArgv = getParams(normalizedCommands[commandNum]);
            if(strcmp(normalizedCommands[commandNum+1], "<")==0){
                execute(tmpArgv, normalizedCommands[commandNum+2], 1);
            } else if(strcmp(normalizedCommands[commandNum+1], ">")==0){
                execute(tmpArgv, normalizedCommands[commandNum+2], 2);
            } else if(strcmp(normalizedCommands[commandNum+1], ">>")==0){
                execute(tmpArgv, normalizedCommands[commandNum+2], 3);
            }
        } else {
            perror("syntax error");
            exit(2);
        }
    } else {
        tmpArgv = getParams(normalizedCommands[commandNum]);
        execute(tmpArgv, NULL, 0);
    }

}
