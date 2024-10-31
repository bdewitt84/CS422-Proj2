// use the whole pseudo-shell program to work out the exeuctions?

#include<stdio.h>
#include<stdlib.h>
#include<unistd.h> // fork, execvp
#include<sys/types.h>
#include<sys/wait.h>
#include"string_parser.h"


// Signatures

void usage();
void print_command_line(command_line*);


int main(int argc, char const *argv[]) {
  // read progam worklad from input file (like file mode from proj1)
  if (argc == 1) {
    usage();
    return 0;
  }

  char *input = NULL;
  size_t len = 0;
  

  // open file for reading
  freopen(argv[1], "r", stdin);

  // command_line token_buffer;
  // token_buffer = str_filler(input, " ");

  while(getline(&input, &len, stdin) != -1) {
    
    command_line token_buffer;
    int line_num = 0;

    token_buffer = str_filler(input, " ");
    // print_command_line(&token_buffer);

    // for each cmd, mcp must launch the separate proc to run the cmd
    pid_t child_process = fork();

    if (child_process < 0) {
      fprintf(stderr, "fork failed\n");
      free(input);
      free_command_line(&token_buffer);
      exit(-1);
    }
    else if (child_process == 0) {
      printf("I am a child process with PID: %d\n", getpid());
      char** args = token_buffer.command_list;
      if (execvp(args[0], args) < 0) {
        // handle errors
        printf("execvp() failed for '%s'\n", args[0]);
      };
      free(input);
      free_command_line(&token_buffer);
      exit(-1); // will not be reached if image swap is successful
    }
    
    free_command_line(&token_buffer); 

  }
  // free_command_line(&token_buffer);
  free(input);

  // once all processes are running, wait() for all processes to stop
  while(wait(NULL) > 0);

  // once all processes have terminated, MCP must exit using exit()
  exit(0);
}

void usage() {
  // print usage text
}

void print_command_line(command_line* cmd){
  // print each token
  int num_tokens = cmd->num_token;
  for (int i = 0; i < num_tokens; ++i) {
    printf("arg %d: %s\n", i, cmd->command_list[i]);
  }
}
