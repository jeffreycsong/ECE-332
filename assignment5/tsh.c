/* 
 * tsh - A tiny shell program with job control
 * 
 * <Put your name and login ID here>
 * Jeffrey Song     32476074
 * Byung Woong Ko   31292688
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>

/* Misc manifest constants */
#define MAXLINE    1024   /* max line size */
#define MAXARGS     128   /* max args on a command line */
#define MAXJOBS      16   /* max jobs at any point in time */
#define MAXJID    1<<16   /* max job ID */

/* Job states */
#define UNDEF 0 /* undefined */
#define FG 1    /* running in foreground */
#define BG 2    /* running in background */
#define ST 3    /* stopped */

/* 
 * Jobs states: FG (foreground), BG (background), ST (stopped)
 * Job state transitions and enabling actions:
 *     FG -> ST  : ctrl-z
 *     ST -> FG  : fg command
 *     ST -> BG  : bg command
 *     BG -> FG  : fg command
 * At most 1 job can be in the FG state.
 */

/* Global variables */
extern char **environ;      /* defined in libc */
char prompt[] = "tsh> ";    /* command line prompt (DO NOT CHANGE) */
int verbose = 0;            /* if true, print additional output */
int nextjid = 1;            /* next job ID to allocate */
char sbuf[MAXLINE];         /* for composing sprintf messages */

struct job_t {              /* The job struct */
    pid_t pid;              /* job PID */
    int jid;                /* job ID [1, 2, ...] */
    int state;              /* UNDEF, BG, FG, or ST */
    char cmdline[MAXLINE];  /* command line */
};
struct job_t jobs[MAXJOBS]; /* The job list */
/* End global variables */


/* Function prototypes */

/* Here are the functions that you will implement */
void eval(char *cmdline);
int builtin_cmd(char **argv);
void do_bgfg(char **argv);
void waitfg(pid_t pid);

void sigchld_handler(int sig);
void sigtstp_handler(int sig);
void sigint_handler(int sig);

/* Here are helper routines that we've provided for you */
int parseline(const char *cmdline, char **argv); 
void sigquit_handler(int sig);

void clearjob(struct job_t *job);
void initjobs(struct job_t *jobs);
int maxjid(struct job_t *jobs); 
int addjob(struct job_t *jobs, pid_t pid, int state, char *cmdline);
int deletejob(struct job_t *jobs, pid_t pid); 
pid_t fgpid(struct job_t *jobs);
struct job_t *getjobpid(struct job_t *jobs, pid_t pid);
struct job_t *getjobjid(struct job_t *jobs, int jid); 
int pid2jid(pid_t pid); 
void listjobs(struct job_t *jobs);

void usage(void);
void unix_error(char *msg);
void app_error(char *msg);
typedef void handler_t(int);
handler_t *Signal(int signum, handler_t *handler);

/*
 * main - The shell's main routine 
 */
int main(int argc, char **argv) 
{
    char c;
    char cmdline[MAXLINE];
    int emit_prompt = 1; /* emit prompt (default) */

    /* Redirect stderr to stdout (so that driver will get all output
     * on the pipe connected to stdout) */
    dup2(1, 2);

    /* Parse the command line */
    while ((c = getopt(argc, argv, "hvp")) != EOF) {
        switch (c) {
        case 'h':             /* print help message */
            usage();
	    break;
        case 'v':             /* emit additional diagnostic info */
            verbose = 1;
	    break;
        case 'p':             /* don't print a prompt */
            emit_prompt = 0;  /* handy for automatic testing */
	    break;
	default:
            usage();
	}
    }

    /* Install the signal handlers */

    /* These are the ones you will need to implement */
    Signal(SIGINT,  sigint_handler);   /* ctrl-c */
    Signal(SIGTSTP, sigtstp_handler);  /* ctrl-z */
    Signal(SIGCHLD, sigchld_handler);  /* Terminated or stopped child */

    /* This one provides a clean way to kill the shell */
    Signal(SIGQUIT, sigquit_handler); 

    /* Initialize the job list */
    initjobs(jobs);

    /* Execute the shell's read/eval loop */
    while (1) {

	/* Read command line */
	if (emit_prompt) {
	    printf("%s", prompt);
	    fflush(stdout);
	}
	if ((fgets(cmdline, MAXLINE, stdin) == NULL) && ferror(stdin))
	    app_error("fgets error");
	if (feof(stdin)) { /* End of file (ctrl-d) */
	    fflush(stdout);
	    exit(0);
	}

	/* Evaluate the command line */
    int fd0 = dup(STDIN_FILENO);
    int fd1 = dup(STDOUT_FILENO);
    int fd2 = dup(STDERR_FILENO);

	eval(cmdline);

    dup2(fd0, STDIN_FILENO);
    dup2(fd1, STDOUT_FILENO);
    dup2(fd2, STDERR_FILENO);

	//fflush(stdout);
	fflush(stdout);
    } 

    exit(0); /* control never reaches here */
}
  
/* Contributions: Byung Woong Ko
 * eval - Evaluate the command line that the user has just typed in
 * 
 * If the user has requested a built-in command (quit, jobs, bg or fg)
 * then execute it immediately. Otherwise, fork a child process and
 * run the job in the context of the child. If the job is running in
 * the foreground, wait for it to terminate and then return.  Note:
 * each child process must have a unique process group ID so that our
 * background children don't receive SIGINT (SIGTSTP) from the kernel
 * when we type ctrl-c (ctrl-z) at the keyboard.  
*/
void eval(char *cmdline) {

    char *argv[MAXARGS]; // initialize argument variable
    char buf[MAXLINE]; // intialize buffer for modified cmdline (same as prof code)
    strcpy(buf, cmdline); // copy cmdline in to buffer
    
    int BGorFG = 1; //variable for later to assign job to FG or BG, set to foreground by default

    if(parseline(buf, argv)==1) { // set to background if parseline returns 1
        BGorFG = 2;
    }
    
    if(argv[0] == NULL) { // return if argv is null
        return;
    }

    if(builtin_cmd(argv) == 0) {
      
        sigset_t set; // set signal

        sigemptyset(&set); // intialize signal set
        sigaddset(&set,SIGCHLD); //add SIGCHILD to the set
        sigprocmask(SIG_BLOCK, &set, NULL); // block the signal

        
        pid_t pid0; // for the process

        if((pid0 = fork()) == 0) { // if child
         
            // redirection variables
            int file;
            int redirection = 0;

            // parsing variables
            char *args[MAXARGS][MAXARGS];
            int row = 0;
            int col = 0;
            
            int numPipes = 0; // piping prep variables

            int i = 0;
            while (i < MAXARGS) {

                if(argv[i] == NULL) {
                    break;
                }
                else if (!strcmp(argv[i], "<")) { // open file, redirection I/O, and close file we opened       
                    redirection++;
                    file = open(argv[i+1], O_RDONLY | O_CREAT, S_IRWXU);
                    dup2(file, STDIN_FILENO);         
                    close(file);                    
                }   
                else if (!strcmp(argv[i], ">")) { // open file, redirection I/O, and close file we opened 
                    redirection++;
                    file = open(argv[i+1], O_WRONLY|O_CREAT, S_IRWXU|S_IRWXG|S_IRWXO); 
                    dup2(file, STDOUT_FILENO);       
                    close(file);   
                }
                else if (!strcmp(argv[i], ">>")) { //open file, redirection I/O, and close file we opened 
                    redirection++;  
                    file = open(argv[i+1], O_WRONLY | O_CREAT | O_APPEND, S_IRWXU | S_IRWXG | S_IRWXO);
                    dup2(file, STDOUT_FILENO);
                    close(file);
                }    
                else if (!strcmp(argv[i], "2>")) { //open file, redirection I/O, and close file we opened 
                    redirection++;   
                    file = open(argv[i+1], O_WRONLY|O_CREAT, S_IRWXU|S_IRWXG|S_IRWXO);
                    dup2(file, STDERR_FILENO);
                    close(file);
                }   
                else if(!strcmp(argv[i], "|")) { //count number of pipes
                    numPipes++;
                }
                if(!strcmp(argv[i], "|") || !strcmp(argv[i], ">") || !strcmp(argv[i], ">>") || !strcmp(argv[i], "2>") || !strcmp(argv[i], "<")){ // input parsing
                        row++;
                        col = 0;
                    }
                    else {
                        args[row][col++] = argv[i];
                        args[row][col] = NULL;
                    }
                i++;    
                }
            
            if(redirection >= 1) { // execute redirection command
                execvp(args[0][0], args[0]);
            }
    
            if( numPipes >= 1) { //execute piping
                int pipeArray[numPipes][2];
                int i = 0;

                while(i < numPipes){ // creating pipes
                    pipe(pipeArray[i]);
                    i++;
                }
                
                for(int i = 0; i <= (numPipes); i++) { // doing piping
                    pid_t pid;
                    if ((pid = fork()) == 0){
                        
                        if(i == 0) { //intial arg/pipe
                            dup2(pipeArray[i][1],STDOUT_FILENO);
                        }

                        else if (i != numPipes){ // intermediary pipes
                            dup2(pipeArray[i-1][0],STDIN_FILENO);
                            dup2(pipeArray[i][1],STDOUT_FILENO);
                        }

                        else if(i == numPipes) { // final arg/pipe
                            dup2(pipeArray[i-1][0],STDIN_FILENO);
                        }
                        execvp(args[i][0], args[i]);
                    }
                    else if (pid >= 1){ //close everything we dup2 in child
                        if(i == 0) {
                            close(pipeArray[i][1]);
                        }
                        else if (i != numPipes){
                            close(pipeArray[i-1][0]);
                            close(pipeArray[i][1]);
                        }
                        else if(i == numPipes) {
                            close(pipeArray[i-1][0]);
                        }  
                        waitpid(pid, NULL, 0);                                
                    }
                    
                }
            }
            
            if(numPipes == 0 && redirection == 0) {
                sigprocmask(SIG_UNBLOCK, &set, NULL); // block signals
                setpgid(0,0); // assign child to new group as group leader
                if(execve(argv[0], argv, environ)) { // check if command exists
                    printf("%s: Command not found\n", argv[0]);
                    fflush(stdout);
                    exit(0); 
                }
            }
            else { // if we did not exec previously
                fflush(stdout);
                exit(0);
            }
            
        }
        else { // if parent
            addjob(jobs, pid0, BGorFG, cmdline); // add job to processes
            sigprocmask(SIG_UNBLOCK, &set, NULL); // unblock signals
            if(BGorFG == 2) { // if BGorFG is background, 
                struct job_t *job = getjobpid(jobs, pid0);
                printf("[%d] (%d) %s",job->jid,job->pid,job->cmdline);
            }
            else {
                waitfg(pid0); // wait for foreground process to finish
            }

        }
    }
    fflush(stdout);
    return;
}

/* 
 * parseline - Parse the command line and build the argv array.
 * 
 * Characters enclosed in single quotes are treated as a single
 * argument.  Return true if the user has requested a BG job, false if
 * the user has requested a FG job.  
 */
int parseline(const char *cmdline, char **argv) 
{
    static char array[MAXLINE]; /* holds local copy of command line */
    char *buf = array;          /* ptr that traverses command line */
    char *delim;                /* points to first space delimiter */
    int argc;                   /* number of args */
    int bg;                     /* background job? */

    strcpy(buf, cmdline);
    buf[strlen(buf)-1] = ' ';  /* replace trailing '\n' with space */
    while (*buf && (*buf == ' ')) /* ignore leading spaces */
	buf++;

    /* Build the argv list */
    argc = 0;
    if (*buf == '\'') {
	buf++;
	delim = strchr(buf, '\'');
    }
    else {
	delim = strchr(buf, ' ');
    }

    while (delim) {
	argv[argc++] = buf;
	*delim = '\0';
	buf = delim + 1;
	while (*buf && (*buf == ' ')) /* ignore spaces */
	       buf++;

	if (*buf == '\'') {
	    buf++;
	    delim = strchr(buf, '\'');
	}
	else {
	    delim = strchr(buf, ' ');
	}
    }
    argv[argc] = NULL;
    
    if (argc == 0)  /* ignore blank line */
	return 1;

    /* should the job run in the background? */
    if ((bg = (*argv[argc-1] == '&')) != 0) {
	argv[--argc] = NULL;
    }
    return bg;
}

/* Contributions: Byung Woong Ko
 * builtin_cmd - If the user has typed a built-in command then execute
 *    it immediately.  
 */
int builtin_cmd(char **argv){

    // If user inputs "quit"  
  if (strcmp(argv[0], "quit") == 0){                       
    exit(0);            // Terminate the shell
  }

    // If user inputs either "bg" or "fg" 
  else if(strcmp(argv[0], "bg") == 0 || strcmp(argv[0], "fg") == 0){    
    do_bgfg(argv);      // Do bgfg
    return 1;           // Builtin command, return 1
  }

    // If user input "jobs"
  else if(strcmp(argv[0], "jobs") == 0){                                
    listjobs(jobs);     // List jobs, given function
    return 1;           // Builtin command, return 1
  }
  
    //input does not match builtin commands
  else {
    return 0;           // Not builtin command, return 0
  }
}

/* Contributions: Byung Woong Ko
 * do_bgfg - Execute the builtin bg and fg commands
 */
void do_bgfg(char **argv){
  struct job_t* job;        // Initialize job
  int jid;                  // initialize jobID 
  char* id = argv[1];       // The argv[1] holds either jobs PID or JID, following argv[0] command

  if (id == NULL){          // If user did not enter job/ process id
    printf("%s command requires PID or %%jobid argument\n", argv[0]);  // Print error statement
    fflush(stdout);
    return;
    }

//Check if PID or JID
  if (isdigit(id[0])) {     // If first bit of id is a number, then its PID
    pid_t pid1 = atoi(id);                      // Convert id string to int and store in pid1 
    // If PID entered doesn't match a job
    if ((job = getjobpid(jobs,pid1)) == 0) {   
      printf("(%d): no such process\n", pid1);  // Print error statement if process not found
      return; 
    }
  }
  else if (id[0] == '%') {  // If first bit of id is "%", then its JID 
    jid = atoi(&id[1]);                         // Convert id string to int and store in jid
    // If jid entered does not match a job
    if ((job = getjobjid(jobs,jid)) == 0) {  
      printf("%%%d: no such job\n", jid);       // Print error if job not found
      return;
    }
  }
  // If argument was neither PID nor JID 
  else {                    
    printf("%s: argument must be a PID or %%jobid\n", argv[0]);     // Print error statement
    return;
  }

  // Execute
  if (kill(-(job->pid), SIGCONT) < 0) {     // Start job, check if a process matches PID 
    if (errno != ESRCH) {                   // Check for unexpected error
      printf("kill error!\n");              // Print kill error message if found
    }    
  }
  
    // Check for bg or fg
  if (strcmp(argv[0], "bg") == 0) {         // If executing in background 
    job->state = BG;                        // Set job's state to background 
    printf("[%d] (%d) %s", job->jid, job->pid, job->cmdline);  // Print background job's info
  }
  
  else if (strcmp("fg", argv[0]) == 0) {    // If executing in foreground 
    job->state = FG;        // Set job's state to foreground 
    waitfg(job->pid);       // Call waitfg on the current job 
  }
  
  else {    // If neither foreground nor background 
    printf("bg/fg error: %s\n", argv[0]);  // Print error message 
  }
  return;
}

/* Contributions: Jeffrey Song
 * waitfg - Block until process pid is no longer the foreground process
 */
void waitfg(pid_t pid)
{
	struct job_t *job = getjobpid(jobs, pid);
	
	
	while(job->state==FG){ // while process ID is same as FG ID
        sleep(1);
    }

    return;

}

/*****************
 * Signal handlers
 *****************/

/* Contributions: Jeffrey Song
 * sigchld_handler - The kernel sends a SIGCHLD to the shell whenever
 *     a child job terminates (becomes a zombie), or stops because it
 *     received a SIGSTOP or SIGTSTP signal. The handler reaps all
 *     available zombie children, but doesn't wait for any other
 *     currently running children to terminate.  
 */
void sigchld_handler(int sig) 
{
    pid_t pid;
    int status;

    while ((pid = waitpid(fgpid(jobs), &status, WUNTRACED|WNOHANG)) > 0) {

        if (pid < 1)
            break;
        
    	if(WIFEXITED(status)) // check if exited

            deletejob(jobs, pid);      //delete the child from the list
        
        //check if child process terminated 
        else if(WIFSIGNALED(status)){

            printf("Job [%d] (%d) terminated by signal %d\n", pid2jid(pid), pid, WTERMSIG(status));
            fflush(stdout);
            deletejob(jobs,pid);        //delete the child from the list

        }

        //check if child that cause the return is currently stopped
        else if(WIFSTOPPED(status)){

            printf("Job [%d] (%d) stopped by signal %d\n", pid2jid(pid), pid, WSTOPSIG(status));
            struct job_t *job=getjobpid(jobs,pid);
            fflush(stdout);
            job = getjobpid(jobs, pid);
            job->state=ST;      //change job status to ST
            
        }
    }
    return;
}

/* Contributions: Jeffrey Song
 * sigint_handler - The kernel sends a SIGINT to the shell whenver the
 *    user types ctrl-c at the keyboard.  Catch it and send it along
 *    to the foreground job.  
 */
void sigint_handler(int sig) 
{
    pid_t pid = fgpid(jobs); // initialize
    if (pid != 0) { 
        kill(-pid, sig); // kills pid
    }
    return;
}

/* Contributions: Jeffrey Song
 * sigtstp_handler - The kernel sends a SIGTSTP to the shell whenever
 *     the user types ctrl-z at the keyboard. Catch it and suspend the
 *     foreground job by sending it a SIGTSTP.  
 */
void sigtstp_handler(int sig) 
{
    int pid = fgpid(jobs);  // initialize pid as the process ID using fgpid() 
    int jid = pid2jid(pid);  // inititalize jid as the job id 

    if (pid > 0) {   // if pid exists

        struct job_t *temp = getjobpid(jobs, pid);
        temp->state = ST;  // update job's pid to ST for stopped
        printf("Job [%d] (%d) stopped by signal %d\n", jid, pid, sig);  // print job stopping message
        kill(-pid, sig);  // end job
    
  }

    return;
    
}

/*********************
 * End signal handlers
 *********************/

/***********************************************
 * Helper routines that manipulate the job list
 **********************************************/

/* clearjob - Clear the entries in a job struct */
void clearjob(struct job_t *job) {
    job->pid = 0;
    job->jid = 0;
    job->state = UNDEF;
    job->cmdline[0] = '\0';
}

/* initjobs - Initialize the job list */
void initjobs(struct job_t *jobs) {
    int i;

    for (i = 0; i < MAXJOBS; i++)
	clearjob(&jobs[i]);
}

/* maxjid - Returns largest allocated job ID */
int maxjid(struct job_t *jobs) 
{
    int i, max=0;

    for (i = 0; i < MAXJOBS; i++)
	if (jobs[i].jid > max)
	    max = jobs[i].jid;
    return max;
}

/* addjob - Add a job to the job list */
int addjob(struct job_t *jobs, pid_t pid, int state, char *cmdline) 
{
    int i;
    
    if (pid < 1)
	return 0;

    for (i = 0; i < MAXJOBS; i++) {
	if (jobs[i].pid == 0) {
	    jobs[i].pid = pid;
	    jobs[i].state = state;
	    jobs[i].jid = nextjid++;
	    if (nextjid > MAXJOBS)
		nextjid = 1;
	    strcpy(jobs[i].cmdline, cmdline);
  	    if(verbose){
	        printf("Added job [%d] %d %s\n", jobs[i].jid, jobs[i].pid, jobs[i].cmdline);
            }
            return 1;
	}
    }
    printf("Tried to create too many jobs\n");
    return 0;
}

/* deletejob - Delete a job whose PID=pid from the job list */
int deletejob(struct job_t *jobs, pid_t pid) 
{
    int i;

    if (pid < 1)
	return 0;

    for (i = 0; i < MAXJOBS; i++) {
	if (jobs[i].pid == pid) {
	    clearjob(&jobs[i]);
	    nextjid = maxjid(jobs)+1;
	    return 1;
	}
    }
    return 0;
}

/* fgpid - Return PID of current foreground job, 0 if no such job */
pid_t fgpid(struct job_t *jobs) {
    int i;

    for (i = 0; i < MAXJOBS; i++)
	if (jobs[i].state == FG)
	    return jobs[i].pid;
    return 0;
}

/* getjobpid  - Find a job (by PID) on the job list */
struct job_t *getjobpid(struct job_t *jobs, pid_t pid) {
    int i;

    if (pid < 1)
	return NULL;
    for (i = 0; i < MAXJOBS; i++)
	if (jobs[i].pid == pid)
	    return &jobs[i];
    return NULL;
}

/* getjobjid  - Find a job (by JID) on the job list */
struct job_t *getjobjid(struct job_t *jobs, int jid) 
{
    int i;

    if (jid < 1)
	return NULL;
    for (i = 0; i < MAXJOBS; i++)
	if (jobs[i].jid == jid)
	    return &jobs[i];
    return NULL;
}

/* pid2jid - Map process ID to job ID */
int pid2jid(pid_t pid) 
{
    int i;

    if (pid < 1)
	return 0;
    for (i = 0; i < MAXJOBS; i++)
	if (jobs[i].pid == pid) {
            return jobs[i].jid;
        }
    return 0;
}

/* listjobs - Print the job list */
void listjobs(struct job_t *jobs) 
{
    int i;
    
    for (i = 0; i < MAXJOBS; i++) {
	if (jobs[i].pid != 0) {
	    printf("[%d] (%d) ", jobs[i].jid, jobs[i].pid);
	    switch (jobs[i].state) {
		case BG: 
		    printf("Running ");
		    break;
		case FG: 
		    printf("Foreground ");
		    break;
		case ST: 
		    printf("Stopped ");
		    break;
	    default:
		    printf("listjobs: Internal error: job[%d].state=%d ", 
			   i, jobs[i].state);
	    }
	    printf("%s", jobs[i].cmdline);
	}
    }
}
/******************************
 * end job list helper routines
 ******************************/


/***********************
 * Other helper routines
 ***********************/

/*
 * usage - print a help message
 */
void usage(void) 
{
    printf("Usage: shell [-hvp]\n");
    printf("   -h   print this message\n");
    printf("   -v   print additional diagnostic information\n");
    printf("   -p   do not emit a command prompt\n");
    exit(1);
}

/*
 * unix_error - unix-style error routine
 */
void unix_error(char *msg)
{
    fprintf(stdout, "%s: %s\n", msg, strerror(errno));
    exit(1);
}

/*
 * app_error - application-style error routine
 */
void app_error(char *msg)
{
    fprintf(stdout, "%s\n", msg);
    exit(1);
}

/*
 * Signal - wrapper for the sigaction function
 */
handler_t *Signal(int signum, handler_t *handler) 
{
    struct sigaction action, old_action;

    action.sa_handler = handler;  
    sigemptyset(&action.sa_mask); /* block sigs of type being handled */
    action.sa_flags = SA_RESTART; /* restart syscalls if possible */

    if (sigaction(signum, &action, &old_action) < 0)
	unix_error("Signal error");
    return (old_action.sa_handler);
}

/*
 * sigquit_handler - The driver program can gracefully terminate the
 *    child shell by sending it a SIGQUIT signal.
 */
void sigquit_handler(int sig) 
{
    printf("Terminating after receipt of SIGQUIT signal\n");
    exit(1);
}