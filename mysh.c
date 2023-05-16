#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <string.h>
#include <fcntl.h>
#include <linux/limits.h>
#include "arraylist.h"
#ifndef BUFSIZE
#define BUFSIZE 512
#endif
#ifndef ALSIZE
#define ALSIZE 100
#endif
#ifndef DEBUG
#define DEBUG 0
#endif

/*
 * Personal implementation of a command line shell
 * Authors: Sean M. Patrick & Fulton R. Wilcox
 */

void interpret(char *cmdline, array_list *al, array_list *wildcard_al);  
void process_Custom_Executable(array_list *al);
void processInput(array_list *list);
int processWildcard(array_list *wildcard_al, char *wildcard_token);
void pwd();
void changeDir(char *path);
void cleanUp(char *cmdline);
void IOLoop();
int searchCommands(array_list *al);
void execute(char** args, int numArgs);
void callExec(char** args);
char* getFileType(char *file_name);
char* getFileEndPattern(char *file_name, int patternLength);
char* getFileEnd(char *file_name, int patternLength);
char* getFileStartPattern(char *file_name, int patternLength);
char* getFileName(char *file_name);
int isExecutable(char *file_name);
void handleWildcardMatch(int absolutePath, char *file_name, char *path, array_list *wildcard_al);
int containsWildcard(char *cmdstring);
int containsHomeDirShortcut(char *cmdstring);
char* specialHandlingMemCopy(char* src, int size);

array_list al, wildcard_al;
int fin, bytes, start = 0, end = 0, validCommand = 1, cmdline_size = 0, count = 0, exit_status = 1, special_handling = 0, special_handling_index = 512;
int saved_stdin, saved_stdout;
char *cmdline;
char *cmdstring;
char *home_path;
char buffer[BUFSIZE];
char *prompt = "mysh> ";
char *vanilla_paths[6] = {"/usr/local/sbin/", "/usr/local/bin/", "/usr/sbin/", "/usr/bin/", "/sbin/", "/bin/"};

int main(int argc, char **argv){
    home_path = getenv("HOME");
    //detects if input is from stdinput or textfile 
    if (argc > 1) {
        fin = open(argv[1], O_RDONLY);
        if (fin == -1) {
            perror(argv[1]);
            exit(EXIT_FAILURE);
        }
    } else {
        fin = 0;
    }
    if(!fin){
        printf("Welcome to Sean & Robbie's shell!\n");
        fputs(prompt, stderr);
    }
    memset(buffer, 0, BUFSIZE);
    IOLoop();
    return EXIT_SUCCESS;
}

/*
 * Takes pointer to tokenized arraylist as argument.  
 * Self-implemented functions (cd, exit, pwd) are checked first and executed if they are a match.
 * Otherwise, first argument is searched for a '/' character which indicates that it is an executable,
 * and calls process_Custom_Executable if this is the case.
 * Otherwise, searchCommands is called to search through batch commands and returns if searchCommands returns true.
 * Otherwise, the command is undefined and an error is thrown.
 */
void processInput(array_list *al) {
    if(get_length(al) == 0 ) return;
    if(DEBUG) {
        for(int i=0; i< get_length(al); i++) {
            if(strcmp("exit", al->data[i]) == 0) exit(EXIT_SUCCESS);
            fprintf(stderr, "|%s| ", al->data[i]);
        }
        return;
    }
    if(strcmp(al->data[0], "exit") == 0) {
        exit(EXIT_SUCCESS);
    }
    else if(strcmp(al->data[0], "pwd") == 0) {
        if(get_length(al) == 1) {pwd(); return;}
        else {fprintf(stderr, "error: too many arguments\n"); exit_status = 0; return;}
    }
    else if(strcmp(al->data[0], "cd") == 0) {
        if(get_length(al) > 2) {fprintf(stderr, "error: too many arguments\n"); exit_status = 0; return;}
        else if(get_length(al) < 2 || strcmp("", al->data[1]) == 0) {changeDir(home_path); return;}
        else if(get_length(al) == 2) {
            changeDir(al->data[1]);
            return;
        }
    }
    else {
        for(int i=0; i<strlen(al->data[0]); i++) {
            if(al->data[0][i] == '/') {process_Custom_Executable(al); return;}
        }
    }
    if(searchCommands(al)) return;
    fprintf(stderr, "error: undefined command: "); 
    for(int i=0; i<get_length(al); i++) {fprintf(stderr, "%s ", al->data[i]);} 
    fprintf(stderr, "\n");
    exit_status = 0;
    return;
}

/*
 * Prints the path of the working directory by calling getcwd()
 */
void pwd() {
    char path[PATH_MAX];
    fprintf(stderr, "%s\n", getcwd(path, PATH_MAX));
}

/*
 * Takes in a string that is the desired directory, and changes the working directory if path is a valid path
 */
void changeDir(char *path) {
    if(chdir(path) == -1) {
        printf("No such file or directory\n");
        exit_status = 0;
    }
    return;
}

/*
 * Main input/output loop of the shell
 * Uses POSIX function read() to read data from standard input which is the command line for the shell,
 * and builds a string representing  the command line until no more data is present in the command line.
 * Calls the function interpret() when the command line is fully parsed and ready to be tokenized
 */
void IOLoop(){
    while(((bytes = read(fin, buffer, BUFSIZE)) > 0)){
        //if (DEBUG) fprintf(stderr, "read %d bytes\n", bytes);
        if(count == 0){
            cmdline_size = bytes;
            cmdline = malloc(sizeof(char) * cmdline_size);
            memcpy(cmdline, buffer, cmdline_size);
            count ++;
        }
        else{
            int temp = cmdline_size;
            cmdline_size += bytes;
            cmdline = realloc(cmdline, cmdline_size);
            memcpy(cmdline + temp, buffer, bytes);
            count ++;
        }
        if(bytes < BUFSIZE){
            interpret(cmdline, &al, &wildcard_al);
            cleanUp(cmdline);
        }
        else if(bytes == BUFSIZE && buffer[BUFSIZE - 1] == '\n'){
            interpret(cmdline, &al, &wildcard_al);
            cleanUp(cmdline);
        }
    }
}

/*
 * Input tokenizer
 * Iterates through buffer array which contains user input, characters of interest include spaces, newlines, pipes, redirects.
 * If any token containing a path contains the home directory shortcut "~", the "~" will be replaced with the user's home directory.
 * If any token is a wildcard (contains a "*"), wildcard expansion is performed and the wildcard arguments are added to the arraylist,
 * replacing the original token. If no matches were found during wildcard expansion, the token will be passed unchanged.
 * A token is pushed into an arraylist containing all previous tokens.
 * If a newline character is detected, processInput() is called to execute the command.
 */
void interpret(char *cmdline, array_list *al, array_list *wildcard_al){
    init(al, ALSIZE);
    for(int i = 0; i < cmdline_size; i ++){
        if(((cmdline[i] == ' ' || cmdline[i] == '\n' || cmdline[i] == '|' || cmdline[i] == '<' || cmdline[i] == '>') && !special_handling) || (special_handling_index >= start && cmdline[i] == '\n')){
            end = i;
            if(special_handling_index < start || special_handling_index == 512) {
                cmdstring = malloc(sizeof(char) * ((end - start) + 1));
                memcpy(cmdstring, cmdline + start, end - start);
                cmdstring[end - start] = '\0';
            } else {
                cmdstring = specialHandlingMemCopy(cmdline + start, end - start);
            }
            if(strcmp(cmdstring, "") != 0){
                if(containsHomeDirShortcut(cmdstring)){
                    char *path = malloc((strlen(home_path) + 1) * sizeof(char));
                    path[strlen(home_path)] = '\0';
                    strcpy(path, home_path);
                    char *temp = malloc(sizeof(char));
                    int current_size = 1;
                    for(int j = 1; cmdstring[j] != '\0'; j ++){
                        temp[j - 1] = cmdstring[j];
                        temp = realloc(temp, ++current_size);
                    }
                    temp[current_size - 1] = '\0';
                    path = realloc(path, (strlen(home_path) + 1 + strlen(temp)));
                    strcat(path, temp);
                    free(temp);
                    free(cmdstring);
                    cmdstring = path;
                }
                if(containsWildcard(cmdstring)){
                    if(processWildcard(wildcard_al, cmdstring)){
                        for(int j = 0; j < get_length(wildcard_al); j ++){
                            push(al, wildcard_al->data[j]);
                        }
                        destroy(wildcard_al);
                    }
                    else{
                        destroy(wildcard_al);
                        push(al, cmdstring);
                    }
                }
                else{
                    push(al, cmdstring);
                }
            }
            start = i + 1;
            if(cmdline[i] == '\n' && !special_handling) {
                processInput(al); 
                    if(exit_status) prompt = "mysh> ";
                    else prompt = "!mysh> ";
                    if(!fin) fputs(prompt, stderr);
                    exit_status = 1;
                destroy(al);
                if(fin == 0) {free(cmdstring); return;}
                init(al, ALSIZE);
            }
            else if(cmdline[i] == '|' || cmdline[i] == '<' || cmdline[i] == '>') {
                char cmdstring[2] = {cmdline[i], '\0'};
                push(al, cmdstring);
            }
            free(cmdstring);
            //if(cmdline[i+1] == 0) break;        this was causing problems
        }
        if(cmdline[i] == '\\' && special_handling_index != i-1) {special_handling = 1; special_handling_index = i;}
        if(special_handling_index == i-1) special_handling = 0;
    }
    destroy(al);
    return;
}

/*
 * Searches through paths "/usr/local/sbin/", "/usr/local/bin/", "/usr/sbin/", "/usr/bin/", "/sbin/", "/bin/"
 * in that order for a batch command, populates array with arguments
 * and executes using execute function if it is found.
 * Returns false if nothing was executed and true if something was executed.
 */
int searchCommands(array_list *al) {
    int found = 0;
    //puts arguments into an array so it can be pass into execv
    int numArgs = get_length(al);
    char* arguments[numArgs+1];
    arguments[numArgs] = NULL;
    for(int i=0; i<numArgs; i++) {
        arguments[i] = malloc(strlen(al->data[i])+1);
        strcpy(arguments[i], al->data[i]);
    }

    struct stat pfile;
    for(int i=0; i<6; i++) {
        char* path = malloc(strlen(vanilla_paths[i]) + strlen(al->data[0]) + 2);
        strcpy(path, vanilla_paths[i]);
        strcat(path, al->data[0]);
        if(stat(path, &pfile) != -1) {
            found = 1;
            free(arguments[0]);
            arguments[0] = path;
            execute(arguments, numArgs);
            for(int i=0; i<numArgs; i++) {
                free(arguments[i]);
            }
            return found;
        }
        if(DEBUG) printf(" %s ", path);
        free(path);
    }
    for(int i=0; i<numArgs; i++) {
        free(arguments[i]);
    }
    return 0;
}

/*
 * Checks executable using stat to verify existence of executable, 
 * returns failure and throws error if executable does not exist.
 * Otherwise, argument array is populated and passed into execute function.
 */
void process_Custom_Executable(array_list *al) {
    if(strcmp(al->data[0], "/") == 0) {
        fprintf(stderr, "%s: no such file or directory\n", al->data[0]);
        exit_status = 0;
        return;
    }
    struct stat pfile;
    for(int i=0; i<get_length(al); i++) {
        for(int j=0; j<strlen(al->data[i]); j++) {
            if(al->data[i][j] == '/' || i == 0 || (i != 0 && strcmp(al->data[i-1], "|") == 0)) {
                if(stat(al->data[i], &pfile) == -1) {
                    fprintf(stderr, "%s: no such file or directory\n", al->data[i]);
                    exit_status = 0;
                    return;
                }
                else break;
            }
        }
    }
    int numArgs = get_length(al);
    char* arguments[numArgs+1];
    arguments[numArgs] = NULL;
    for(int i=0; i<numArgs; i++) {
        arguments[i] = malloc(strlen(al->data[i])+1);
        strcpy(arguments[i], al->data[i]);
    }
    execute(arguments, numArgs);
    for(int i=0; i<numArgs; i++) {
        free(arguments[i]);
    }
    return;
}

/*
 * Main function to execute executables after setting input and output source.
 * I/O can be changed between calls depending on the user input (pipes/multiple redirects)
 * callExec is called when input and output is set appropriately and executable is executed.
 * When execution is finished, saved stdin and stdout is restored.
 */
void execute(char** arguments, int numArgs) {
    int inputRedirect = 0, outputRedirect = 0, piping = 0, inputRedirectIndex = 0, outputRedirectIndex = 0, pipingIndex = 0;
    int fds[2];
    for(int i = 0; i < numArgs; i ++){
        if(strcmp(arguments[i], "<") == 0) {
            if(piping == 1) {
                fprintf(stderr, "error: cannot redirect input and pipe\n");
                return;
                exit_status = 0;
            }
            inputRedirect = 1; 
            inputRedirectIndex = i;
            struct stat pfile;
            if(stat(arguments[inputRedirectIndex + 1], &pfile) == -1) { exit_status = 0; fprintf(stderr, "error: no such file"); return;}
        }
        else if(strcmp(arguments[i], ">") == 0) {
            if(outputRedirect == 1) {
                fprintf(stderr, "error: cannot redirect output and pipe\n");
                return;
                exit_status = 0;
            }
            outputRedirect = 1;
            outputRedirectIndex = i;
        }
        else if(strcmp(arguments[i], "|") == 0) {
            if(outputRedirect == 1) {
                fprintf(stderr, "error: cannot redirect output and pipe\n");
                return;
                exit_status = 0;
            }
            piping = 1; 
            pipingIndex = i; 
            if(pipe(fds) == -1){ 
                exit_status = 0; 
                return;
            }
        }
        //fds[0] - read end  fds[1] - write end
    }
    if(inputRedirect && !piping){
        int fd = open(arguments[inputRedirectIndex + 1], O_RDONLY);
        if(fd == -1){ exit_status = 0; return;}
        saved_stdin = dup(STDIN_FILENO);
        dup2(fd, STDIN_FILENO); // STDIN_FILENO (0) - file descriptor for stdin
        close(fd);
    }
    if(outputRedirect && !piping){
        int fd = open(arguments[outputRedirectIndex + 1], O_WRONLY | O_CREAT | O_TRUNC, 0640);
        if(fd == -1){ exit_status = 0; return;}
        saved_stdout = dup(STDOUT_FILENO);
        dup2(fd, STDOUT_FILENO); // STDOUT_FILENO (1) - file descriptor for stdout
        close(fd);
    }
    if(piping){
            saved_stdout = dup(STDOUT_FILENO);
            dup2(fds[1], STDOUT_FILENO);
            saved_stdin = dup(STDIN_FILENO);
            dup2(fds[0], STDIN_FILENO);
    }
    if((inputRedirect && !outputRedirect && !piping) || (!inputRedirect && outputRedirect && !piping)){ //only input redirection or output redirection
        char *newArgs[numArgs - 1];
        newArgs[numArgs - 2] = NULL;
        for(int i = 0; i < numArgs - 2; i ++){
            newArgs[i] = malloc(strlen(arguments[i]) + 1);
            strcpy(newArgs[i], arguments[i]);
        }
        callExec(newArgs);
        for(int i = 0; i < numArgs - 2; i ++){
            free(newArgs[i]);
        }
        if(inputRedirect){
            dup2(saved_stdin, STDIN_FILENO);
            close(saved_stdin);
        }
        else{
            dup2(saved_stdout, STDOUT_FILENO);
            close(saved_stdout);
        }
    }
    else if(inputRedirect && outputRedirect && !piping){ //only input and output redirection
        char *newArgs[numArgs - 3];
        newArgs[numArgs - 4] = NULL;
        for(int i = 0; i < numArgs - 4; i ++){
            newArgs[i] = malloc(strlen(arguments[i]) + 1);
            strcpy(newArgs[i], arguments[i]);
        }
        callExec(newArgs);
        for(int i = 0; i < numArgs - 4; i ++){
            free(newArgs[i]);
        }
        exit_status = 0;
        dup2(saved_stdin, STDIN_FILENO);
        dup2(saved_stdout, STDOUT_FILENO);
        close(saved_stdin);
        close(saved_stdout);
    }
    else if(inputRedirect && !outputRedirect && piping){ //input redirection and piping
        char* args1[pipingIndex-1];
        for(int i=0; i<pipingIndex-2; i++) {
            args1[i] = malloc(strlen(arguments[i])+1);
            strcpy(args1[i], arguments[i]);
        }
        args1[pipingIndex-2] = NULL;

        char* args2[numArgs-pipingIndex];
        args2[numArgs-pipingIndex-1] = NULL;
        for(int i=pipingIndex+1; i<numArgs; i++) {
            args2[i-(pipingIndex+1)] = malloc(strlen(arguments[i])+1);
            strcpy(args2[i-(pipingIndex+1)], arguments[i]);
        }

        int fd = open(arguments[inputRedirectIndex + 1], O_RDONLY);
        if(fd == -1){ exit_status = 0; return;}
        dup2(fd, STDIN_FILENO); // STDIN_FILENO (0) - file descriptor for stdin

        callExec(args1);
        close(fd);
        dup2(fds[0], STDIN_FILENO);
        dup2(saved_stdout, STDOUT_FILENO);
        close(saved_stdout);

        callExec(args2);
        close(fds[0]);
        close(fds[1]);
        for(int i=0; i<pipingIndex-1; i++) free(args1[i]);
        for(int i=0; i<numArgs-1-pipingIndex; i++) free(args2[i]);
        dup2(saved_stdin, STDIN_FILENO);
        close(saved_stdin);

    }
    else if(!inputRedirect && outputRedirect && piping){ //output redirection and piping
        char* args1[pipingIndex+1];
        for(int i=0; i<pipingIndex; i++) {
            args1[i] = malloc(strlen(arguments[i])+1);
            strcpy(args1[i], arguments[i]);
        }
        args1[pipingIndex] = NULL;

        char* args2[numArgs-2-pipingIndex];
        args2[numArgs-2-pipingIndex-1] = NULL;
        for(int i=pipingIndex+1; i<numArgs-2; i++) {
            args2[i-(pipingIndex+1)] = malloc(strlen(arguments[i])+1);
            strcpy(args2[i-(pipingIndex+1)], arguments[i]);
        }
        callExec(args1);
        int fd = open(arguments[outputRedirectIndex + 1], O_WRONLY | O_CREAT | O_TRUNC, 0640);
        if(fd == -1){ exit_status = 0; return;}
        dup2(fd, STDOUT_FILENO); // STDOUT_FILENO (1) - file descriptor for stdout
        close(fd);

        callExec(args2);
        dup2(saved_stdout, STDOUT_FILENO);
        close(saved_stdout);
        close(fds[0]);
        close(fds[1]);
        for(int i=0; i<pipingIndex+1; i++) free(args1[i]);
        for(int i=0; i<numArgs-2-pipingIndex; i++) free(args2[i]);
        dup2(saved_stdin, STDIN_FILENO);
        close(saved_stdin);

    }
    else if(!inputRedirect && !outputRedirect && piping){ //only piping
        char* args1[pipingIndex+1];
        for(int i=0; i<pipingIndex; i++) {
            args1[i] = malloc(strlen(arguments[i])+1);
            strcpy(args1[i], arguments[i]);
        }
        args1[pipingIndex] = NULL;

        char* args2[numArgs-pipingIndex];
        args2[numArgs-pipingIndex-1] = NULL;
        for(int i=pipingIndex+1; i<numArgs; i++) {
            args2[i-(pipingIndex+1)] = malloc(strlen(arguments[i])+1);
            strcpy(args2[i-(pipingIndex+1)], arguments[i]);
        }

        callExec(args1);
        dup2(saved_stdout, STDOUT_FILENO);
        close(saved_stdout);
        callExec(args2);
        close(fds[0]);
        close(fds[1]);
        for(int i=0; i<pipingIndex+1; i++) free(args1[i]);
        for(int i=0; i<numArgs-pipingIndex; i++) free(args2[i]);
        dup2(saved_stdin, STDIN_FILENO);
        close(saved_stdin);
    }
    else if(inputRedirect && outputRedirect && piping){ //input/output redirection and piping
        char* args1[pipingIndex-1];
        for(int i=0; i<pipingIndex-2; i++) {
            args1[i] = malloc(strlen(arguments[i])+1);
            strcpy(args1[i], arguments[i]);
        }
        args1[pipingIndex-2] = NULL;
        int fd = open(arguments[inputRedirectIndex + 1], O_RDONLY);
        if(fd == -1){ exit_status = 0; return;}
        dup2(fd, STDIN_FILENO); // STDIN_FILENO (0) - file descriptor for stdin
        close(fd);
        callExec(args1);

        char* args2[numArgs-2-pipingIndex];
        args2[numArgs-2-pipingIndex-1] = NULL;
        for(int i=pipingIndex+1; i<numArgs-2; i++) {
            args2[i-(pipingIndex+1)] = malloc(strlen(arguments[i])+1);
            strcpy(args2[i-(pipingIndex+1)], arguments[i]);
        }
        int fd2 = open(arguments[outputRedirectIndex + 1], O_WRONLY | O_CREAT | O_TRUNC, 0640);
        if(fd2 == -1){ exit_status = 0; return;}
        dup2(fd2, STDOUT_FILENO); // STDOUT_FILENO (1) - file descriptor for stdout
        close(fd2);
        dup2(fds[0], STDIN_FILENO);
        callExec(args2);
        for(int i=0; i<numArgs-2-pipingIndex; i++) free(args2[i]);
        for(int i=0; i<pipingIndex-1; i++) free(args1[i]);
        dup2(saved_stdin, STDIN_FILENO);
        close(saved_stdin);
        dup2(saved_stdout, STDOUT_FILENO);
        close(saved_stdout);
    }
    else{ 
        callExec(arguments);
    }
    return;
}

/*
 * Calls fork to create a child process, then calles execv to execute if a child process was successfully created.
 * Returns after child process is completed or fork/execv fails.
 */
void callExec(char** args) {
    int process = fork();
    if(process == -1) {fprintf(stderr, "error: cannot execute"); exit_status = 0; return;}
    if(process == 0) {
        execv(args[0], args);
    }
    int wstatus;
    int tpid = wait(&wstatus);
    if(DEBUG) printf("%d", tpid);
    if (WIFEXITED(wstatus)) {
        // child exited normally
        if(DEBUG) printf("child exited with %d\n", WEXITSTATUS(wstatus));
    }
    return;
}

/*
 * Takes pointer to string represening the parsed command line
 * Resets variables and frees necessary data associated with building the parsed command line
 */
void cleanUp(char *cmdline){ //used to reset and free data to prepare for next input command
    start = 0;
    end = 0;
    count = 0;
    cmdline_size = 0;
    free(cmdline);
}

/*
 * Takes pointer to an arraylist specifically for building the expansion of the wildcard, 
 * and the wildcard string token itself as arguments
 * Returns 1 if the wildcard expansion was successful and file matches were found, or 0 if no files match the wildcard pattern
 */
int processWildcard(array_list *wildcard_al, char *wildcard_token){
    DIR *dp;
    struct dirent *de;
    char path[PATH_MAX];
    int matches_found = 0, containsDot = 0, absolutePath = 0, absolutePathEndIndex = 0, star_index = 0;
    for(int i = 0; i < strlen(wildcard_token); i ++){
        if(wildcard_token[i] == '.'){containsDot = 1; break;}
    }
    for(int i = strlen(wildcard_token) - 1; i >= 0; i --){
        if(wildcard_token[i] == '/') {absolutePath = 1; absolutePathEndIndex = i; break;}
    }
    if(absolutePath){
        memcpy(path, wildcard_token, absolutePathEndIndex + 1);
        path[absolutePathEndIndex + 1] = '\0';
        char *new_wildcard_token = malloc(sizeof(char) * (strlen(wildcard_token) - absolutePathEndIndex));
        new_wildcard_token[strlen(wildcard_token) - absolutePathEndIndex - 1] = '\0';
        memcpy(new_wildcard_token, wildcard_token + absolutePathEndIndex + 1, strlen(wildcard_token) - absolutePathEndIndex - 1);
        wildcard_token = new_wildcard_token;
    }
    else{
        getcwd(path, PATH_MAX);
    }
    for(int i = 0; i < strlen(wildcard_token); i ++){
        if(wildcard_token[i] == '*') {star_index = i; break;}
    }
    init(wildcard_al, ALSIZE);
    if(wildcard_token[0] == '*' && wildcard_token[1] == '.'){ //matching files of same type (*.txt) (works)
        char *file_type = malloc((strlen(wildcard_token + 1) + 1) * sizeof(char));
        strcpy(file_type, wildcard_token+1);
        dp = opendir(path);
        if(dp == NULL) {destroy(wildcard_al); return 0;}
        while((de = readdir(dp)) != NULL){
            char *type = getFileType(de->d_name);
            if(type == NULL) continue;
            if(strcmp(type, file_type) == 0){
                matches_found = 1;
                handleWildcardMatch(absolutePath, de->d_name, path, wildcard_al);
            }
            free(type);
        }
        closedir(dp);
        free(file_type);
    }
    else if(wildcard_token[0] == '*' && strlen(wildcard_token) == 1){ //matching all files (*) (works)
        dp = opendir(path);
        if(dp == NULL) {destroy(wildcard_al); return 0;}
        while((de = readdir(dp)) != NULL){
            if((strlen(de->d_name) == 1 && de->d_name[0] == '.') || (strlen(de->d_name) == 2 && strcmp(de->d_name, "..") == 0) || (de->d_name[0] == '.')) continue;
            matches_found = 1;
            handleWildcardMatch(absolutePath, de->d_name, path, wildcard_al);
        }
        closedir(dp);
    }
    else if(wildcard_token[0] == '*' && wildcard_token[1] != '.' && !containsDot){ //matching files ending with pattern (*bar) (works)
        char *endPattern = malloc((sizeof(char)) * (strlen(wildcard_token)));
        strcpy(endPattern, wildcard_token + 1);
        dp = opendir(path);
        if(dp == NULL) {destroy(wildcard_al); return 0;}
        while((de = readdir(dp)) != NULL){
            char *fileEndPattern = getFileEnd(de->d_name, strlen(wildcard_token) - 1);
            if(fileEndPattern == NULL) continue;
            if(strcmp(endPattern, fileEndPattern) == 0){
                matches_found = 1;
                handleWildcardMatch(absolutePath, de->d_name, path, wildcard_al);
            }
            free(fileEndPattern);
        }
        closedir(dp);
        free(endPattern);
    }
    else if(wildcard_token[strlen(wildcard_token) - 1] == '*'){ //matching files starting with pattern (foo*) (works)
        char *startPattern = malloc(sizeof(char) * (strlen(wildcard_token)));
        startPattern[strlen(wildcard_token) - 1] = '\0';
        memcpy(startPattern, wildcard_token, strlen(wildcard_token) - 1);
        dp = opendir(path);
        if(dp == NULL) {destroy(wildcard_al); return 0;}
        while((de = readdir(dp)) != NULL){
            char *fileStartPattern = getFileStartPattern(de->d_name, strlen(wildcard_token) - 1);
            if(fileStartPattern == NULL) continue;
            if(strcmp(startPattern, fileStartPattern) == 0){
                matches_found = 1;
                handleWildcardMatch(absolutePath, de->d_name, path, wildcard_al);
            }
            free(fileStartPattern);
        }
        closedir(dp);
        free(startPattern);
    }
    else if(wildcard_token[0] != '*' && wildcard_token[strlen(wildcard_token) - 1] != '*' && !containsDot){ //matching files starting and ending with pattern (foo*bar) (works)
        char *startPattern = malloc(sizeof(char) * (star_index + 1));
        startPattern[star_index] = '\0';
        memcpy(startPattern, wildcard_token, star_index);
        char *endPattern = malloc(sizeof(char) * (strlen(wildcard_token) - star_index));
        endPattern[strlen(wildcard_token) - star_index - 1] = '\0';
        memcpy(endPattern, wildcard_token + star_index + 1, strlen(wildcard_token) - star_index - 1);
        dp = opendir(path);
        if(dp == NULL) {destroy(wildcard_al); return 0;}
        while((de = readdir(dp)) != NULL){
            char *fileStartPattern = getFileStartPattern(de->d_name, strlen(startPattern));
            char *fileEndPattern = getFileEnd(de->d_name, strlen(endPattern));
            if(fileStartPattern == NULL || fileEndPattern == NULL) {
                if(fileStartPattern != NULL) free(fileStartPattern);
                if(fileEndPattern != NULL) free(fileEndPattern);
                continue;
            }
            if((strcmp(startPattern, fileStartPattern) == 0) && (strcmp(endPattern, fileEndPattern) == 0)){
                matches_found = 1;
                handleWildcardMatch(absolutePath, de->d_name, path, wildcard_al);
            }
            free(fileStartPattern);
            free(fileEndPattern);
        }
        closedir(dp);
        free(startPattern);
        free(endPattern);
    }
    else if(wildcard_token[0] == '*' && wildcard_token[1] != '.' && containsDot){ //matching files ending with pattern of same type (*bar.txt) (works)
        char *endPattern = malloc(strlen(wildcard_token) * sizeof(char));
        endPattern[strlen(wildcard_token) - 1] = '\0';
        strcpy(endPattern, wildcard_token + 1);
        int patternLength = 0;
        for(int i = 1; i < strlen(wildcard_token); i ++){
            if(wildcard_token[i] == '.') break;
            patternLength ++;
        }
        dp = opendir(path);
        if(dp == NULL) {destroy(wildcard_al); return 0;}
        while((de = readdir(dp)) != NULL){
            if(isExecutable(de->d_name)) continue;
            char *fileEndPattern = getFileEndPattern(de->d_name, patternLength);
            char *file_type = getFileType(de->d_name);
            if(fileEndPattern == NULL || file_type == NULL){ 
                if(fileEndPattern != NULL) free(fileEndPattern);
                if(file_type != NULL) free(file_type);
                continue;
            }
            fileEndPattern = realloc(fileEndPattern, strlen(fileEndPattern) + strlen(file_type) + 1);
            strcat(fileEndPattern, file_type);
            if(strcmp(endPattern, fileEndPattern) == 0){
                matches_found = 1;
                handleWildcardMatch(absolutePath, de->d_name, path, wildcard_al);
            }
            free(fileEndPattern);
            free(file_type);
        }
        closedir(dp);
        free(endPattern);
    }
    else if(star_index != 0 && wildcard_token[star_index + 1] == '.'){ //matching files starting with pattern of same type (foo*.txt) (works)
        char *startPattern = malloc(sizeof(char) * (star_index + 1));
        startPattern[star_index] = '\0';
        memcpy(startPattern, wildcard_token, star_index);
        char *type = malloc(sizeof(char) * (strlen(wildcard_token) - star_index));
        type[strlen(wildcard_token) - star_index - 1] = '\0';
        strcpy(type, wildcard_token + star_index + 1);
        int patternLength = strlen(startPattern);
        dp = opendir(path);
        if(dp == NULL) {destroy(wildcard_al); return 0;}
        while((de = readdir(dp)) != NULL){
            if(isExecutable(de->d_name)) continue;
            char *fileStartPattern = getFileStartPattern(de->d_name, patternLength);
            char *file_type = getFileType(de->d_name);
            if(fileStartPattern == NULL || file_type == NULL){
                if(fileStartPattern != NULL) free(fileStartPattern);
                if(file_type != NULL) free(file_type);
                continue;
            }
            if((strcmp(startPattern, fileStartPattern) == 0) && (strcmp(type, file_type) == 0)){
                matches_found = 1;
                handleWildcardMatch(absolutePath, de->d_name, path, wildcard_al);
            }
            free(fileStartPattern);
            free(file_type);
        }
        closedir(dp);
        free(startPattern);
        free(type);
    }
    else if(star_index != 0 && wildcard_token[star_index + 1] != '.' && containsDot){ //matching files starting and ending with pattern of same type (foo*bar.txt) (works)
        char *startPattern = malloc(sizeof(char) * (star_index + 1));
        startPattern[star_index] = '\0';
        memcpy(startPattern, wildcard_token, star_index);
        char *endPattern = malloc(sizeof(char) * (strlen(wildcard_token) - star_index));
        endPattern[strlen(wildcard_token) - star_index - 1] = '\0';
        strcpy(endPattern, wildcard_token + star_index + 1);
        int startPatternLength = strlen(startPattern), endPatternLength = 0;
        for(int i = star_index + 1; i < strlen(wildcard_token); i ++){
            if(wildcard_token[i] == '.') break;
            endPatternLength ++;
        }
        dp = opendir(path);
        if(dp == NULL) {destroy(wildcard_al); return 0;}
        while((de =readdir(dp)) != NULL){
            if(isExecutable(de->d_name)) continue;
            char *fileStartPattern = getFileStartPattern(de->d_name, startPatternLength);
            char *fileEndPattern = getFileEndPattern(de->d_name, endPatternLength);
            char *file_type = getFileType(de->d_name);
            if(fileStartPattern == NULL || fileEndPattern == NULL || file_type == NULL) {
                if(fileStartPattern != NULL) free(fileStartPattern);
                if(fileEndPattern != NULL) free(fileEndPattern);
                if(file_type != NULL) free(file_type);
                continue;
            }
            fileEndPattern = realloc(fileEndPattern, strlen(fileEndPattern) + strlen(file_type) + 1);
            strcat(fileEndPattern, file_type);
            if((strcmp(startPattern, fileStartPattern) == 0) && (strcmp(endPattern, fileEndPattern) == 0)){
                matches_found = 1;
                handleWildcardMatch(absolutePath, de->d_name, path, wildcard_al);
            }
            free(fileStartPattern);
            free(fileEndPattern);
            free(file_type);
        }
        closedir(dp);
        free(startPattern);
        free(endPattern);
    }
    else if(star_index != 0 && wildcard_token[strlen(wildcard_token) - 1] == '*' && wildcard_token[strlen(wildcard_token) - 2] == '.'){ //matching files of any type with same name (foo.*) (works)
        char *name = getFileName(wildcard_token);
        dp = opendir(path);
        if(dp == NULL){destroy(wildcard_al); return 0;}
        while((de = readdir(dp)) != NULL){
            char *file_name = getFileName(de->d_name);
            if(strcmp(name, file_name) == 0){
                matches_found = 1;
                handleWildcardMatch(absolutePath, de->d_name, path, wildcard_al);
            }
            free(file_name);
        }
        closedir(dp);
        free(name);
    }
    if(absolutePath){
        free(wildcard_token);
    }
    return matches_found;
}

/*
 * Takes a pointer to a string representing the name of a specific file as an argument
 * Returns a string representing the type of file, or NULL if the file is an executable,
 * a special directory entry ("." or ".."), or a hidden file
 */
char* getFileType(char *file_name){
    if(strlen(file_name) <= 2) return NULL;
    if(isExecutable(file_name)) return NULL;
    int i;
    for(i = 0; i < strlen(file_name); i ++){
        if(file_name[i] == '.') break;
    }
    int size = strlen(file_name) - i;
    char *type = malloc((size + 1) * sizeof(char));
    strcpy(type, file_name + i);
    return type; 
}

/*
 * Takes pointer to string representing the name of a file,
 * and an integer representing the length of the file ending pattern to check for as arguments
 * Ignores the file extension
 * Returns string representing the ending pattern of the file,
 * or NULL if the ending pattern length is larger than the ending pattern of the file
 */
char* getFileEndPattern(char *file_name, int patternLength){
    int i;
    for(i = 0; i < strlen(file_name); i ++){
        if(file_name[i] == '.') break;
    }
    char *endPattern = malloc((patternLength + 1) * sizeof(char));
    endPattern[patternLength] = '\0';
    for(int j = i - patternLength, k = 0; j < i; j ++, k ++){
        if(j < 0) {free(endPattern); return NULL;}
        endPattern[k] = file_name[j];
    }
    return endPattern;
}

/*
 * Takes pointer to string representing name of a file, and an integer representing the length of file end pattern to check for as arguments
 * Includes the file extension as part of the file name
 * Returns string representing ending pattern of the file,
 * or NULL if ending pattern is larger than file ending pattern, or special directory entries
 */
char* getFileEnd(char *file_name, int patternLength){
    if((strlen(file_name) == 1 && file_name[0] == '.') || (strlen(file_name) == 2 && strcmp(file_name, "..") == 0) || (strlen(file_name) < patternLength)){
        return NULL;
    }
    int fileLength = strlen(file_name);
    char *end = malloc((patternLength + 1) * sizeof(char));
    end[patternLength] = '\0';
    for(int j = fileLength - 1, k = strlen(end) - 1; j > fileLength - 1 - patternLength; j --, k --){
        end[k] = file_name[j];
    }
    return end;
}

/*
 * Takes pointer to string representing the name of a file, 
 * and an integer representing the length of the file ending pattern to check for as arguments
 * Returns string representing the ending pattern of the file,
 * or NULL if the ending pattern length is larger than the ending pattern of the file
 */
char* getFileStartPattern(char *file_name, int patternLength){
    if((strlen(file_name) == 1 && file_name[0] == '.') || (strlen(file_name) == 2 && strcmp(file_name, "..") == 0)){
        return NULL;
    }
    char *startPattern = malloc((patternLength + 1) * sizeof(char));
    startPattern[patternLength] = '\0';
    for(int i = 0; i < patternLength; i ++){
        startPattern[i] = file_name[i];
    }
    return startPattern;
}

/*
 * Takes pointer to string representing name of a file as an argument
 * Returns a string representing the name of the file, ignoring the file extension,
 * or NULL if special directory entry or executable file because the function
 * is only used when searching for file matches of a specific file type (non-executable)
 */
char* getFileName(char *file_name){
    if((strlen(file_name) == 1 && file_name[0] == '.') || (strlen(file_name) == 2 && strcmp(file_name, "..") == 0)){
        return NULL;
    }
    if(isExecutable(file_name)) return NULL;
    int i;
    for(i = 0; i < strlen(file_name); i ++){
        if(file_name[i] == '.') break;
    }
    char *fileName = malloc(sizeof(char) * (i + 1));
    fileName[i] = '\0';
    memcpy(fileName, file_name, i);
    return fileName;
}

/*
 * Takes pointer to string representing name of a file as an argument
 * Returns 1 if the file is an executable, 0 otherwise
 */
int isExecutable(char *file_name){
    int executable = 1;
    for(int i = 0; i < strlen(file_name); i ++){
        if(file_name[i] == '.') {executable = 0; break;}
    }
    return executable;
}

/*
 * Takes int representing if an absolute path is present in wildcard token, pointer to string of file name,
 * pointer to string of desired path of directory to go through,
 * and pointer to wildcard arraylist as arguments
 * Pushes desired file name into the wildcard arraylist when a match is found, including the absolute path when necessary
 */
void handleWildcardMatch(int absolutePath, char *file_name, char *path, array_list *wildcard_al){
    if(!absolutePath){
        push(wildcard_al, file_name);
    }
    else{
        char tempPath[PATH_MAX];
        strcpy(tempPath, path);
        strcat(tempPath, file_name);
        push(wildcard_al, tempPath);
    }
}

/*
 * Takes pointer to string representing parsed command line as argument
 * Returns 1 if a token is a wildcard (contains a "*"), 0 otherwise
 */
int containsWildcard(char *cmdstring){
    int wildcard = 0;
    for(int i = 0; i < strlen(cmdstring); i ++){
        if(cmdstring[i] == '*') {wildcard = 1; break;}
    }
    return wildcard;
}

/*
 * Takes pointer to string representing parsed command line as argument
 * Returns 1 if a token contains a path starting with "~/", 0 otherwise
 */
int containsHomeDirShortcut(char *cmdstring){
    if(strlen(cmdstring) < 2) return 0;
    if(cmdstring[0] == '~' && cmdstring[1] == '/') return 1;
    return 0;
}

/*
 * Replaces memcpy used in tokenizing the command when escape characters are used.
 * Adjusts the size needed to store the new characters from the deletion of '\' characters and
 * copies token as appropriate with the spacial handling of escape characters.
 * Returns the new token to be used in interpret().
 */
char* specialHandlingMemCopy(char* src, int size) {
    int specialH = 0;
    int specialchars = 0;
    if (*src == '\\') {
        specialchars += 1;
        specialH = 1;
    }
    for(int i = 1; i < size; i++) if(*(src + i) == '\\' && *(src + i - 1) != '\\') specialchars += 1;
    int size2 = size - specialchars;
    char *str = malloc((sizeof(char) * size2) + 1);
    str[size2] = '\0';
    int index = 0;
    for(int i = 0; i < size; i++) {
        if(i == 0 && *(src + i) == '\\') specialH = 1;
        else if(i != 0 && *(src + i - 1) != '\\' && *(src + i) == '\\') specialH = 1;
        else if(specialH) {
            if(*(src+i) == '\n') continue;
            str[index] = *(src + i);
            index++;
            specialH = 0;
        }
        else {
            str[index] = *(src + i);
            index++;
            specialH = 0;
        }
    }
    return str;
}
