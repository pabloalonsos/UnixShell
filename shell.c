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
 *       TODO:      UNCOMMENT FORK IN EVALUATECMD?????
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

void pipeline(char **cmds);

int main(int argc, char *argv[]){

    size_t bufferSize = MAXBUFFSIZE;
    char *cmdInput;

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
        } else if (!strcmp(cmdInput, "friend\n")){
            printf("Hello Gandalf! long time no see. Don't get into Moria, it's a trap!\n");
        }

        sepCmds = setPipes(cmdInput);

        pipeline(sepCmds);

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
}

void closeRedirect(int fd){
    if (close(fd) < 0) perror("Error closing file descriptor");
}

void evaluateCmd(char **args){

    int inPos, outPos, err = 0;
    int fdIn = 0,
        fdOut = 0;

    int pid, status;

    /*if((pid = fork()) == 0) {*/

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

    /*} else { // Parent*/
        /*printf("waiting for ma' kid\n");*/
        /*wait(&status);*/
    /*}*/
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
    parsedArray[argPos] = NULL;

    return parsedArray;
}

int getCommandNum(char **cmds){
    int count = 0;

    for(size_t i=0; i<sizeOfArray(cmds); i++){
        if(strcmp(cmds[i],"|") != 0){
            count++;
        }
    }
    return count;
}

void closeAllPipefds(int pipefds[][2], int size){
    for(int i=0; i<size; i++){
        close(pipefds[i][0]);
        close(pipefds[i][1]);
    }
}

void pipeline(char **cmds){

    int pipePos,
        commandNum = getCommandNum(cmds),
        pipefds[commandNum][2],
        isFirstCmd = 1,
        pid, status,
        sizeCmds = sizeOfArray(cmds);

    pipePos = pipePosition(cmds);

    if(pipePos){
        if((pid=fork()) == 0){
            for(int i=0; i<commandNum; i++){

                fprintf(stderr, "a\n");
                pipePos = pipePosition(cmds);

                if(isFirstCmd){

                    pipe(pipefds[i]);
                    if((pid = fork()) == 0){

                        close(pipefds[i][0]); // close unnecesary end of pipe
                        dup2(pipefds[i][1], STDOUT_FILENO);
                        close(pipefds[i][1]);
                        evaluateCmd(parseCommand(cmds[0]));
                        //close(pipefds[i][1]); //parent closes output

                    } else {
                        /*for(int i=0; i<commandNum; i++){*/
                            /*close(pipefds[i][0]);*/
                            /*close(pipefds[i][1]);*/
                        /*}*/

                        //if(waitpid(pid,&status,0)<0) //parent waits for child to finish
                        //    perror("wait foreground: wait pid error");
                    }
                    isFirstCmd = 0;

                } else if( i == (commandNum-1)){
                    pipe(pipefds[i]);
                    if((pid = fork()) == 0){

                        close(pipefds[i][1]); // close unnecesary end of pipe
                        dup2(pipefds[i][0], STDIN_FILENO);
                        close(pipefds[i][0]); //parent closes output
                        evaluateCmd(parseCommand(cmds[sizeCmds-1]));

                    } else {
                        /*for(int i=0; i<commandNum; i++){*/
                            /*close(pipefds[i][0]);*/
                            /*close(pipefds[i][1]);*/
                        /*}*/

                        //if(waitpid(pid,&status,0)<0) //parent waits for child to finish
                        //    perror("wait foreground: wait pid error");
                    }


                } else {

                    pipe(pipefds[i]);
                    /*fprintf(stderr, "am i getting here?1\n");*/

                    if((pid=fork()) == 0){

                        dup2(pipefds[i][0], STDIN_FILENO);
                        dup2(pipefds[i][1], STDOUT_FILENO);

                        close(pipefds[i][0]);
                        close(pipefds[i][1]);
                        evaluateCmd(parseCommand(cmds[pipePos-1]));
                        //close(pipefds[i][0]); //parent closes output
                        //close(pipefds[i][1]); //parent closes output


                    } else {
                        /*for(int i=0; i<commandNum; i++){*/
                            /*close(pipefds[i][0]);*/
                            /*close(pipefds[i][1]);*/
                        /*}*/

                        //if(waitpid(pid,&status,0)<0) //parent waits for child to finish
                        //    perror("wait foreground: wait pid error");
                    }
                }

                cmds[pipePos] = NULL;
            }

        } else {
            fprintf(stderr, "stuck here?\n");
            for(int i=0; i<commandNum; i++){
                close(pipefds[i][0]);
                close(pipefds[i][1]);
            }
            waitpid(-1, NULL, 0);
            fprintf(stderr, "yeah I'm stuck\n");

        }
    } else {
        if((pid=fork()) == 0){
            evaluateCmd(parseCommand(cmds[0])); // evaluate last command
        } else {
            wait(&status);
        }
    }


}
