// SLUSH - The SLU Shell
// Program execution commands have the form:
// prog_n [args] ( ... prog_3 [args] ( prog_2 [args] ( prog_1 [args]

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>

#define MAX_INPUT 256 // max input size
#define MAX_ARGS 15 // max number of arguments per pipe

void pipeline(char **commands, int num_commands) {
    int prev_pipe[2], new_pipe[2];
    pid_t pid;

    for (int i = num_commands - 1; i >= 0; i--) {
        // pipe and error check
        if (pipe(new_pipe) == -1) {
            perror("pipe");
            exit(EXIT_FAILURE);
        }

        // fork error check
        if ((pid = fork()) == -1) {
            perror("fork");
            exit(EXIT_FAILURE);
        }

        // child process
        if (pid == 0) {
            // redirect previous pipe output to stdin
            if (i != num_commands - 1) {
                dup2(prev_pipe[0], STDIN_FILENO);
                close(prev_pipe[0]);
                close(prev_pipe[1]);
            }
            // redirect stdout to new pipe if more commands exist
            if (i != 0) {
                dup2(new_pipe[1], STDOUT_FILENO);
            }
            close(new_pipe[0]);
            close(new_pipe[1]);

            // constructing arguments
            char* args[MAX_ARGS];
            int arg_count = 0;
            char *arg = strtok(commands[i], " ");
            while(arg && arg_count < MAX_ARGS) {
                args[arg_count++] = arg;
                arg = strtok(NULL, " ");
            }
            args[arg_count] = NULL;

            // execute current command
            execvp(args[0], args);

            // execvp error check
            perror(args[0]);
            exit(EXIT_FAILURE);
        }

        // close previous pipe if it exists
        if (i != num_commands - 1) {
            close(prev_pipe[0]);
            close(prev_pipe[1]);
        }
        prev_pipe[0] = new_pipe[0];
        prev_pipe[1] = new_pipe[1];
    }
    
    close(prev_pipe[0]);
    close(prev_pipe[1]);

    // wait for child processes
    while (wait(NULL) > 0);
}

// function to print current directory
void print_prompt() {
    char cwd[MAX_INPUT];
    char *home = getenv("HOME");

    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        // ignore home segment of path
        if (home && strncmp(cwd, home, strlen(home)) == 0) {
            printf("SLUSH|%s> ", cwd + strlen(home) + 1);
        } 
        else {
            printf("SLUSH|%s> ", cwd);
        }
    } 
    else {
        perror("getcwd");
        printf("SLUSH> ");
    }
    fflush(stdout);
}

// print a new prompt & flush stdout on catching ^C
void handler() {
    printf("\n");
    print_prompt();
    fflush(stdout);
}

int main () {
    char input[256];

    // signal handling
    signal(SIGINT, handler);

    while(1) {
        print_prompt();
        
        // getting input, checking for EOF, stripping newline
        if ((fgets(input, sizeof(input), stdin)) == 0) {
            printf("\n");
            break;
        }
        input[strcspn(input, "\n")] = '\0';

        char *commands[256];
        int num_commands = 0;
        char *token = strtok(input,"(");
        
        // parsing commands
        while(token != NULL) {
            // syntax validation
            if (strlen(token) == 0 || strspn(token, " ") == strlen(token)) {
                printf("Invalid null command\n");
                num_commands = 0;
                break;
            }
            commands[num_commands++] = token;
            token = strtok(NULL, "(");
        }

        // checking for "cd" input
        if (num_commands == 1 && strncmp(commands[0], "cd", 2) == 0) {
            char *dir = commands[0] + 3; // target directory
            if (chdir(dir) == -1) perror("cd");
            continue;
        }

        // execute pipeline
        pipeline(commands, num_commands);
    }

    return 0;
}
