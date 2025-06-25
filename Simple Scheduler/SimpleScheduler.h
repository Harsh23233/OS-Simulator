// SimpleScheduler.h
#include <sys/types.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#define SHM_KEY 0x1234
#define SEM_KEY 0x5678
#define MAX_JOBS 100

typedef enum { READY, RUNNING, COMPLETED } JobStatus;

typedef struct command {
    pid_t pid;                   // Process ID of the job
    char* name[256];              // Job name
    int burst_time;              // Total execution time required by the job
    int wait_time;               // Total waiting time
    int remaining_time;
    int completion_time;         // Total completion time
    int start_time;              // Time when the job was added to the queue
    int end_time;                // Time when the job completes
    int status;                  // Job status: READY, RUNNING, COMPLETED
    struct command *next;        // Pointer to the next job in the queue
} command;

typedef struct Shared_queue{
    command* head;
    command* tail;    
} Shared_queue;



// Shared functions for both files
void initialize_shared_resources();
void cleanup_shared_resources();
void sem_lock();
void sem_unlock();
void add_job_to_queue(pid_t pid, const char *job_name);
command *get_next_job();
void move_job_to_end(command *job);
void remove_job_from_queue(command *job);
int is_completed(pid_t pid);
int NCPU;
int TSLICE;
