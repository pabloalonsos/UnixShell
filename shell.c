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

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>

#define MAXBUFFSIZE 2048
#define MAXCOMMSIZE 512

/*
 * Prints the PS1 environment variable as prompt or the default.
 */
void printPrompt(void);

/*
 * Allocates memory safely and throws an error if it fails
 */
void *secureMalloc(size_t sizeToAlloc, char *errMsg);

/*
 * It returns the size of an array that has NULL as tail
 */
int sizeOfArray(char **array);

/*
 * This method divides the input string of commands into an array of elements in which the following 
 * characters have their own char *:
 *  ">", ">>", ">&", "<"
 * The rest of the elements of the array are the commands to be run (e.g. ls -la, echo, etc)
 */
char **parseCommand(char *commandInput);

/*
 * It returns the position in the array of commands the position of an input redirection character
 * If it doesn't find any, it returns 0
 */
int inputPosition(char **args);

/*
 * It returns the position in the array of commands the position of an output redirection character
 * If it doesn't find any, it returns 0
 */
int outputPosition(char **args);

/*
 * It sets the output redirection of the forms ">", ">>" and ">&"
 */
void redirectOutput(char **args, int fdOut, int err, int outPos);

/*
 * It sets the output redirection of the forms "<"
 */
void redirectInput(char **args, int fdIn, int err, int inPos);

/*
 * It executes the method close() on the file descriptor passed
 */
void closeRedirect(int fd);

/*
 * It tokenizes the elements of the command. This is, if we have a command with options,
 * it will tokenize the command and then each of the options.
 */
char **getParams(char *params);

/*
 * It safely calls execvp passing the command and arguments
 */
void execute(char *args);

/*
 * It sets the necessary redirection and executes the commands parsed previously
 */
void evaluateCmd(char **cmd);

int pipePosition(char **args);

char **setPipes(char *cmds);

void pipeline(char **cmds, int pipePos);

int main(int argc, char *argv[]){

    size_t bufferSize = MAXBUFFSIZE;
    char *cmdInput;

    int hasPipe;
    char **sepCmds; //Will store each command separated by |

    printf("\nSpeak, friend, and enter [your commands]. Enter 'exit' when you are done.\n\n");

    while (1){
        // print prompt
        printPrompt();
        cmdInput = secureMalloc(bufferSize * sizeof(char), "Couldn't allocate memory for the input command");

        // get & parse command
        getline(&cmdInput, &bufferSize, stdin); // get input

        if(!strcmp(cmdInput, "exit\n")){
            free(cmdInput);
            exit(0);
        } else if (!strcmp(cmdInput, "friend")){
            printf("Hello Gandalf! long time no see. Don't get into Moria, it's a trap!\n");
        }

        sepCmds = setPipes(cmdInput);

        hasPipe = pipePosition(sepCmds);
        printf("pipe at: (%d)\n", hasPipe);
        pipeline(sepCmds, hasPipe);

        for(int i = 0; i < MAXBUFFSIZE; i++){
            sepCmds[i] = NULL;
        }

        free(cmdInput);

    }

    return 0;
}

void printPrompt(void){
    char* PS1 = getenv("PS1");
    if (PS1) printf("%s ",PS1);
    else printf("♞ :: ");
}

void *secureMalloc(size_t sizeToAlloc, char *errMsg){
    void *mem = malloc(sizeToAlloc);
    // memory allocation error check
    if(!mem){
        printf("%s\n", errMsg);
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

char **parseCommand(char *commandInput){
    char *token;
    char *tmpStr = secureMalloc(MAXCOMMSIZE * sizeof(char), "Couldn't allocate mem for temporary parsing string.");
    char **parsedArray = secureMalloc(MAXBUFFSIZE * sizeof(char*), "Couldn't allocate memory for the command parsed array.");
    int argPos = 0;

    // replace trailing \n with empty char
    /*commandInput[strlen(commandInput)-1] = '\0';*/

    // Ignore leading spaces
    while(*commandInput && (*commandInput == ' '))
        commandInput++;

    token = strtok(commandInput, " \n");

    while(token != NULL){
        if(strcmp(token, "<")==0){
            parsedArray[argPos++] = tmpStr;
            parsedArray[argPos++] = "<";
            tmpStr = secureMalloc(MAXCOMMSIZE * sizeof(char*), "Couldn't allocate mem for temporary parsing string.");
        } else if (strcmp(token, ">") == 0){
            parsedArray[argPos++] = tmpStr;
            parsedArray[argPos++] = ">";
            tmpStr = secureMalloc(MAXCOMMSIZE * sizeof(char*), "Couldn't allocate mem for temporary parsing string.");
        } else if (strcmp(token, ">>") == 0){
            parsedArray[argPos++] = tmpStr;
            parsedArray[argPos++] = ">>";
            tmpStr = secureMalloc(MAXCOMMSIZE* sizeof(char*), "Couldn't allocate mem for temporary parsing string.");
        } else if (strcmp(token, ">&") == 0){
            parsedArray[argPos++] = tmpStr;
            parsedArray[argPos++] = ">&";
            tmpStr = secureMalloc(MAXCOMMSIZE * sizeof(char*), "Couldn't allocate mem for temporary parsing string.");
        } else {
            strcat(tmpStr," ");
            strcat(tmpStr,token);
        }
        token = strtok(NULL," ");
    }

    parsedArray[argPos++] = ++tmpStr;

    return parsedArray;
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
        args++;
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
        args++;
    }
    return 0;
}

//returns first | position
int pipePosition(char **args){
    int pos = 0;
    while(*args != NULL){
        if(!strcmp(*args, "|")){
            if ((*(args+1) != NULL) && (*(args-1) != NULL)){
                return pos;
            }
        }
        pos++;
        args++;
    }
    return 0;
}

char **getParams(char *params){
    char **commandTokens = secureMalloc(MAXCOMMSIZE * sizeof(char*), "Couldn't allocate mem for temporary parsing string.");
    char *token = strtok(params, " \n");
    int arrayPos = 0;

    // We tokenize the command and arguments, and we eliminate the line-break at the end of the command.
    while(token != NULL){
        // Add the token to the array string and then gets the next argument
        commandTokens[arrayPos++] = token;
        token = strtok(NULL, " ");
    }
    commandTokens[arrayPos] = NULL;
    return commandTokens;
}


void execute(char *args){
    char **argv = getParams(args);
    if(execvp(argv[0],argv) < 0){ //execute command
        perror("Command not found.\n");
        exit(1);
    } 
}

void redirectInput(char **args, int fdIn, int err, int inPos){
    fdIn = open (args[inPos+1], O_RDONLY);
    if(fdIn < 0){
        perror("file descriptor stdin error");
        exit(1);
    }

    if (dup2(fdIn, STDIN_FILENO) < 0) perror("error dup2ing stdin");

    args[inPos] = NULL;

}

void redirectOutput(char **args, int fdOut, int err, int outPos){
    if(!strcmp(args[outPos], ">&"))
        err = 1;

    if(!strcmp(args[outPoters], ">>")){
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
}

void closeRedirect(int fd){
    if (close(fd) < 0) perror("Error closing file descriptor");
}

void evaluateCmd(char **args){

    int inPos, outPos, err = 0;
    int fdIn = 0,
        fdOut = 0;

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
                    redirectInput(args, fdIn, err, inPos);
                    redirectOutput(args, fdOut, err, outPos);

                    execute(args[0]);

                    closeRedirect(fdIn);
                    closeRedirect(fdOut);
                }
            } else if(inPos > 0){  // Input Redirection
                redirectInput(args, fdIn, err, inPos);

                execute(args[0]);

                closeRedirect(fdIn);
            } else if(outPos > 0){ // Output Redirection
                redirectOutput(args, fdOut, err, outPos);

                execute(args[0]);

                closeRedirect(fdOut);
            }
        } else {
            printf("executing: (%s)\n", args[0]);
            execute(args[0]);
        }

    } else { // Parent
        printf("waiting for ma' kid\n");
        wait(&status);
    }
}

char **setPipes(char *cmds){

    char *token;
    char *tmpStr = secureMalloc(MAXCOMMSIZE * sizeof(char), "Couldn't allocate mem for temporary parsing string.");
    char **parsedArray = secureMalloc(MAXBUFFSIZE * sizeof(char*), "Couldn't allocate memory for the parsed array.");
    int argPos = 0;

    // replace trailing \n with empty char
    cmds[strlen(cmds)-1] = '\0';

    // Ignore leading spaces
    while(*cmds && (*cmds == ' '))
        cmds++;

    token = strtok(cmds, " \n");

    while(token != NULL){
        if(strcmp(token, "|")==0){
            parsedArray[argPos++] = tmpStr;
            parsedArray[argPos++] = "|";
            tmpStr = secureMalloc(MAXCOMMSIZE * sizeof(char*), "Couldn't allocate mem for temporary parsing string.");
        } else {
            strcat(tmpStr," ");
            strcat(tmpStr,token);
        }
        token = strtok(NULL," ");
    }

    parsedArray[argPos++] = ++tmpStr;

    return parsedArray;
}

void pipeline(char **cmds, int pipePos){

    int pid, status,
        pipefd[2];

    printf("current pipe position: (%d)\n", pipePos);

    if(pipePos){
        if(pipe(pipefd)){
            perror("There was an error setting the pipes");
            exit(1);
        };

        if((pid = fork()) == 0){ // child
            //printf("child at: (%d)\n", pid);
            // copy stdout fd into pipefd & close regular stdout
            //printf("pipefd[1]: (%d)\n", pipefd[1]);
            fprintf(stderr,"STODUT_FILENO 1\n");
            dup2(pipefd[1], STDOUT_FILENO);
            fprintf(stderr, "dupped\n");
            close(pipefd[0]);
            fprintf(stderr,"evaluating kid\n");
            evaluateCmd(parseCommand(cmds[pipePos-1]));
            //cmds[pipePos] = NULL;
        } else { // parent
            cmds[pipePos] = NULL;
            //printf("waiting parent\n");
            wait(&status);
            //printf("parent at: (%d)\n", pid);
            dup2(pipefd[0], STDIN_FILENO);
            close(pipefd[1]);
            pipePos = pipePosition(cmds);
            //printf("new pipe position at (%d)\n", pipePos);
            pipeline(cmds, pipePos);
        }
    } else {
        //evaluate last command
        //printf("else?\n");
        //printf("size of array: (%d)\n",sizeOfArray(cmds));
        printf("trying to execute: (%s)\n", cmds[sizeOfArray(cmds)-1]);
        evaluateCmd(parseCommand(cmds[sizeOfArray(cmds)-1]));
    }

}


















