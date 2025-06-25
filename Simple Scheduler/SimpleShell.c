#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <fcntl.h>     // For O_* constants
#include <sys/mman.h>  
#include <semaphore.h>  // For init
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <stdbool.h>
#include "SimpleScheduler.h"
#include <time.h>
#include <wait.h>
#include <errno.h>
#include <sys/time.h>

#define SHM_NAME "my_shm"

int scheduler_pid;      // Store Scheduler's PID
int shm_fd_1;
Shared_queue* shared_queue;
int* number_of_jobs;
int shm_fd_2;
int status = 1;
typedef struct complete_queue{
    command* head;
    command* tail;
}complete_queue;
complete_queue* Complete_queue;
bool is_command_valid(char *input) {
    int pipe_count = 0;
    char *token = input;
    while ((token = strchr(token, '|')) != NULL) {
        pipe_count++;
        token++;
    }
    if (pipe_count > 0) {
        printf("Invalid command: Pipes are not allowed.\n");
        return false;
    }
    return true;
}

// Initialize shared resources
void initialize_shared_resources() {
    shm_fd_1 = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (shm_fd_1 == -1) {
        perror("Shared memory creation failed");
        exit(1);
    }
    if (ftruncate(shm_fd_1, 2046) == -1) {
        perror("Shared memory resizing failed");
        exit(1);
    }
    printf("sem opening\n");
    shared_queue = (Shared_queue*) mmap(NULL, 2046, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd_1, 0);
    if (shared_queue == MAP_FAILED) {
        perror("Shared memory mapping failed");
        exit(1);
    }
    shared_queue->head = NULL;
    shared_queue->tail = NULL;
    shm_fd_2 = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT | 0666);
    if (shm_fd_2 < 0)
    {
        perror("Failed to create shared memory segment");
        exit(EXIT_FAILURE);
    }
    number_of_jobs = (int *)shmat(shm_fd_2, NULL, 0);
    if (number_of_jobs == (void *)-1)
    {
        perror("Failed to attach shared memory segment");
        shmctl(shm_fd_2, IPC_RMID, NULL); // Cleanup if attachment failed
        exit(EXIT_FAILURE);
    }    
}

// Cleanup shared resources
void cleanup_shared_resources() {
    munmap(shared_queue, 2046);
    close(shm_fd_1);
    shm_unlink(SHM_NAME);
    shmctl(shm_fd_2, IPC_RMID, NULL);
}


// Parse the job name from input
char* parse_job_name(char *input) {
    strtok(input, " ");
    return strtok(NULL, "\n");
}

long get_current_time_ms() {
    struct timeval time;
    gettimeofday(&time, NULL);
    return time.tv_sec * 1000 + time.tv_usec / 1000;
}





void addToHistory(char* name, pid_t p_pid, int wait_time, int comp_time)
{
    // Allocate memory for the new job
    command *new_job = (command *)malloc(sizeof(command));
    if (!new_job) {
        perror("Failed to allocate memory for new job");
        return;
    }

    // Initialize the new job
    new_job->pid = p_pid;
    new_job->completion_time = comp_time;
    new_job->wait_time = wait_time;
    strcpy(*(new_job->name), name);
    new_job->next = NULL;


    if (Complete_queue->head == NULL) {
        // Queue is empty, initialize head and tail
        Complete_queue->head = new_job;
        Complete_queue->tail = new_job;
    } else {
        // Add to the end of the queue
        Complete_queue->tail->next = new_job;
        Complete_queue->tail = new_job;
    }
}


void Completed_jobs()
{
    int status;
    pid_t pid;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0)
    {
        if (WIFEXITED(status) || WIFSIGNALED(status)){        
            command *prev = NULL;
            command *current = shared_queue->head;

            // Traverse the queue to find the job with the matching PID
            while (current != NULL)
            {
                if (current->pid == pid)
                {
                    current->end_time = get_current_time_ms();
                    current->completion_time = current->end_time - current->start_time;
                    if (current->completion_time % TSLICE != 0) {
                        current->completion_time = ((current->completion_time / TSLICE) + 1) * TSLICE;
                    }                    
                    current->wait_time = current->completion_time - current->burst_time;

                    // Add the job to history
                    addToHistory(*(current->name), current->pid, current->wait_time, current->completion_time);
                    
                    // Update current job count
                    (*number_of_jobs)--;

                    // Remove the job node from the linked list
                    if (prev == NULL)
                    {
                        // Removing the head node
                        shared_queue->head = current->next;
                    }
                    else
                    {
                        // Removing a middle or tail node
                        prev->next = current->next;
                    }

                    // Update tail if we removed the last node
                    if (current == shared_queue->tail)
                    {
                        shared_queue->tail = prev;
                    }

                    free(current);
                    break;
                }
                prev = current;
                current = current->next;
            }
        }
    }    
}


void handle_SIGCHLD(int signum)
{
    Completed_jobs();
}



void print_job_info()
{
    printf("\n-------- Command History --------\n");
    if (Complete_queue->tail==NULL && Complete_queue->head==NULL)
    {
        printf("No history to display\n");
    }
    else
    {
        command* temp = Complete_queue->head;
        while(temp!=NULL)
        {
            printf("Command: %s\n", *(temp->name));
            printf("PID: %d\n", temp->pid);
            printf("Wait time : %d\n", temp->wait_time);
            printf("Execution time : %d\n", temp->completion_time);
            printf("Status : %d\n ",temp->status);


            printf("-----------------------------\n");
            usleep(80);
        }
    }
}






// Add job to the queue
void add_job_to_queue(int pid, const char *job_name) {
    // Allocate memory for the new job
    command *new_job = (command *)malloc(sizeof(command));
    if (!new_job) {
        perror("Failed to allocate memory for new job");
        return;
    }

    // Initialize the new job
    new_job->pid = pid;
    new_job->status = READY;
    new_job->completion_time = 0;
    new_job->wait_time = 0;
    new_job->burst_time = 0;
    strcpy(*(new_job->name), job_name);
    signal(SIGINT, SIG_IGN); 
    new_job->start_time = get_current_time_ms();
    new_job->next = NULL;


    if (shared_queue->head == NULL) {
        // Queue is empty, initialize head and tail
        shared_queue->head = new_job;
        shared_queue->tail = new_job;
    } else {
        // Add to the end of the queue
        shared_queue->tail->next = new_job;
        shared_queue->tail = new_job;
    }

}

// Submit a job to the scheduler
void submit_job(char *job_name) {
    char** args = malloc(2046);
    args[0] = job_name;
    args[1] = NULL;
    // Execute the command
    pid_t p_pid = fork();
    if (p_pid < 0)
    {
        printf("failed fork");
        exit(1);
    }
    if (p_pid == 0)
    {
        add_job_to_queue(p_pid, job_name);
        (*number_of_jobs)++;
        kill(getpid(), SIGSTOP);

        if (execvp(args[0], args) == -1)
        {
            printf("%s : command not found\n", args[0]);
            exit(1);
        }
        printf("DOESNT EXECUTE\n");
    }
    else
    {
        signal(SIGCHLD, handle_SIGCHLD);

    }    
}

void handle_SIGINT(int signum)
{
    status = 0;
    printf("-------- SHELL LOOP OVER -------- \n");
    while (status == 0)
    {
        int check = 1;
        if(shared_queue->head==NULL && shared_queue->tail==NULL){
            check = 0;
        }

        sleep(10);
        if (!check)
        {
            print_job_info();
            kill(scheduler_pid, SIGTERM); // Send termination signal
            printf("Scheduler process terminated.\n");
            cleanup_shared_resources();
            status = 1;
        }
    }
    exit(0);
}



// Main shell loop for user input
void shell_loop() {
    char *command = NULL;
    size_t len = 0;
    signal(SIGINT, handle_SIGINT);
    signal(SIGCHLD, handle_SIGCHLD);   


    while (status) {
        printf("SimpleShell$ ");
        if (getline(&command, &len, stdin) == -1) {
            perror("Input error");
            free(command);
            break;
        }
        if (is_command_valid(command)) {
            if (strncmp(command, "submit", 6) == 0) {
                char *job_name = parse_job_name(command);
                if (job_name) {
                    submit_job(job_name);
                } else {
                    printf("Invalid submit command. Use: submit ./job_name\n");
                }
            } /*else if (strncmp(command, "history", 7) == 0) {
                display_command_history();
            } */else if (strncmp(command, "exit", 4) == 0) {
                break;
            } else {
                printf("Unknown command: %s", command);
            }
        }
    }
    free(command);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <NCPU> <TSLICE>\n", argv[0]);
        exit(1);
    }
    NCPU = atoi(argv[1]);
    TSLICE = atoi(argv[2]);
    if(NCPU<=0 || TSLICE<=0){
        printf("Number of CPU's and time quantum should be positive integers.");
        exit(0);
    }
    Complete_queue = (complete_queue*)malloc(sizeof(complete_queue)*256);
    Complete_queue->head = NULL;
    Complete_queue->tail = NULL;
    
    initialize_shared_resources();
    // Fork the scheduler
    scheduler_pid = fork();
    setpgid(0,0);
    if (scheduler_pid == 0) {
        execl("./SimpleScheduler", "./SimpleScheduler", argv[1], argv[2], NULL);
        perror("Scheduler execution failed");
        exit(1);
    }

    shell_loop();  // Start the shell loop
    return 0;
}
