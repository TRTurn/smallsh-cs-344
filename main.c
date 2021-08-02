/*
 * Assignment 3: smallsh
 * - Command Prompt.
 * - Comments & Blank Lines
 * - Expansion of Variables $$
 * - Built-in Commands
 * - Executing Other Commands
 * - Input & Output Redirection
 * - Executing Commands in Foreground & Background
 * - Signals SIGINT & SIGTSTP
 */
 
#include <stdio.h> // main
#include <stdlib.h> // main malloc
#include <string.h> // string
#include <unistd.h> // fork // exec
#include <sys/wait.h> // waitpid
#include <fcntl.h> // O_RDONLY
#include <stdbool.h> // bool
#include <signal.h> // sig

// Constants
#define MAXLENCMD 2048 
#define MAXARGS 512 
#define DELIM " \n"
#define EXPANDVAR "$$"

/* struct to hold & processes */
typedef struct node{ // linked list background process struct
  int pid; 
  struct node *next;
}node;

// Linked List
void push_node(node **head, int new_pid);
void remove_node(struct node **head, int pid);
void check_node(struct node *head);
void kill_node(struct node *head);

// Handlers
void handle_SIGINT(int sig);
void handle_SIGSTP(int sig);

// Required Functions
int cmd_status(char **args);
int cmd_cd(char **args);
int cmd_exit(char **args);

//
int num_funcs();
int std_exe(char **args, bool bg);
char *expand(char *arg);
int builtInExe(char **args);
char **split_line(char *line);
char *read_line();
void shell();


int fgpid = 0; // currently runngin foreground child process
int lastStat = 0; // last exit status // default 0
bool background = true; // background available

struct node *child = NULL; // global linked list

/* array of built in functions, parallel to name array */
int (*funcArray[])(char **) = {
  &cmd_exit,
  &cmd_cd,
  &cmd_status
};

/* string array of built in function names */
char *funcNames[] = {
  "exit",
  "cd",
  "status",
};

// Main
int main(int argc, char **argv){
  shell();

  free(child);

  return 0;
}


//-Linked List
/* adds new child to end of linked list */
void push_node(node **head, int new_pid){ 
  if(new_pid != -1){
    struct node* new_child = (struct node*) malloc(sizeof(struct node)); 
    new_child->pid = new_pid; 
    new_child->next = *head; 
    *head = new_child; 
  }
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

// TODO:
/* Checks removed node exit status */
void check_node(struct node *head){ 
  if(head == NULL){
    return;
  }

  pid_t pid;
  int status;
  while (head != NULL){ 
    pid = waitpid(head->pid, &status, WNOHANG);
    if(pid == head->pid){
      fprintf(stdout, "%d Exited | Status %d\n", head->pid, WEXITSTATUS(status));
      remove_node(&child, head->pid);
    }
    head = head->next; 
  } 
  fflush(stdout);

  return;
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

//-- Handlers
/* handles SIGINT signals */
void handle_SIGINT(int sig){
  kill(fgpid, SIGINT);
}

/* handles SIGTSTP signals */
void handle_SIGSTP(int sig){
  if(background == true){
    background = false;
  }
  else if(background == false){
    background = true;
  }
}

//-- Built In
/* displays contents of name array */
int cmd_status(char **args){
  fprintf(stdout, "%d\n", lastStat);
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
  return sizeof(funcNames) / sizeof(char *);
}

//-- Main Functions
/* executes standard functions */
int std_exe(char **args, bool bg){
  pid_t pid;
  int i = 0;
  int targetin = 0;
  int targetout = 0;
  int status;

	pid = fork();
  
	switch (pid){
		case -1: // error
			perror("fork() failed");
      fflush(stdout);
			exit(1);
			break;

		case 0: // child
      while(args[i] != NULL){ // find redirect
        if(strcmp(args[i], "<") == 0){ // input
          args[i] = NULL; // remove < arg
          i++; // increment to filename
          targetin = open(args[i], O_RDONLY);
          args[i] = NULL;
          dup2(targetin, 0);
        }
        else if(strcmp(args[i], ">") == 0){ // output
          args[i] = NULL; // remove > arg
          i++; // increment to filename
          targetout = open(args[i], O_WRONLY | O_CREAT | O_TRUNC, 0640);
          args[i] = NULL; // remove file arg
          dup2(targetout, 1);
        }
        i++;
      }
      
      if(bg == true){ // background
        if(targetin == 0){ // no input
          targetin = open("/dev/null", O_RDONLY);
          dup2(targetin, 0);
        }
        if(targetout == 0){ // no output
          targetout = open("/dev/null", O_WRONLY | O_CREAT | O_TRUNC, 0640);
          dup2(targetout, 1);
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
      lastStat = WEXITSTATUS(status); // save last process exit status
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

  return result; // needs free()
}

// builtInExe
/* executed built in commands */
int builtInExe(char **args){

  // Check empty or comment
  if(args[0] == NULL || strchr(args[0], '#') != NULL) {
    return 1;
  }

  int i = 0;
  while(args[i] != NULL){ // check if args has EXPANDVAR
    if(strstr(args[i], EXPANDVAR)){ 
      args[i] = expand(args[i]);
    }
    i++;
  }

  bool bg = false; // background true/false
  i--; // bring back to last arg
  if(strcmp(args[i], "&") == 0){ // last is &
    args[i] = NULL;
    bg = true; // do in background mode
  }

  if(background == false){ // check if background available
    bg = false;
  }

  for(i = 0; i < num_funcs(); ++i){ // check if args[i] is built in
    if (strcmp(args[0], funcNames[i]) == 0){
      return (*funcArray[i])(args);
    }
  }

  return std_exe(args, bg); // else args[i] is not built in
}

/* parses inputed command into array of pointers */
char **split_line(char *line){
  int buff = MAXARGS;
  int position = 0;
  char **tokens = malloc(buff * sizeof(char*));
  char *token, **tokens_backup;

  token = strtok(line, DELIM);
  while (token != NULL) {
    tokens[position] = token;
    position++;
    token = strtok(NULL, DELIM);
  }
  tokens[position] = NULL;

  return tokens;
}

/* reads in stdin command */
char *read_line(){
  char *line = NULL;
  size_t buff = MAXLENCMD;

  signal(SIGINT, handle_SIGINT);
  signal(SIGTSTP, handle_SIGSTP); 

  printf(": ");
  getline(&line, &buff, stdin);

  return line;
}

// shell
/* main shell interface, read, parse, execute */
void shell(){
  char *line;
  char **args;
  int run;

  do{
    check_node(child); // check if background processes have exited

    line = read_line(); // get new cmd
    args = split_line(line); // parse cmd
    run = builtInExe(args); // execute cmd

    free(line);
    free(args);
  } while(run);
}


/* Sources
 * C Programming Book
 * OSU Tutor
 *
 *
 * 
*/