/*
 * Assignment 3: smallsh
 * Author: Tim Turnbaugh
 * Class: OSU CS 344: Operating Systems
*/
 
#include <fcntl.h>
#include <stdbool.h> 
#include <signal.h> 
#include <stdio.h>
#include <stdlib.h> 
#include <string.h>
#include <sys/wait.h> 
#include <sys/types.h>
#include <unistd.h> 


// Constants
#define EXPANSIONVAR "$$"
#define DELIM " \n"
#define MAXBUFF 2048 // Per guidelines
#define MAXARGS 512 // Per guidelines

/* struct to hold & processes */
typedef struct node{ // linked list background process struct
  int pid; 
  struct node *next;
}node;

// Linked List Functions
void push_node(struct node**, int);
void remove_node(struct node**, int);
void check_node(struct node*);
void kill_list(struct node*);

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
  char *_arg;

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
    if(strstr(args[i], EXPANSIONVAR)){ 
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

void handle_SIGINT(int signum){
  kill(fgpid, SIGINT);
}

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

/* removes a specified node from linked list. The list is then adjusted accordingly for the surrounding nodes. */
void remove_node(struct node **head, int pid){ 
  struct node* current = *head, *prev;

  // If head is target node 
  if (current != NULL && current->pid == pid){ 
    *head = current->next;
    free(current);
    return; 
  } 

  // Keep progressing through linked list
  while(current != NULL && current->pid != pid){ 
    prev = current; 
    current = current->next; 
  } 

  // End of list check
  if(current == NULL){
    return; 
  }

  // Removal of node
  prev->next = current->next; 
  free(current);
} 


/* Kills node */
void kill_list(struct node *current_node){ 
  // Check if empty linked list
  if(current_node == NULL){
    return;
  }

  // Kill current node and its children
  while(current_node != NULL){ 
    kill(current_node->pid, SIGKILL);
    current_node = current_node->next;
  } 

  return;
} 

/* displays contents of name array */
int cmd_status(char **args){
  fprintf(stdout, "%d\n", last_exit);
  fflush(stdout);
  return 1; // Don't close smallsh
}

/* Change Directory (cd) */
int cmd_cd(char **args){
  if (args[1] == NULL){
    chdir(getenv("HOME")); // Home is default subdirectory of root in linux
  } 
  else if (chdir(args[1]) != 0){
      perror("error changing to directory");
      fflush(stdout);
      exit(1);
    }
  return 1; // Don't close smallsh
}

/* ends shell */
int cmd_exit(char **args){
  kill_list(child);
  kill(fgpid, SIGKILL);

  return 0; // Close smallsh
}

// size of function array
/* returns number of functions in function array */
int num_funcs(){
  return sizeof(func_names) / sizeof(char *);
}

/* executes normal/standard functions */
int std_exe(char **args, bool bg){
  pid_t spawnpid;
  int i = 0;
  int target_in = 0;
  int target_out = 0;
  int status;

	spawnpid = fork(); // Per class exploration 
	switch (spawnpid){
		case -1: 
      // Executed by parent when fork fails and creation of child also fails
			perror("fork() Failed");
      fflush(stdout);
			exit(1);
			break;

		case 0: 
      // Child executes this branch
      // TODO
      while(args[i] != NULL){ 
        // Checks if current arg is angle brackets if so copies contents accordingly
        if(strcmp(args[i], "<") == 0){ 
          args[i] = NULL; 
          target_in = open(args[++i], O_RDONLY);
          args[i] = NULL;
          dup2(target_in, 0);
        }
        else if(strcmp(args[i], ">") == 0){ 
          args[i] = NULL;
          target_out = open(args[++i], O_WRONLY | O_CREAT | O_TRUNC, 0640);
          args[i] = NULL; 
          dup2(target_out, 1);
        }

        // Next arg
        i++;
      }
      
      // TODO: dup2
      if(bg == true){ 
        if(target_in == 0){ 
          target_in = open("/dev/null", O_RDONLY);
          dup2(target_in, 0);
        }
        if(target_out == 0){ 
          target_out = open("/dev/null", O_WRONLY | O_CREAT | O_TRUNC, 0640);
          dup2(target_out, 1);
        }
      }

      if(execvp(args[0], args) == -1){ // Exec funcs only return if error occurs "Error Check"
        perror("Exec Error");
        fflush(stdout);
        exit(1);
      }
      
      // Print and clear
      fflush(stdout);
      break;

		default:
      // Parent executes this branch because spawnpid == pid of child
      if(bg == true){ // background mode
        fprintf(stdout, "%d Started\n", spawnpid);
        push_node(&child, spawnpid); // Add child node to linked list
      }
      else{ // foreground mode
        fgpid = spawnpid;
        waitpid(spawnpid, &status, 0);
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

/* substitutes EXPANSIONVAR with pid */
char *expand(char *arg){
  pid_t num = getpid(); 
    int pidLen = snprintf(NULL, 0, "%d", num); // get str pid length
    char *pid = malloc(pidLen+1); // make str for pid
    snprintf(pid, pidLen+1, "%d", num); // pid_t -> str
  char var[] = EXPANSIONVAR;
  char *result; 
  int i;
  int count = 0; 

  // TODO
  int varLen = strlen(var); 

  for (i = 0; arg[i] != '\0'; ++i){ // count how many times $$ shows up
    if (strstr(&arg[i], var) == &arg[i]){ 
      count++; 
      i += varLen-1; 
    } 
  } 

  // TODO:
  result = (char*)malloc(i + count * (pidLen - varLen) + 1); 

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
 * Usage of genenv("HOME"): https://www.tutorialspoint.com/c_standard_library/c_function_getenv.htm 
 * 
 * Background and foreground process in linux explanation: https://alligator.io/workflow/background-foreground-processes/
 * 
 * 
*/