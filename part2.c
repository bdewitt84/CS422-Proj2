// ./part2.c

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


int main(int argc, char const *argv[]) {

  // handle wrong number of arguments
  if (argc != 2) {
    usage();
    return 0;
  }

  char *input = NULL;
  size_t len = 0;
  int num_lines;
  pid_t* pid_array;

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
      printf("PID %d waiting for SIGUSR1\n", getpid());
      sigwait(&set, &sig);
      printf("PID %d received SIGUSR1\n", getpid());

      // try to swap image, handle errors
      if (execvp(args[0], args) < 0) {
        printf("execvp() failed for '%s'\n", args[0]);
        free(input);
        free_command_line(&token_buffer);
        free(pid_array);
        exit(-1);
      };
      exit(0);
    }
    ++cur;
    free_command_line(&token_buffer); 
  }

  // free_command_line(&token_buffer);
  free(input);
  
  // Delay and report for debugging
  int delay = 3;
  printf("%d second delay\n", delay);
  sleep(delay);

  // Send SIGUSR1 to each child process to execute workload
  for(int i=0; i < num_lines; ++i) {
    printf("Starting child process %d PID: %d\n",i ,pid_array[i]);
    kill(pid_array[i], SIGUSR1);
  }

  // Send SIGSTOP to each child process
  for(int i=0; i < num_lines; ++i) {
    printf("Stopping child process %d PID: %d\n",i ,pid_array[i]);
    kill(pid_array[i], SIGSTOP);
  }

  // Send SIGCONT to each child process
  for(int i=0; i < num_lines; ++i) {
    printf("Resuming child process %d PID: %d\n",i ,pid_array[i]);
    kill(pid_array[i], SIGCONT);
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

