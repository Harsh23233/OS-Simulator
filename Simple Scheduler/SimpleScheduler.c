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
#include <sys/time.h>
#include <errno.h>

#define SHM_NAME "/my_shm"

Shared_queue *shared_queue;
int* number_of_jobs;
int shm_fd_1, shm_fd_2;

// Function to reconnect resources in the exec'ed child process
void reconnect_resources_after_exec() {
    shm_fd_1 = shm_open(SHM_NAME, O_RDWR, 0666);
    if (shm_fd_1 == -1) {
        perror("Shared memory re-open failed in child");
        exit(1);
    }

    shared_queue = (Shared_queue*) mmap(NULL, 2046, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd_1, 0);
    if (shared_queue == MAP_FAILED) {
        perror("Shared memory re-mapping failed in child");
        exit(1);
    }

    // Re-attach shm_fd_2 for number_of_jobs
    int shm_fd_2 = shmget(IPC_PRIVATE, sizeof(int), 0666);
    if (shm_fd_2 < 0) {
        perror("Failed to re-access shared memory segment in child");
        exit(EXIT_FAILURE);
    }

    number_of_jobs = (int *)shmat(shm_fd_2, NULL, 0);
    if (number_of_jobs == (void *)-1) {
        perror("Failed to re-attach shared memory segment in child");
        shmctl(shm_fd_2, IPC_RMID, NULL); // Cleanup if attachment failed
        exit(EXIT_FAILURE);
    }
}

void cleanup_shared_resources() {
    munmap(shared_queue, 2046);
    close(shm_fd_1);
    shm_unlink(SHM_NAME);
    shmctl(shm_fd_2, IPC_RMID, NULL);
}


long get_current_time_ms() {
    struct timeval time;
    gettimeofday(&time, NULL);
    return time.tv_sec * 1000 + time.tv_usec / 1000;
}
// Function to check if a job is completed
int is_completed(pid_t pid) {
    int status;
    pid_t result = waitpid(pid, &status, WNOHANG);
    return result == pid && WIFEXITED(status);
}

command* get_next_job() {
    command* temp = shared_queue->head;
    while(temp != NULL){
        if(temp->pid > 0 && temp->status == READY){
            return temp;
        }
        temp = temp->next;
    }
    return NULL;
}

void move_n_nodes_to_end(command* head, int n) {
    if (head == NULL || n <= 0) return;  // If the list is empty or n <= 0, do nothing

    command* current = head;
    command* new_head = NULL;
    command* new_tail = NULL;

    // Traverse to the nth node
    for (int i = 1; i < n && current->next != NULL; i++) {
        current = current->next;
    }

    // If `n` is greater than the length of the list, do nothing
    if (current == NULL || current->next == NULL) return;

    // `new_head` is the head of the new list after moving nodes
    new_head = current->next;
    // `new_tail` is the end of the first `n` nodes
    new_tail = head;

    // Update the last node of the moved part to NULL
    current->next = NULL;

    // Traverse to the end of the list
    command* end = new_head;
    while (end->next != NULL) {
        end = end->next;
    }

    // Attach the first part to the end
    end->next = head;

    // Update the head pointer to the new head
    head = new_head;
}


void start_scheduler(int n_cpu, int time_slice) {
    long current_time;
    bool jobs_available = true;

    while (true)
    {

        if ((*number_of_jobs) > 0)
        {
            int running_jobs = 0;
            command* job;
            // Process up to NCPU jobs
            for (int i = 0; i < n_cpu && i != (*number_of_jobs); i++) {
                command *job = get_next_job();
                if(job == NULL){
                    printf("No more job available.\n");
                    break;
                }
                printf("found job\n");
                printf("job pid %d\n",job->pid);
                if (job && job->status == READY) {
                    printf("signal1\n");
                    kill(job->pid, SIGCONT); // Signal to start/resume job
                    printf("signal2\n");
                    job->status = RUNNING;
                    running_jobs++;
                }
            }

            usleep(time_slice * 1000);

            command *temp = shared_queue->head;
            int j = 0;
            // Calculate completion and waiting time
            while(temp != NULL && j<=running_jobs){
                if (temp && temp->status == RUNNING) {
                    kill(temp->pid, SIGSTOP);
                    temp->status = READY;
                    temp->burst_time += time_slice;
                    j++;
                }
                temp = temp->next;
            }     
            move_n_nodes_to_end(shared_queue->head,n_cpu);
        }    
    }
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
    printf("in sched\n");
    //signal(SIGTERM, handle_scheduler_termination);
    reconnect_resources_after_exec();
    start_scheduler(NCPU, TSLICE);
    return 0;
}
