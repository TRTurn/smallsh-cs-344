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

#define CMDMAX 2048 // max command length
#define ARGMAX 512 // max arguments
#define DELIM " \n" // delimitor(s)
#define VAREXP "$$" // variable to expand
int fgpid = 0; // currently runngin foreground child process
int lastStat = 0; // last exit status // default 0
bool background = true; // background available

//------------------------------------------------------------Linked List
//______________________________________________struct child
/* struct to hold & processes
 */
typedef struct child{ // linked list bagckground process struct
  int pid; 
  struct child *next;
}child;

child *chld = NULL; // global linked list

//______________________________________________pushCHLD
/* adds new child to end of linked list
 */
void pushCHLD(child **head, int new_pid){ 
  if(new_pid != -1){
    struct child* new_child = (struct child*) malloc(sizeof(struct child)); 
    new_child->pid = new_pid; 
    new_child->next = *head; 
    *head = new_child; 
  }
} 

//______________________________________________removeCHLD
/* removes exited child from linked list
 */
void removeCHLD(struct child **head, int pid){ 
  struct child* temp = *head, *prev; 
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

//______________________________________________checkCHLD
/* removes exited child from linked list
 */
void checkCHLD(struct child *head){ 
  if(head == NULL){
    return;
  }

  pid_t pid;
  int status;
  while (head != NULL){ 
    pid = waitpid(head->pid, &status, WNOHANG);
    if(pid == head->pid){
      fprintf(stdout, "%d Exited | Status %d\n", head->pid, WEXITSTATUS(status));
      removeCHLD(&chld, head->pid);
    }
    head = head->next; 
  } 
  fflush(stdout);

  return;
} 

//______________________________________________killCHLD
/* removes exited child from linked list
 */
void killCHLD(struct child *head){ 
  if(head == NULL){
    return;
  }

  while(head != NULL){ 
    kill(head->pid, SIGKILL);
    head = head->next;
  } 

  return;
} 

//---------------------------------------------------------------Handlers
//______________________________________________handle_INT
/* handles SIGINT signals
 */
void handle_INT(int sig){
  kill(fgpid, SIGINT);
}

//______________________________________________handle_TSTP
/* handles SIGTSTP signals
 */
void handle_TSTP(int sig){
  if(background == true){
    background = false;
  }
  else if(background == false){
    background = true;
  }
}

//---------------------------------------------------------------Built In
//______________________________________________status
/* displays contents of name array
 */
int cmd_status(char **args){
  fprintf(stdout, "%d\n", lastStat);
  fflush(stdout);

  return 1;
}

//______________________________________________cd
/* cd command
 */
int cmd_cd(char **args){
  if (args[1] == NULL){
    chdir(getenv("HOME"));
  } 
  else{
    if (chdir(args[1]) != 0){
      perror("error changing to directory");
      fflush(stdout);
      exit(1);
    }
  }

  return 1;
}

//______________________________________________exit
/* ends shell
 */
int cmd_exit(char **args){
  killCHLD(chld);
  kill(fgpid, SIGKILL);

  return 0;
}

//______________________________________________builtFunctions[]
/* array of built in functions, parallel to name array
 */
int (*builtFunctions[])(char **) = {
  &cmd_exit,
  &cmd_cd,
  &cmd_status
};

//______________________________________________builtNames[]
/* string array of built in function names
 */
char *builtNames[] = {
  "exit",
  "cd",
  "status",
};

//______________________________________________size of function array
/* returns number of functions in function array
 */
int builtSize(){
  return sizeof(builtNames) / sizeof(char *);
}

//---------------------------------------------------------Main Functions
//______________________________________________nativeExe
/* executes non-built in functions
 */
int nativeExe(char **args, bool bg){
  pid_t pid;
  int i = 0;
  int targetin = 0;
  int targetout = 0;
  int status;

  //signal(SIGCHLD, handle_CHLD);
	pid = fork();
  
	switch (pid){
		case -1: //error
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
        pushCHLD(&chld, pid);
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

//______________________________________________varExpand
/* replaces $$ with pid
 */
char *varExpand(char *arg){
  pid_t num = getpid(); // get pid in int form
    int pidLen = snprintf(NULL, 0, "%d", num); // get str pid length
    char *pid = malloc(pidLen+1); // make str for pid
    snprintf(pid, pidLen+1, "%d", num); // pid_t -> str
  char var[] = VAREXP;
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

//______________________________________________builtInExe
/* executed built in commands
 */
int builtInExe(char **args){
  if(args[0] == NULL || strchr(args[0], '#') != NULL) { // ignore empty or comment
    return 1;
  }

  int i = 0;
  while(args[i] != NULL){ // check if args has $$
    if(strstr(args[i], VAREXP)){ // if so, replace
      args[i] = varExpand(args[i]);
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

  for(i = 0; i < builtSize(); ++i){ // check if args[i] is built in
    if (strcmp(args[0], builtNames[i]) == 0){
      return (*builtFunctions[i])(args);
    }
  }

  return nativeExe(args, bg); // else args[i] is not built in
}

//______________________________________________splitLine
/* parses inputed command into array of pointers
 */
char **splitLine(char *line){
  int buff = ARGMAX;
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

//______________________________________________readLine
/* reads in stdin command
 */
char *readLine(){
  char *line = NULL;
  size_t buff = CMDMAX;

  signal(SIGINT, handle_INT);
  signal(SIGTSTP, handle_TSTP); 

  printf(": ");
  getline(&line, &buff, stdin);

  return line;
}

//______________________________________________shell
/* main shell interface, read, parse, execute
 */
void shell(){
  char *line;
  char **args;
  int run;

  do{
    checkCHLD(chld); // check if background processes have exited

    line = readLine(); // get new cmd
    args = splitLine(line); // parse cmd
    run = builtInExe(args); // execute cmd

    free(line);
    free(args);
  }while(run); // while 1 continue, 0 exit
}

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++Main
int main(int argc, char **argv){
  shell();

  free(chld);

  return 0;
}