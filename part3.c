// ./part3.c


// Imports

#include<stdio.h>
#include<stdlib.h>
#include<unistd.h> // fork, execvp
#include<sys/types.h>
#include<sys/wait.h>
#include<signal.h> // sigwait, sigprocmask
#include"string_parser.h"


// Signatures

void usage();
void print_command_line(command_line*);
int count_lines(const char* file);
int schedule_next_proc(pid_t** pid_array, int* cur);
void handle_alarm(int signum);


// Globals

volatile sig_atomic_t alarm_triggered = 0;


int main(int argc, char const *argv[]) {

  // handle wrong number of arguments
  if (argc != 2) {
    usage();
    return 0;
  }

  char *input = NULL;
  size_t len = 0;
  int num_lines;
  pid_t *pid_array;

  // get the number of lines in the file
  num_lines = count_lines(argv[1]);

  // create array to hold PIDs
  pid_array = (pid_t*)malloc(sizeof(pid_t) * num_lines);

  // open file for reading
  freopen(argv[1], "r", stdin);

  // iterate over commands
  int cur = 0;
  while(getline(&input, &len, stdin) != -1) {
    
    command_line token_buffer;
    int line_num = 0;

    // get the args from the current line
    token_buffer = str_filler(input, " ");

    // for each cmd, mcp must launch the separate proc to run the cmd
    pid_t child_process = fork();

    // add the pid to the array
    pid_array[cur] = child_process;

    // handle fork
    if (child_process < 0) {
      fprintf(stderr, "fork failed\n");
      free(input);
      free_command_line(&token_buffer);
      free(pid_array);
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
        free(input);
        free_command_line(&token_buffer);
        free(pid_array);
        exit(-1);
      };
    }
    ++cur;
    free_command_line(&token_buffer); 
  }

  // free_command_line(&token_buffer);
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
  
  // initialize process scheduler
  int cur_proc = 0;

  // activate first process
  printf("sendint SIGCONT to proc %d with pid %d\n", cur_proc, pid_array[cur_proc]);
  kill(pid_array[cur_proc], SIGCONT);

  // Wait for alarm
  while(1) {

    if (alarm_triggered) {
      int terminated;
      printf("scheduling next process\n");

      // stop current process
      printf("sending SIGSTOP to proc %d with pid %d\n", cur_proc, pid_array[cur_proc]);
      kill(pid_array[cur_proc], SIGSTOP);

      // find next active process
      ++cur_proc;
      if (cur_proc >= num_lines) {
        cur_proc = 0;
      }

      terminated = 0;
      while(waitpid(pid_array[cur_proc], NULL, WNOHANG) != 0) {

        // loop through processes
        ++cur_proc;
        if (cur_proc >= num_lines) {
          cur_proc = 0;
        }

        // exit if all processes are terminated
        ++terminated;
        if (terminated >= num_lines) {
          printf("All processes terminated. Exiting normally.");
          exit(0);
        }
      }

      // sent cont to next process
      printf("sending SIGCONT to proc %d with pid %d\n", cur_proc, pid_array[cur_proc]);
      kill(pid_array[cur_proc], SIGCONT);

      // reset alarm
      alarm(interval);
      alarm_triggered = 0;
    }
    printf("waiting for alarm\n");
    sleep(1);
  }

  // once all processes are running, wait() for all processes to stop
  while(wait(NULL) > 0);

  // clean up allocated memory
  free(pid_array);

  // once all processes have terminated, MCP must exit using exit()
  exit(0);
}

void usage() {
  // print usage text
}

void print_command_line(command_line* cmd){
  // print each token
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

int schedule_next_proc(pid_t** pid_array, int* cur) {
  printf("scheduling next process: %d\n", *pid_array[*cur]);
  return 0;
}

void handle_alarm(int signum) {
  alarm_triggered = 1;
  return;
}
