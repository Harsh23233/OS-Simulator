#include<stdio.h>
#include<unistd.h>
#include<string.h>
#include<sys/wait.h>
#include<sys/time.h>
#include<stdlib.h>

void shell_loop();
char **parse_for_piping(char* input, int* n);
int parse_command(char* input, char** args, int* bg);
void piping(char** commands, int n);
int launch(char** args, int bg, char *com, char* full_com);
int create_process_and_run(char** args, int background, char* com, char* full_com);

//struct to model command
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

//linkedlist to store command details for history
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
//at time of termination
void display_shellHistory(){
    command *curr = head;
    int i=1;
    while(curr!=NULL){
        printf("%d. Command: %s  PID: %d  Start Time: %s  Execution Duration: %ld sec  %ld microsec\n",i++,curr->com,curr->pid,curr->start,curr->duration_sec,curr->duration_microsec);
        curr = curr->next;
    }
}
//when history cmd is called
void showHistory(){
    command *curr = head;
    int i=1;
    while(curr!=NULL){
        printf("%d. Command: %s\n",i++,curr->com);
        curr = curr->next;
    }
}
//free history after termination
void free_shellHistory(){
    command *curr = head;
    while(curr!=NULL){
        command *next = curr->next;
        free(curr->com);
        free(curr);
        curr = next;
    } 
}
//struct to model background processes
typedef struct bg_process {
    int pid;
    struct timeval start;
    char *command;
    struct bg_process *next;
} bg_process;

bg_process *bg_head = NULL;

//to store background processes
void add_bgProcess(int pid, struct timeval start, char *command) {
    bg_process *new_bg = malloc(sizeof(bg_process));
    new_bg->pid = pid;
    new_bg->start = start;
    new_bg->command = strdup(command);
    new_bg->next = bg_head;
    bg_head = new_bg;
}
//check status of background processes
void check_bgProcess() {
    bg_process *prev = NULL;
    bg_process *curr = bg_head;
    struct timeval finish;
    int status;

    while (curr != NULL) {
        int res = waitpid(curr->pid, &status, WNOHANG); 
        if (res == 0) {
            prev = curr;
            curr = curr->next;
        } else if (res == curr->pid) {
            gettimeofday(&finish, NULL);
            long sec = finish.tv_sec - curr->start.tv_sec;
            long mic_sec = ((sec * 1000000) + finish.tv_usec) - curr->start.tv_usec;

            command *com = head;  
            while (com != NULL) {
                if (com->pid == curr->pid) {
                    com->duration_sec = sec;
                    com->duration_microsec = mic_sec;
                    break;
                }
                com = com->next;
            }
            if (prev == NULL) {
                bg_head = curr->next;
            } else {
                prev->next = curr->next;
            }
            free(curr->command);
            free(curr);
            curr = (prev == NULL) ? bg_head : prev->next;
        } else {
            perror("Error checking background process");
        }
    }
}
//handle termination signal
void handle_SIGINT(int sig){
    printf("\nShell terminated. Command history:\n");
    display_shellHistory();
    free_shellHistory();
    exit(0);
}

void *read_user_input(){
    int max_input_size = 1024;
    char* input = malloc(max_input_size);
    if(fgets(input, max_input_size, stdin)==NULL){
        printf("Error in reading prompt");
        exit(1);
    }
    return input;
}
void shell_loop() {
    char *command;
    char *full_command;
    char **commands;
    int status;
    int n = 0;
    signal(SIGINT,handle_SIGINT);
    do {
        printf("user_prompt$ ");
        command = read_user_input();
        full_command = strdup(command);
        check_bgProcess();
        
        if (strcmp(command, "history\n") == 0) {
            showHistory();
        }
        else {
            int bg = 0; 
            commands = parse_for_piping(command, &n);
            if (n > 0) {
                piping(commands, n);
            }
            else {
                char **args = malloc(64 * sizeof(char *));
                parse_command(commands[0], args, &bg);
                status = launch(args, bg,command,full_command);               
                free(args);
            }
            free(commands);
        }
        free(command);
    } while (status == 0);
}
//count no. of pipes and return connected processes as a set
char **parse_for_piping(char* input, int* n){
    char **command_set = malloc(64*sizeof(char *));
    char *com;
    int i =0;
    char *temp = input; 
    com = strtok(input, "|");
    while(com != NULL){
        command_set[i++] = com;
        com = strtok(NULL,"|");
    }
    input = temp;
    command_set[i] = NULL;
    *n = i-1;
    return command_set;

}
//separates command and argument
int parse_command(char* input, char** args, int* bg) {
    int i = 0;
    *bg = 0;

    char* token = strtok(input, " \t\r\n");
    while (token != NULL) {
        if (strcmp(token, "&") == 0) {
            *bg = 1;  
        } else {
            args[i++] = token;
        }
        token = strtok(NULL, " \t\r\n");
    }
    args[i] = NULL; 
    return i;
}

int launch(char **args, int bg, char *com, char *full_com) {
    
    int status;
    status = create_process_and_run(args, bg, com, full_com);
    return status;
}

int create_process_and_run(char **args, int bg, char *com, char *full_com) {
    struct timeval start, finish;
    struct tm *time;
    char time_list[100];
    int status;

    gettimeofday(&start, NULL);
    time_t t = start.tv_sec;
    time = localtime(&t);
    strftime(time_list, sizeof(time_list), "%Y-%m-%d %H:%M:%S", time);
    
    int pid = fork();
    if (pid < 0) {
        printf("Failed to fork\n");
        exit(1);
    } else if (pid == 0) {
        if (execvp(args[0], args) == -1) {
            printf("Invalid Prompt\n");
            exit(1);
        }
    } else {
        if (bg) {
            printf("Process initiated in background with PID: %d\n", pid);
            add_bgProcess(pid, start, full_com);
            store_command(full_com, pid, time_list, 0,0);

        } else {
            if (waitpid(pid, &status, 0) == -1) {
                perror("Error handling child process");
                exit(1);
            }
            gettimeofday(&finish, NULL);
            long sec = finish.tv_sec - start.tv_sec;
            long mic_sec = ((sec * 1000000) + finish.tv_usec) - start.tv_usec;
            if (WIFEXITED(status)) {
                store_command(full_com,pid,time_list,sec,mic_sec);
            } else {
                printf("Invalid termination of Process %d\n", pid);
            }
        }
    }
    return 0;
}
//handle piping if pipe commands exist
void piping(char** commands, int n) {
    int pipe_fd[2];
    int fd_in = 0;       
    int bg = 0; 
    char* args[64];      

    for (int i = 0; i <= n; i++) {
        if (i != n) {
            pipe(pipe_fd);  
        }

        int pid = fork();
        if (pid == 0) {  
            if (fd_in != 0) {
                dup2(fd_in, STDIN_FILENO);  
                close(fd_in);
            }
            if (i != n) {
                dup2(pipe_fd[1], STDOUT_FILENO);  
                close(pipe_fd[1]);
            }
            parse_command(commands[i], args, &bg);
            execvp(args[0], args);
            perror("exec failed");
            exit(EXIT_FAILURE);
        } else { 
            if (!bg) {
                wait(NULL);  
            }
            if (i != n) {
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