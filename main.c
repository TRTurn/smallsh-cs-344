/*
 * Assignment 3: smallsh
 * Author: Tim Turnbaugh
 * Class: OSU CS 344: Operating Systems
*/
 
#include <stdio.h>
#include <stdlib.h> 
#include <string.h>
#include <unistd.h> 
#include <sys/wait.h> 
#include <sys/types.h>
#include <fcntl.h>
#include <stdbool.h> 
#include <signal.h> 

// Constants
#define EXPANDVAR "$$"
#define DELIM " \n"
#define MAXBUFF 2048
#define MAXARGS 512 

/* struct to hold & processes */
typedef struct node{ // linked list background process struct
  int pid; 
  struct node *next;
}node;

// Linked List Functions
void push_node(struct node**, int);
void remove_node(struct node**, int);
void check_node(struct node*);
void kill_node(struct node*);

// Required Functions
int cmd_status(char** );
int cmd_cd(char**);
int cmd_exit(char**);

// Helper Functions
int num_funcs();
int std_exe(char**, bool);
char *expand(char*);

// Input Handling
char *read_in();
char **parse_in(char*);
int func_exe(char**);
void shell();

// Signal Handlers
void handle_SIGINT(int);
void handle_SIGSTP(int);


// --Initialize Globals--
/* array of built in functions, parallel to name array */
int (*func_array[])(char **) = {
  &cmd_exit,
  &cmd_cd,
  &cmd_status
};
/* string array of built in function names */
char *func_names[] = {
  "exit",
  "cd",
  "status",
};

int fgpid = 0; 
int last_exit = 0;
bool background = true; 
struct node *child = NULL;


int main(int argc, char **argv){
  shell();

  free(child);

  return 0;
}

// Shell Commands
/* main shell interface, read, parse, execute */
void shell(){
  char *input;
  char **args;
  int run;

  do{
    check_node(child); // check if previous processes have exited
    input = read_in(); // get input
    args = parse_in(input); // parse input
    run = func_exe(args); // execute cmds from input

    free(input);
    free(args);

  } while(run);
}

/* reads in stdin command */
char *read_in(){
  char *buffer = NULL;
  size_t buff_size = MAXBUFF;

  // Set sig handlers
  signal(SIGINT, handle_SIGINT);
  signal(SIGTSTP, handle_SIGSTP); 
  
  printf(": ");
  getline(&buffer, &buff_size, stdin);

  return buffer;
}

/* parses inputed command into array of pointers */
char **parse_in(char *input){
  int buff = MAXARGS;
  int position = 0;
  char **_args = malloc(buff * sizeof(char*));
  char *_arg, **args_backup;

  // TODO:
  // Breaking up the input into a series of arguments
  // Comparable to https://www.tutorialspoint.com/c_standard_library/c_function_strtok.htm
  _arg = strtok(input, DELIM);
  while (_arg != NULL) {
    _args[position] = _arg;
    position++;
    _arg = strtok(NULL, DELIM);
  }

  // Add final Null
  _args[position] = NULL;
  return _args;
}

/* execute built in commands */
int func_exe(char **args){
  // Check if empty or comment
  if(args[0] == NULL || strchr(args[0], '#') != NULL) {
    return 1;
  }

  int i = 0;
  while(args[i] != NULL){ 
    // Check for expansion variable 
    if(strstr(args[i], EXPANDVAR)){ 
      args[i] = expand(args[i]);
    }
    i++;
  }

  bool bg = false; 
  i--; // bring back to last arg
  if(strcmp(args[i], "&") == 0){ 
    args[i] = NULL;
    bg = true; // do in background mode
  }

  if(background == false){ // check if background available
    bg = false;
  }

  for(i = 0; i < num_funcs(); ++i){ // check if args[i] is built in
    if (strcmp(args[0], func_names[i]) == 0){
      return (*func_array[i])(args);
    }
  }

  return std_exe(args, bg); // else args[i] is not built in
}


//-- Handlers
/* handles SIGINT signals */
void handle_SIGINT(int signum){
  kill(fgpid, SIGINT);
}

/* handles SIGTSTP signals */
void handle_SIGSTP(int signum){
  if(background == true){
    background = false;
  }
  else if(background == false){
    background = true;
  }
}

// TODO:
/* Checks and waits for status of nodes in linked list */
void check_node(struct node *head){ 
  if(head == NULL){
    return;
  }

  pid_t pid;
  int status;
  while (head != NULL){ 
    // Wait for head process to complete
    pid = waitpid(head->pid, &status, WNOHANG);
    if(pid == head->pid){
      fprintf(stdout, "%d Exited | Status %d\n", head->pid, WEXITSTATUS(status));
      remove_node(&child, head->pid);
    }
    //TODO: Add else check for break?
    head = head->next; 
  } 
  fflush(stdout);
  return;
} 

// --Linked List Commands--
/* adds new child to end of linked list */
void push_node(struct node **head, int new_pid){ 
  if(new_pid != -1){
    struct node* new_child = (struct node*) malloc(sizeof(struct node)); 
    new_child->pid = new_pid; 
    new_child->next = *head; 
    *head = new_child; 
  }
  return;
} 

/* removes exited child from linked list */
void remove_node(struct node **head, int pid){ 
  struct node* temp = *head, *prev; 
  if (temp != NULL && temp->pid == pid){ 
    *head = temp->next;
    free(temp);
    return; 
  } 

  while(temp != NULL && temp->pid != pid){ 
    prev = temp; 
    temp = temp->next; 
  } 

  if(temp == NULL){
    return; 
  }

  prev->next = temp->next; 
  free(temp);
} 


/* Kills node */
void kill_node(struct node *head){ 
  if(head == NULL){
    return;
  }

  while(head != NULL){ 
    kill(head->pid, SIGKILL);
    head = head->next;
  } 

  return;
} 


//-- Built In
/* displays contents of name array */
int cmd_status(char **args){
  fprintf(stdout, "%d\n", last_exit);
  fflush(stdout);

  return 1;
}

/* Change Directory (cd) */
int cmd_cd(char **args){
  if (args[1] == NULL){
    chdir(getenv("HOME"));
  } 
  else if (chdir(args[1]) != 0){
      perror("error changing to directory");
      fflush(stdout);
      exit(1);
    }
  return 1;
}

/* ends shell */
int cmd_exit(char **args){
  kill_node(child);
  kill(fgpid, SIGKILL);

  return 0;
}

// size of function array
/* returns number of functions in function array */
int num_funcs(){
  return sizeof(func_names) / sizeof(char *);
}

//-- Main Functions
/* executes standard functions */
int std_exe(char **args, bool bg){
  pid_t pid;
  int i = 0;
  int target_in = 0;
  int target_out = 0;
  int status;

	pid = fork();
  
	switch (pid){
		case -1: 
			perror("Failed");
      fflush(stdout);
			exit(1);
			break;

		case 0: 
      while(args[i] != NULL){ 
        if(strcmp(args[i], "<") == 0){ 
          args[i] = NULL; 
          i++;
          target_in = open(args[i], O_RDONLY);
          args[i] = NULL;
          dup2(target_in, 0);
        }
        else if(strcmp(args[i], ">") == 0){ 
          args[i] = NULL;
          i++; 
          target_out = open(args[i], O_WRONLY | O_CREAT | O_TRUNC, 0640);
          args[i] = NULL; 
          dup2(target_out, 1);
        }
        i++;
      }
      
      // TODO: Dev/Null?
      if(bg == true){ // background
        if(target_in == 0){ // no input
          target_in = open("/dev/null", O_RDONLY);
          dup2(target_in, 0);
        }
        if(target_out == 0){ // no output
          target_out = open("/dev/null", O_WRONLY | O_CREAT | O_TRUNC, 0640);
          dup2(target_out, 1);
        }
      }

      if(execvp(args[0], args) == -1){
        perror("sh");
        fflush(stdout);
        exit(1);
      }
      
      fflush(stdout);
      break;

		default: // parent
      if(bg == true){ // background mode
      //Todo fix extracolon in pid
        fprintf(stdout, "%d Started\n", pid);
        push_node(&child, pid);
      }
      else{ // foreground mode
        fgpid = pid;
        waitpid(pid, &status, 0);
        if(WIFSIGNALED(status)){
          fprintf(stdout, "\n Signal %d recived\n", WTERMSIG(status));
        }
      }
      last_exit = WEXITSTATUS(status); // save last process exit status
			break;
	}
  fflush(stdout);

  return 1;
}

/* replaces EXPANDVAR with pid */
char *expand(char *arg){
  pid_t num = getpid(); // get pid in int form
    int pidLen = snprintf(NULL, 0, "%d", num); // get str pid length
    char *pid = malloc(pidLen+1); // make str for pid
    snprintf(pid, pidLen+1, "%d", num); // pid_t -> str
  char var[] = EXPANDVAR;
  char *result; 
  int i;
  int count = 0; 
  int varLen = strlen(var); 

  for (i = 0; arg[i] != '\0'; ++i){ // count how many times $$ shows up
    if (strstr(&arg[i], var) == &arg[i]){ 
      count++; 
      i += varLen-1; 
    } 
  } 

  result = (char*)malloc(i + count * (pidLen - varLen) + 1); // new string

  i = 0; 
  while (*arg){ 
    if (strstr(arg, var) == arg ){
      strcpy(&result[i], pid); 
      i += pidLen; 
      arg += varLen; 
    } 
    else{
      result[i++] = *arg++; 
    }
  } 
  return result;
}





/* Sources
 * C Programming Book
 * OSU Tutor
 * /dev/null https://linuxhint.com/what_is_dev_null/
 * Usage of signal(): https://www.tutorialspoint.com/c_standard_library/c_function_signal.htm
 * Usage of waitpid(): https://www.tutorialspoint.com/unix_system_calls/waitpid.htm
 * Usage of getline(): https://c-for-dummies.com/blog/?p=1112
 * Usage of strtok(): https://www.tutorialspoint.com/c_standard_library/c_function_strtok.htm 
*/