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
#include <sys/wait.h>

#define MAXBUFFSIZE 2048
#define MAXCOMMSIZE 512

void printPrompt(void);
void *secureMalloc(size_t sizeToAlloc);
int sizeOfArray(char **array);
char **parseCommands(char *commandInput);
int inputPosition(char **args);
int outputPosition(char **args);
char **getParams(char *params);
void execute(char *args);
void runCommands(char **normalizedCommands);

int main(int argc, char *argv[]){

    size_t bufferSize = MAXBUFFSIZE;
    char **normalizedCommands;
    char *cmdInput;

    while (1){
        // print prompt
        printPrompt();
        cmdInput = secureMalloc(bufferSize * sizeof(char));

        // get & parse command
        getline(&cmdInput, &bufferSize, stdin); // get input

        if(!strcmp(cmdInput, "exit\n")){
            free(cmdInput);
            exit(0);
        }

        normalizedCommands = parseCommands(cmdInput);

        // execute
        runCommands(normalizedCommands);

        free(cmdInput);

    }

    return 0;
}

void printPrompt(void){
    char* PS1 = getenv("PS1");
    if (PS1) printf("%s ",PS1);
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

int sizeOfArray(char **array){
    int size = 0;
    while(array[size]!=NULL) size++;
    return size;
}

char **parseCommands(char *commandInput){
    char *token;
    char *tmpStr = secureMalloc(100 * sizeof(char));
    char **parsedArray = secureMalloc(MAXBUFFSIZE * sizeof(char*));
    int argPos = 0;

    // replace trailing \n with empty char
    commandInput[strlen(commandInput)-1] = '\0';

    // Ignore leading spaces
    while(*commandInput && (*commandInput == ' '))
        commandInput++;

    token = strtok(commandInput, " \n");

    while(token != NULL){
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
            strcat(tmpStr," ");
            strcat(tmpStr,token);
        }
        token = strtok(NULL," ");
    }

    parsedArray[argPos++] = ++tmpStr;

    char **normalizedCommands = secureMalloc(argPos * sizeof(char*));
    for(size_t i=0; i<argPos; i++){
        normalizedCommands[i] = parsedArray[i];
    }

    free(parsedArray);
    return normalizedCommands;
}

// Returns position of stdin redirect token '<' or 0 otherwise
int inputPosition(char **args){
    int pos = 0;
    while(*args != NULL){
        if(!strcmp(*args, "<")){
            // Check syntax: token must have arguments at both sides
            if((*(args+1) != NULL) && (*(args-1) != NULL))
                return pos;
        }
        pos++;
        *args++;
    }
    return 0;
}

int outputPosition(char **args){
    int pos = 0;
    while(*args != NULL){
        if((!strcmp(*args,">")) || (!strcmp(*args,">>")) || (!strcmp(*args,">&"))){
            // Check syntax: token must have arguments at both sides
            if((*(args+1) != NULL) && (*(args-1) != NULL))
                return pos;
        }
        pos++;
        *args++;
    }
    return 0;
}

char **getParams(char *params){
    char **commandTokens = secureMalloc(30 * sizeof(char*));
    char *token = strtok(params, " \n");
    int arrayPos = 0;

    // We tokenize the command and arguments, and we eliminate the line-break at the end of the command.
    while(token != NULL){
        // Add the token to the array string and then gets the next argument
        commandTokens[arrayPos++] = token;
        token = strtok(NULL, " ");
    }
    return commandTokens;
}


void execute(char *args){
    char **argv = getParams(args);
    printf("Running command: (%s)\n",argv[0]);
    if(execvp(argv[0],argv) < 0){ //execute command
        perror("Command not found.\n");
        exit(1);
    } 
}

void runCommands(char **args){

    int inPos, outPos, err = 0;
    int fdIn, fdOut;

    int pid, status;

    if((pid = fork()) == 0) {

        if(sizeOfArray(args)>1){
            inPos = inputPosition(args);
            outPos = outputPosition(args);

            // I/O redirection
            if(inPos && outPos){
                // stdout must go before stdin
                if(inPos > outPos){
                    printf("Please use simple I/O redirection\n");
                } else {
                    // we need to redirect error to if the redirection is '>&'
                    if(!strcmp(args[outPos], ">&"))
                        err = 1;

                    printf("fdIn: (%s)\n", args[inPos+1]);
                    fdIn = open (args[inPos+1], O_RDONLY);
                    if(fdIn < 0){
                        perror("file descriptor stdin error");
                        exit(1);
                    }

                    if(!strcmp(args[outPos], ">>")){
                        fdOut = open(args[outPos+1], O_WRONLY|O_CREAT|O_APPEND, 0644);
                        if(fdOut < 0){
                            perror("file descriptor stdout append error");
                            exit(1);
                        }
                    } else {
                        fdOut = open (args[outPos+1], O_WRONLY|O_CREAT|O_TRUNC, 0644);
                        if(fdOut < 0){
                            perror("file descriptor stdout replace error");
                            exit(1);
                        }
                    }

                    if (dup2(fdIn, STDIN_FILENO) < 0) perror("error dup2ing stdin");
                    if (dup2(fdOut, STDOUT_FILENO) < 0) perror("error dup2ing stdout");
                    if(err)
                        if (dup2(fdOut, STDERR_FILENO) < 0) perror("error dup2ing stderr");

                    args[inPos] = NULL;

                    execute(args[0]);

                    if (close(fdIn) < 0) perror("Error closing input file descriptor");
                    if (close(fdOut) < 0) perror("Error closing output file descriptor");
                }
            } else if(inPos > 0){  // Input Redirection
                fdIn = open (args[inPos+1], O_RDONLY);
                if(fdIn < 0){
                    perror("file descriptor stdin error");
                    exit(1);
                }

                if (dup2(fdIn, STDIN_FILENO) < 0) perror("error dup2ing stdin");

                args[inPos] = NULL;

                execute(args[0]);

                if (close(fdIn) < 0) perror("Error closing input file descriptor");

            } else if(outPos > 0){ // Output Redirection
                if(!strcmp(args[outPos], ">&"))
                    err = 1;

                if(!strcmp(args[outPos], ">>")){
                    fdOut = open(args[outPos+1], O_WRONLY|O_CREAT|O_APPEND, 0644);
                    if(fdOut < 0){
                        perror("file descriptor stdout append error");
                        exit(1);
                    }
                } else {
                    fdOut = open (args[outPos+1], O_WRONLY|O_CREAT|O_TRUNC, 0644);
                    if(fdOut < 0){
                        perror("file descriptor stdout replace error");
                        exit(1);
                    }
                }

                if (dup2(fdOut, STDOUT_FILENO) < 0) perror("error dup2ing stdout");
                if(err)
                    if (dup2(fdOut, STDERR_FILENO) < 0) perror("error dup2ing stderr");

                args[outPos] = NULL;

                execute(args[0]);

                if (close(fdOut) < 0) perror("Error closing output file descriptor");
            }
        } else {
            execute(args[0]);
        }

    } else { // Parent
        wait(&status);
        free(args);
    }
}
