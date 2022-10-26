/*****************************************************
 * Copyright Grégory Mounié 2008-2015                *
 *           Simon Nieuviarts 2002-2009              *
 * This code is distributed under the GLPv3 licence. *
 * Ce code est distribué sous la licence GPLv3+.     *
 *****************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#include "variante.h"
#include "readcmd.h"

#ifndef VARIANTE
#error "Variante non défini !!"
#endif

typedef struct jobs {
    pid_t pid;
    char *cmd;
    struct jobs *next;
} jobs; 

jobs *jobs_head = NULL; //list of jobs


/* Guile (1.8 and 2.0) is auto-detected by cmake */
/* To disable Scheme interpreter (Guile support), comment the
 * following lines.  You may also have to comment related pkg-config
 * lines in CMakeLists.txt.
 */

#if USE_GUILE == 1
#include <libguile.h>

int question6_executer(char *line)
{
	/* Question 6: Insert your code to execute the command line
	 * identically to the standard execution scheme:
	 * parsecmd, then fork+execvp, for a single command.
	 * pipe and i/o redirection are not required.
	 */
	struct cmdline *l;
    l = parsecmd(&line);
    exec_commands(l);
    
	return 0;
}

SCM executer_wrapper(SCM x)
{
        return scm_from_int(question6_executer(scm_to_locale_stringn(x, 0)));
}
#endif


void terminate(char *line) {
#if USE_GNU_READLINE == 1
	/* rl_clear_history() does not exist yet in centOS 6 */
	clear_history();
#endif
	if (line)
	  free(line);
	printf("exit\n");
	exit(0);
}

void add_jobs(pid_t pid, char *cmd){
	//insert in front of the list
	jobs *new_job = malloc(sizeof(jobs)); //allocate memory for the new job
   	new_job->pid = pid;

   	 new_job->cmd = (char *) malloc(sizeof(cmd) + 1);
   	 strcpy(new_job->cmd, cmd);
    
	new_job->next = jobs_head;
    
	jobs_head = new_job;

}

void print_jobs(){
	if (jobs_head == NULL) {
        printf("no background job running\n");
        return;
    }

	else{
		jobs *current = jobs_head;
		while(current!= NULL){
			printf("PID: %d Command: %s \n", current->pid, current->cmd);
			current = current->next;
		}
	}
}

void delete_jobs(){
	//delete jobs list
	jobs *current = jobs_head;

	while(jobs_head!=NULL){
		current = jobs_head;
		free(current);
		jobs_head = jobs_head->next;
	}
}

void exec_pipe(struct cmdline *line){
	
	int pipefd[2];	

	pipe(pipefd);
	
	pid_t pid = fork();

	switch(pid){
	case -1:
		perror("fork");
	break;

	case 0: //child
		dup2(pipefd[1], 1);
		close(pipefd[0]); 
		close(pipefd[1]);
		execvp(line->seq[0][0],line->seq[0]);
	break;

	default:
		dup2(pipefd[0], 0); 
		close(pipefd[1]); 
		close(pipefd[0]);
		execvp(line->seq[1][0], line->seq[1]);

	break;
	}
}


void execute(struct cmdline *line){
	pid_t pid = fork();


	switch (pid){
	case -1:
		perror("fork:");
		break;
		
	case 0:
		//child
		if (line->seq[1]) //if there is a second command, so make a pipe
			exec_pipe(line);
		
        if (strcmp(*line->seq[0], "jobs") == 0)
            print_jobs();
		
		else
			execvp(line->seq[0][0], line->seq[0]); //execute command
		
		break;
		
	default:
	//parent
		if(!line->bg){
			delete_jobs();
			int status;
			waitpid(pid, &status, 0);
		}
		else{
			add_jobs(pid, *line->seq[0]);
		}
	break;
	}
}

int main() {

	
		printf("Variante %d: %s\n", VARIANTE, VARIANTE_STRING);

#if USE_GUILE == 1
        scm_init_guile();
        /* register "executer" function in scheme */
        scm_c_define_gsubr("executer", 1, 0, 0, executer_wrapper);
#endif

	while (1) {
		struct cmdline *l;
		char *line=0;
		int i, j;
		char *prompt = "ensishell>";

		/* Readline use some internal memory structure that
		   can not be cleaned at the end of the program. Thus
		   one memory leak per command seems unavoidable yet */
		line = readline(prompt);
		if (line == 0 || ! strncmp(line,"exit", 4)) {
			terminate(line);
		}

#if USE_GNU_READLINE == 1
		add_history(line);
#endif


#if USE_GUILE == 1
		/* The line is a scheme command */
		if (line[0] == '(') {
			char catchligne[strlen(line) + 256];
			sprintf(catchligne, "(catch #t (lambda () %s) (lambda (key . parameters) (display \"mauvaise expression/bug en scheme\n\")))", line);
			scm_eval_string(scm_from_locale_string(catchligne));
			free(line);
                        continue;
                }
#endif

		/* parsecmd free line and set it up to 0 */
		l = parsecmd( & line);

		/* If input stream closed, normal termination */
		if (!l) {
		  
			terminate(0);
		}
		

		
		if (l->err) {
			/* Syntax error, read another command */
			printf("error: %s\n", l->err);
			continue;
		}

		// if (l->in) printf("in: %s\n", l->in);
		// if (l->out) printf("out: %s\n", l->out);
		// if (l->bg) printf("background (&)\n");

		/* Display each command of the pipe */
		 for (i=0; l->seq[i]!=0; i++) {
		 	char **cmd = l->seq[i];
		 	printf("seq[%d]:", i);
                         for (j=0; cmd[j]!=0; j++) {
                                 printf("'%s' ", cmd[j]);
                         }
		 	printf("\n");
		 }

		/* Execute commands */
        execute(l);
		
	}
}
