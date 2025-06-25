Simple Scheduler
SimpleShell
This SimpleShell implementation allows the user to submit jobs (executables) for execution using a SimpleScheduler. The shell maintains a job queue using shared memory and interacts with the scheduler to manage job execution. The shell takes NCPU (number of CPUs) and TSLICE (time slice for scheduling) as input arguments. Jobs are scheduled based on Round Robin scheduling by the scheduler, and their states are updated. This SimpleShell works by interacting with a SimpleScheduler that manages the execution of jobs using signals and shared memory. Each job waits for a signal from the scheduler to begin execution and can be stopped using SIGSTOP. The shell ensures smooth communication with the scheduler and maintains a job queue in shared memory, allowing proper resource management.
Function-wise Explanation
is_command_valid(char *input)
Purpose: Validates user input by ensuring the command doesn't contain pipes ('|').
Returns:
true if the command is valid.
false if the command contains pipes (since pipes are not allowed).

initialize_shared_resources()
Purpose: Initializes shared memory and semaphores to manage shared resources between the shell and scheduler.
Flow:
Creates a shared memory segment using shm_open for the job queue.
Maps the shared memory segment using mmap.
Creates a second shared memory segment to store the number of jobs.
Opens a semaphore to handle synchronization between processes.

cleanup_shared_resources()
Purpose: Releases shared memory and semaphore resources during termination.
Flow:
Unmaps and unlinks the shared memory.
Closes and unlinks the semaphore to free resources.

parse_job_name(char *input)
Purpose: Extracts the job name from the user's input.
Returns: The extracted job name after the submit keyword.

get_current_time_ms()
Purpose: Fetches the current time in milliseconds using gettimeofday.

Completed_jobs()
Purpose: Handles completed jobs by:
It checks if a job has terminated.
It marks it as failed if it exits with a non-zero status.
Updating the job queue by removing completed or failed jobs.
Adds job details to the history.

handle_SIGCHLD(int signum)
Purpose: A signal handler for SIGCHLD, which is triggered when a child process (job) terminates.
Flow: Calls Completed_jobs() to handle the termination.

print_job_info()
Purpose: Displays the command history and detailed information about executed jobs by iterating over
 the shared memory queue.

add_job_to_queue(int pid, const char *job_name)
Purpose: Adds a new job to the queue (in shared memory).
Flow:
Allocates memory for a new command object.
Initializes the job’s properties (e.g., PID, status, start time).
Adds the job to the end of the queue in shared memory.

submit_job(char *job_name)
Purpose: Forks a new child process to execute the submitted job.
Flow:
Forks a new process.
In the Child process:
Adds itself to the queue.
Pauses until resumed by the scheduler.
Executes the job using execvp.
In the Parent process: Manages foreground jobs and updates the job count.

terminate_shell(int sig)
Purpose: Handles shell termination (on SIGINT), cleans up resources, and exits.

shell_loop()
Purpose: Runs the interactive shell and processes user commands.
Flow:
Displays a command prompt (SimpleShell$).
Accepts user input using getline().
Validates the input:
If the command is submit <job_name>, it calls submit_job().
If the command is exit, it terminates the shell.
On exit, calls terminate_shell() to clean up resources.
main(int argc, char *argv[])
Purpose: Initializes resources, forks the scheduler, and starts the shell.
Flow:
Validates input arguments (NCPU and TSLICE).
Initializes shared resources.
Forks the scheduler process.
Scheduler process: Executes the SimpleScheduler.
Shell process: Starts the shell_loop() to accept user commands.
      14. addToHistory(char* name, pid_t pid,  int wait_time, int comp_time)
Purpose: Once the jobs are completed, they are added to a list of all the completed jobs.
Flow: 
It allocates the memory for a job.
Then, it initializes the job.
The job is then added to the list and its head and tail pointers are updated.
      15. handle_SIGINT(int sig)
Purpose: To handle the SIGINT(Ctrl+c) signal for shell termination.
Flow:
The handler is set up and declared in the shell_loop so that whenever a ctrl+c command is input the signal handler i.e. handle_SIGINT is called.
The handler terminates the shell_loop and checks the job status and if all jobs are complete. The stats are printed for all the jobs.
Then the scheduler is terminated by sending a signal(SIGTERM) using kill().
All the shared resources are cleaned up.
This implementation of SimpleScheduler works closely with SimpleShell to manage job scheduling using shared memory, signals, and semaphores. 

Functions in SimpleScheduler
1. reconnect_resources_after_exec()
Purpose: It re-establishes shared memory and resources if the child process uses exec() (as it overwrites the current process).
Flow:
Opens shared memory for the job queue using shm_open.
Maps the memory using mmap to make it accessible.
Reattaches the shared memory segment storing the number of jobs using shmget and shmat.
2. cleanup_shared_resources()
Purpose: Cleans up shared memory and semaphores when the scheduler is terminated.
Flow:
Unmaps the shared memory and closes the shared memory file descriptor.
Unlinks the shared memory and semaphores to release the resources.
3. get_current_time_ms()
Purpose: Gets the current time in milliseconds using gettimeofday.
Use: Helps track job start, end, and completion times for scheduling.
4. is_completed(pid_t pid)
Purpose: Checks whether a process has completed execution.
Flow:
Uses waitpid with WNOHANG to non-blockingly check if the process has exited.
Returns:
1 if the job has completed.
0 otherwise.
5. get_next_job()
Purpose: Fetches the next READY job from the linked list.
Flow:
Starts from the head of the job queue and iterates through it.
Returns: A pointer to the first job with status == READY or NULL if no such job exists.
6. move_n_nodes_to_end()
Purpose: Moves the first NCPU jobs in the queue to the end.
Flow:
Traverses the queue to identify the first NCPU nodes.
Adjusts the pointers to move these nodes to the end of the list.
7. start_scheduler(int n_cpu, int time_slice)
Purpose: Manages job scheduling using Round Robin with support for NCPU parallel jobs.
Flow:
Checks if there are jobs in the queue using (*number_of_jobs).
If jobs exist:
Fetches up to NCPU jobs from the front of the queue.
Signals them using SIGCONT to resume execution.
Waits for the TSLICE duration using usleep().
After TSLICE:
Signals the jobs with SIGSTOP to pause execution.
Moves the executed jobs to the end of the queue using move_n_nodes_to_end().
8. main()
Purpose: Entry point for the scheduler.
Flow:
Validates the input arguments (NCPU and TSLICE).
Reconnects shared memory using reconnect_resources_after_exec().
Starts the scheduling loop by calling start_scheduler().

Code Flow for a command:
Shell Startup:
SimpleShell initializes shared resources (shared memory and semaphores).
The shell forks a scheduler process to run SimpleScheduler.
Job Submission:
The user enters the executable along with submit as the prompt. 
submit_job() is called, which:
Forks a child process for the job.
The child process:
Adds itself to the shared queue using add_job_to_queue().
Pauses itself using SIGSTOP until resumed by the scheduler,.
The parent continues accepting further user inputs.
Scheduler Operation:
Scheduler enters the infinite loop in start_scheduler().
It checks if any READY jobs exist in the shared queue.
NCPU jobs are fetched using get_next_job().
If jobs are available:
They are resumed using SIGCONT.
Scheduler waits for TSLICE duration using usleep().
After the TSLICE:
Jobs are paused using SIGSTOP.
Their burst times are updated.
Jobs are moved to the rear of the queue.
Job Completion:
If a job completes during its TSLICE:
It is marked as COMPLETED using is_completed().
Its completion and wait times are updated.
Scheduler Sleeping:
If the job queue becomes empty, the scheduler sleeps using usleep(500000) (500 ms).
It continuously checks for new jobs until termination.
Shell Termination:
When the user exits the shell (exit command):
terminate_shell() is called to:
Terminate the scheduler and all jobs.
Cleanup resources (shared memory and semaphores).
The shell and scheduler terminate gracefully.

Contribution:
1. Abhinav Kashyap, 2023022: get_next_job(), move_n_nodes_to_end(), start_scheduler(), cleanup_shared_resources(SimpleShell.c), initialize_shared_resources(), handle_SIGCHLD(), add_job_to_queue, submit_job, terminate_shell.
2. Harsh Sharma, 2023233: get_next_job(), move_n_nodes_to_end(), start_scheduler(), cleanup_shared_resources(SimpleScheduler.c), reconnect_resources_after_exec(), handle_SIGINT(), Completed_jobs, submit_job, addToHistory.







