#include<stdio.h>
#include<unistd.h>
#include<string.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <stdlib.h>

void shell_loop();
char **parse_for_piping(char* input, int* n);
char **parse_command(char* input);
void piping(char** commands, int n);
int launch(char **args);
int create_process_and_run(char **args);

typedef struct command{
    char* com;
    struct command* next;
    int pid;
    char start[100];
    long duration_sec;
    long duration_microsec; 
}command;

command *head = NULL;
command *tail = NULL;

void store_command(char* command1, int pid, char s[], long sec, long micro_sec){
    command *newcommand = malloc(sizeof(command));
    newcommand->com = strdup(command1);
    newcommand->pid = pid;
    strcpy(newcommand->start, s);
    newcommand->duration_sec = sec;
    newcommand->duration_microsec = micro_sec;
    newcommand->next = NULL;
    if(tail == NULL){
        head = newcommand;
        tail = newcommand;
    }else{
        tail->next = newcommand;
        tail = newcommand;
    }
}

void display_command_history(){
    command *curr = head;
    int i=1;
    while(curr!=NULL){
        printf("%d. Command: %s  PID: %d  Start Time: %s  Execution Duration: %ld sec  %ld microsec\n",i++,curr->com,curr->pid,curr->start,curr->duration_sec,curr->duration_microsec);
        curr = curr->next;
    }
}
void showHistory(){
    command *curr = head;
    int i=1;
    while(curr!=NULL){
        printf("%d. Command: %s\n",i++,curr->com);
        curr = curr->next;
    }
}
void free_command_storage(){
    command *curr = head;
    while(curr!=NULL){
        command *next = curr->next;
        free(curr->com);
        free(curr);
        curr = next;
    } 
}
void handle_sigint(int sig){
    printf("\nShell terminated. Displaying command history:\n");
    display_command_history();
    free_command_storage();
    exit(0);
}
void *read_user_input(){
    int max_input_size = 1024;
    char* input = malloc(max_input_size);
    if(fgets(input, max_input_size, stdin)==NULL){
        printf("Error in reading command");
        exit(1);
        }
    return input;
}
void shell_loop() {
    char *command;
    char **commands;
    int status;
    int n = 0;
    signal(SIGINT,handle_sigint);
    do {
        printf("user_prompt$ ");
        command = read_user_input();
        if(strcmp(command,"history\n") == 0){
            showHistory();
        }
        else{
            commands = parse_for_piping(command, &n);
            if(n>0){
                piping(commands, n);
            }
            else{
                char** args = parse_command(commands[0]);
                status = launch(args);
                free(args);
            }
            free(commands);
        }
        free(command);
    } while (status == 0);
}
char **parse_for_piping(char* input, int* n){
    char **command_set = malloc(64*sizeof(char *));
    char *com;
    int i =0;

    com = strtok(input, "|");
    while(com != NULL){
        command_set[i++] = com;
        com = strtok(NULL,"|");
    }
    command_set[i] = NULL;
    *n = i-1;
    return command_set;

}
char **parse_command(char* input){
    char **args = malloc(64 * sizeof(char *));
    char *token;
    int i = 0;

    token = strtok(input, " \t\r\n");
    while (token != NULL) {
        args[i++] = token;
        token = strtok(NULL, " \t\r\n");
    }
    args[i] = NULL;
    return args;
}
int launch(char **args){
    int status;
    status = create_process_and_run(args);
    return status;
}
int create_process_and_run(char **args){
    struct timeval start , finish;
    struct tm *time_info;
    char samay[100];
    int status;

    gettimeofday(&start, NULL);
    time_t t= start.tv_sec;
    time_info = localtime(&t);
    strftime(samay, sizeof(samay), "%Y-%m-%d %H:%M:%S", time_info);
    int pid = fork();
    if (pid < 0) {
        perror("Process creation failed");
        exit(1);
    } else if (pid == 0) {
        if (execvp(args[0], args) == -1) {
            perror("Execution failed");
            exit(1);
        }
    } else {
        if (waitpid(pid, &status, 0) == -1) {
            perror("Error handling child process");
            exit(1);
        }
        gettimeofday(&finish, NULL);
        long sec = finish.tv_sec - start.tv_sec;
        long mic_sec = ((sec * 1000000) + finish.tv_usec) - start.tv_usec;
        if (WIFEXITED(status)) {
            store_command(*args, pid, samay, sec, mic_sec);
        } else {
            printf("Process %d terminated abnormally\n", pid);
        }
    }
    return 0;
}
void piping(char** commands, int n){
    int pipe_fd[2];
    int fd_in = 0;
    int i;

    for(int i=0;i<=n;i++){
        if(i != n){
            pipe(pipe_fd);
        }
        int pid = fork();
        if(pid == 0){
            if(fd_in != 0){
                dup2(fd_in, STDIN_FILENO);
                close(fd_in);
            }
            if(i != n){
                dup2(pipe_fd[1],STDOUT_FILENO);
                close(pipe_fd[1]);
            }
            char** args = parse_command(commands[i]);
            execvp(args[0],args);
        }
        else{
            wait(NULL);
            if(i!=n){
                close(pipe_fd[1]);
                fd_in = pipe_fd[0];
            }
        }
    }
}

int main(){
    shell_loop();
    return 0;
}