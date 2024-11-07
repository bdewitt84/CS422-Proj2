// ./part3.c


// Imports

#include<stdio.h>
#include<stdlib.h>
#include<unistd.h> // fork, execvp
#include<sys/types.h>
#include<sys/wait.h>
#include<signal.h> // sigwait, sigprocmask
#include"string_parser.h"
#include<string.h>


// Signatures

// Displays usage if we get the wrong number of args
void usage(const char* cmd_name);

// Prints the contents of 'cmd', used for debugging
void print_command_line(command_line* cmd);

// Counts the number of lines in file 'filename'
int count_lines(const char* filename);

// Schedules the next process in 'pid_array' from current index 'cur'
int schedule_next_proc(pid_t** pid_array, int* cur);

// Sets the global flag 'alarm_trigged' to 1
void handle_alarm(int signum);

// Prepares child process to execute workload in 'token_buffer' after receiving SIGUSR1
int init_child_process(pid_t child_process, command_line token_buffer);

// Shows status of processes
void report(pid_t *pid_array, int num_processes);

// Returns stat state
void get_stat_state(pid_t pid, char* state);

// Returns stat user time
void get_stat_utime(pid_t pid, double* utime);

// Returns stat system time
void get_stat_stime(pid_t pid, double* stime);

// Returns stat process name
void get_stat_name(pid_t pid, char* buffer);

// Returns io bytes read
int get_io_rchar(pid_t pid, unsigned long* read_bytes);

// returns io bytes written
int get_io_wchar(pid_t pid, unsigned long* write_bytes);


// Globals

// Used by alarm handler
volatile sig_atomic_t alarm_triggered = 0;


int main(int argc, char const *argv[]) {

  // handle wrong number of arguments
  if (argc != 2) {
    usage(argv[0]);
    return 0;
  }

  // Declarations
  int num_lines;                          // used to determine size of pid_array
  const char *input_filename = argv[1];   // name of input file containing list of commands
  pid_t *pid_array;                       // holds pid of each child process
  char *input = NULL;                     // used by getline
  size_t len = 0;                         // used by getline
  command_line token_buffer;              // holds arguments for each command in input file
  pid_t child_process;                    // holds pid of most recently forked child process

  // get the number of lines in the file
  num_lines = count_lines(input_filename);

  // create array to hold PIDs
  pid_array = (pid_t*)malloc(sizeof(pid_t) * num_lines);

  // open file for reading
  freopen(input_filename, "r", stdin);

  // iterate over commands in input file
  for (int i = 0; i < num_lines; ++i) {
    getline(&input, &len, stdin);
    child_process = fork();
    pid_array[i] = child_process;
    token_buffer = str_filler(input, " ");
    init_child_process(child_process, token_buffer);
    free_command_line(&token_buffer);
  }
  free(input);
  
  // Delay to make sure processes are waiting for SIGUSR1
  int delay = 1;
  printf("%d second delay\n", delay);
  sleep(delay);

  // Send SIGUSR1 to each child process to execute workload
  for(int i=0; i < num_lines; ++i) {
    printf("Sending SIGUSR1 to child process %d PID: %d\n", i, pid_array[i]);
    kill(pid_array[i], SIGUSR1);
  }

  // Send SIGSTOP to each child process
  for(int i=0; i < num_lines; ++i) {
    printf("Sending SIGSTOP to  process %d PID: %d\n", i, pid_array[i]);
    kill(pid_array[i], SIGSTOP);
  }

  // Set alarm
  int interval = 1;
  signal(SIGALRM, handle_alarm);
  alarm(interval);
  printf("alarm set\n");
  
  int active_processes = 5;
  int cur_proc = 0;
  int status_ptr;
  int cur_pid;
  int wait_ret;

  while(active_processes) {
    if (alarm_triggered) {
      report(pid_array, num_lines);
      // printf("alarm received\n");
      cur_pid = pid_array[cur_proc];
      // don't check already termiated processes
      if (cur_pid > 0) {
        wait_ret = waitpid(cur_pid, &status_ptr, WNOHANG);
        // something went wrong
        if (wait_ret == -1) {
          printf("waitpid error\n");
          exit(-1);
        }
        // status changed
        else if (wait_ret > 0) {
          // process terminated
          if (WIFEXITED(status_ptr)) {
            int exit_code = WEXITSTATUS(status_ptr);
            printf("PID %d exited with status %d\n", cur_pid, exit_code);
            pid_array[cur_proc] = -1;
            --active_processes;
            // if we are done, exit
            if (active_processes == 0) {
              printf("all processes terminated, exiting\n");
              free(pid_array);
              exit(0);
            }
          }
        }
        // process still running
        else {
          // printf("sending SIGSTOP to %d\n", cur_pid);
          // kill(cur_pid, SIGSTOP);
        }
      }

      do {
        cur_proc = (cur_proc + 1) % num_lines;
      } while (pid_array[cur_proc] == -1);

      // schedule next process
      // printf("sending SIGSTOP to %d\n", cur_pid);
      kill(cur_pid, SIGSTOP);
      cur_pid = pid_array[cur_proc];
      // printf("sending SIGCONT to %d\n", cur_pid);
      kill(cur_pid, SIGCONT);

      // reset alarm
      // printf("reseting alarm\n"); 
      alarm(interval);
      alarm_triggered = 0;

      // report(pid_array, num_lines);
    }
    // wait for alarm
  }

  free(pid_array);
  exit(-1); // We somehow broke the loop without exiting. This should not happen.
}


void usage(const char* cmd_name) {
  // print usage text
  printf("Usage:\n\t%s <PATH>\n\n\t<PATH>: path to input file containing commands to be scheduled\n", cmd_name);
}


void print_command_line(command_line* cmd) {
  // print each token in cmd
  int num_tokens = cmd->num_token;
  for (int j = 0; j < num_tokens; ++j) {
    printf("arg %d: %s\n", j, cmd->command_list[j]);
  }
}


int count_lines(const char* file) {
  int count = 0;
  char* input = NULL;
  size_t len;
  
  // Open file in argument
  freopen(file, "r", stdin);

  // Iterate through lines
  while (getline(&input, &len, stdin) != -1) {
    ++count;
  }
  
  // getline mallocs input, so free it
  free(input);

  return count;
}


void handle_alarm(int signum) {
  alarm_triggered = 1;
  return;
}


int init_child_process(pid_t child_process, command_line token_buffer) {
  // handle fork
  if (child_process < 0) {
    fprintf(stderr, "fork failed\n");
    exit(-1);
  }
  else if (child_process == 0) {
    printf("I am a child process with PID: %d\n", getpid());
    char** args = token_buffer.command_list;
    
    // suspend execution of the child process
    int how = SIG_BLOCK;
    sigset_t set;
    int sig;
    
    // initialize sigset and add SIGUSR1
    sigemptyset(&set);
    sigaddset(&set, SIGUSR1);
    sigprocmask(how, &set, NULL);

    // tell child process to wait for SIGUSR1
    printf("PID %d waiting for SIGINT\n", getpid());
    sigwait(&set, &sig);
    printf("PID %d received SIGINT\n", getpid());

    // handle errors
    if (execvp(args[0], args) < 0) {
      printf("execvp() failed for '%s'\n", args[0]);
      exit(-1);
    };
  }
}

void report(pid_t *pid_array, int num_processes) {

  printf("PID \t PROC NAME \t STATE \t USER TIME \t KERNEL TIME \t READ \t WRITE \n");

  for (int i = 0; i < num_processes; ++i) {
    int cur_pid = pid_array[i];

    if (cur_pid == -1) {
      // process is terminated, don't print anything
    }
    else {
      int state_size = 20;
      char state[state_size];
      char name[128];
      double utime;
      double stime;
      unsigned long read_bytes = 0;
      unsigned long write_bytes = 0;
  
      get_stat_name(cur_pid, name);
      get_stat_state(cur_pid, state);
      get_stat_utime(cur_pid, &utime);
      get_stat_stime(cur_pid, &stime);
      get_io_rchar(cur_pid, &read_bytes);
      get_io_wchar(cur_pid, &write_bytes);
  
      printf("%d \t %-8s \t %s \t %lf \t %lf \t %lu \t %-12lu \n",
        cur_pid,
        name,
        state,
        utime,
        stime,
        read_bytes,
        write_bytes
        );
    }
  }
}

void get_stat_state(pid_t pid, char* buffer) {
  char path[128];
  snprintf(path, sizeof(path), "/proc/%d/stat", pid);
  FILE *fp = fopen(path, "r");

  if (fp == NULL) {
     printf("Error opening file: %s\n", path);
     exit(-1);
  }
  fscanf(fp, "%*d %*s %s", buffer);
  fclose(fp);
}

void get_stat_utime(pid_t pid, double* output) {
  char path[128];
  snprintf(path, sizeof(path), "/proc/%d/stat", pid);
  FILE *fp = fopen(path, "r");

  if (fp == NULL) {
    printf("Error opening file: %s\n", path);
    exit(-1);
  }
  
  for (int i =  0; i < 13; i++) {
    fscanf(fp, "%*s");
  }

  unsigned long utime;
  fscanf(fp, "%lu", &utime);
  fclose(fp);

  long ticks_per_sec = sysconf(_SC_CLK_TCK);
  *output = utime / (double)ticks_per_sec;
}

void get_stat_stime(pid_t pid, double* output) {
  char path[128];
  snprintf(path, sizeof(path), "/proc/%d/stat", pid);
  FILE *fp = fopen(path, "r");

  if (fp == NULL) {
    printf("Error opening file: %s\n", path);
    exit(-1);
  }
  
  for (int i =  0; i < 14; i++) {
    fscanf(fp, "%*s");
  }

  unsigned long stime;
  if (fscanf(fp, "%lu", &stime) != 1) {
    printf("Error reading stime for pid: %d\n", pid);
    fclose(fp);
    exit(EXIT_FAILURE);
  }
  fclose(fp);

  long ticks_per_sec = sysconf(_SC_CLK_TCK);
  *output = stime / (double)ticks_per_sec;
}

void get_stat_name(pid_t pid, char* buffer) {
  char path[128];
  snprintf(path, sizeof(path), "/proc/%d/stat", pid);
  
  FILE *fp = fopen(path, "r");

  if (fp == NULL) {
    printf("Error opening file: %s\n", path);
    exit(-1);
  }
  
  fscanf(fp, "%*s");

  if (fscanf(fp, "%s", buffer) != 1) {
    printf("Error reading name for pid: %d\n", pid);
    fclose(fp);
    exit(EXIT_FAILURE);
  }

  fclose(fp);
}

int get_io_rchar(pid_t pid, unsigned long *read_bytes) {
  char path[128];                                     
  snprintf(path, sizeof(path), "/proc/%d/io", pid);
  
  FILE *fp = fopen(path, "r");

  if (fp == NULL) {
    // printf("Error opening file: %s\n", path);
    return -1;
  }

  char label[64];
  unsigned long value;

  while (fscanf(fp, "%s %lu", label, &value) != EOF) {
    if (strcmp(label, "rchar:") == 0) {
      *read_bytes = value;
      fclose(fp);
      return 0;
    }
  }

  fclose(fp);
  // no value found
  return -1;
}

int get_io_wchar(pid_t pid, unsigned long *write_bytes) {
  char path[128];                                     
  snprintf(path, sizeof(path), "/proc/%d/io", pid);
  
  FILE *fp = fopen(path, "r");

  if (fp == NULL) {
    // printf("Error opening file: %s\n", path);
    return -1;
  }

  char label[64];
  unsigned long value;

  while (fscanf(fp, "%s %lu", label, &value) != EOF) {
    if (strcmp(label, "wchar:") == 0) {
      *write_bytes = value;
      fclose(fp);
      return 0;
    }
  }

  fclose(fp);
  // no value found
  return -1;
}
