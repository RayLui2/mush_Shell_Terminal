#include "mush.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>

#define READ 0
#define WRITE 1
#define PROMPT "8-P "
#define cd "cd"

void block_sigInt();
void unblock_sigInt();

int main(int argc, char *argv[])
{
    /* declare some variables */
    FILE *input_file;
    char *input_command;
    int i;
    int j;
    int inFD;
    int outFD;
    bool one_arg = false;

    /* check if we will be reading from stdin */
    if(argc == 1)
    {
	/* we are using the commmand line interface */	
        input_file = stdin;
	one_arg = true;
	printf("%s", PROMPT);    
    }

    else if(argc == 2)
    {
	input_file = fopen(argv[1], "r");
	/* check if we can open the input file */
	if(input_file == NULL)
	{
	    perror("Error opening the input file.\n");
	    exit(1);
	}
    }

    else
    {
	perror("Run mush2 with a file with commands, or no args for stdin.\n");
	exit(1);
    }

    /* read the first line */
    input_command = readLongString(input_file);

    /* declare our pipeline */
    pipeline pipe_line;

    /* keep going until EOF */
    while(!feof(input_file))
    {
	pipe_line = crack_pipeline(input_command);
	
	/* me checking if the command is cd */
	clstage cd_stage = &pipe_line->stage[0];

	if(strcmp(cd_stage->argv[0], cd) == 0)
	{
	    /* if they only provided no args */
	    if(cd_stage->argc == 1)
	    {
		const char *home_dir = getenv("HOME");
		if(home_dir == NULL)
		{
		    perror("Home env not set\n");
		}
		if(chdir(home_dir) != 0)
		{
		    perror("chdir");
		}
	    }
	    /* if user provided to many args */
	    else if((cd_stage->argc) > 2)
	    {
		printf("Too many arguments. format: cd <directory>\n");
	    }
	    /* good cd command */
	    else
	    {
		/* see if we can cd */
		if(chdir(cd_stage->argv[1]) == -1)
		{
		    perror(cd_stage->argv[1]);
		}
	    }
	    /* if we are in interactive mode */
	    if(one_arg)
	    {
		printf("%s", PROMPT);
	    }
	    /* get next line */
	    input_command = readLongString(input_file);
	}

	/* not cd */
	else
	{
	    /* get number of stages/pipes */
	    int num_stages = pipe_line->length;

	    /* create all the pipes for n-1 stages(children) */
	    int pipes[num_stages -1][2];
	    for(i = 0; i < (num_stages - 1); i++)
	    {
	        if(pipe(pipes[i]) == -1)
	        {
		    perror("Error creating pipes.\n");
		    exit(1);
	        }
	    }

	    /* block sig int */
	    block_sigInt();
            /* declare pid */
	    pid_t pid;

	    /* loop through all the stages we have */
	    for(i = 0; i < num_stages; i++)
	    {
	        /* get the stage that we are on */
	        clstage stage = &pipe_line->stage[i];

	        /* fork, check if successful! */
	        pid = fork();
	        /* check if we can fork successfully */
	        if(pid == -1)
	        {
		    perror("Error forking a command");
		    exit(1);
	        }
	        /* if fork is a child, exec */
	        else if(pid == 0)
	        {
		    /* unblock our sigInt */
		    unblock_sigInt();
		    /* if we are not at first stage */
		    if(i > 0)
		    {
		        /* if an input file is not provided */
		        if(stage->inname == NULL)
		        {
			    /* redirect input from previous pipe */
		  	    if(dup2(pipes[i - 1][READ], STDIN_FILENO) == -1)
			    {
			        perror("Could not dupe child to STDIN\n");
			        exit(1);
			    }
		        }
		        /* if input file is provided */
		        else
		        {
			    /* open the input file */
			    inFD = open(stage->inname, O_RDONLY);
			    if(inFD == -1)
			    {
			        perror("Error opening input file\n");
			        exit(1);
			    }
			    /* redirect input from previous pipe */
			    if(dup2(inFD, STDIN_FILENO) == -1)
			    {
			        perror("Error duiping inFD to STDIN\n");
			        exit(1);
			    }
			    /* close the input file */
			    close(inFD);
		        }
		    }
		    /* if it is the first stage */
		    else if(i == 0)
		    {
		        /* if there is an input file provided */
		        if(stage->inname != NULL)
		        {
			    /* open the input file */
			    inFD = open(stage->inname, O_RDONLY);
			    if(inFD == -1)
			    {
			        perror("Error opening input file\n");
			        exit(1);
			    }
			    /* redirect the input from previous pipe */
			    if(dup2(inFD, STDIN_FILENO) == -1)
			    {
			        perror("Error duping inFD to STDIN\n");
			        exit(1);
			    }
			    /* close the input file */
			    close(inFD);
		        }
		    }		
		    /*if we are not at last stage */
		    if(i < (num_stages - 1))
		    { 
		        /* if there is no output file provided */
		        if(stage->outname == NULL)
		        {
			    /* redirect output for current pipe */				
		            if(dup2(pipes[i][WRITE], STDOUT_FILENO) == -1)
			    {
			        perror("Could not dupe child to STDOUT\n");
			        exit(1);
			    }
		        }
		        /* if output file is provided */
		        else
		        {
			    /* open the output file w/ correct perms */
			    outFD = open(stage->outname, O_WRONLY | O_CREAT 
			    | O_TRUNC, S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR 
			    | S_IWGRP | S_IWOTH);
			    if(outFD == -1)
			    {
			        perror("Error opening output file\n");
			    }
			    /* redirect the output to the file */
			    if(dup2(outFD, STDOUT_FILENO) == -1)
			    {
			        perror("Error duping outFD to STDOUT\n");
			        exit(1);
			    }
			    /* close the file */
			    close(outFD);
		        }
		    }
		    /* last stage */
		    else if(i == (num_stages - 1))
		    {
		        /* if output file is provided */
		        if(stage->outname != NULL)
		        {
			    /* open the output file w/ correct perms */
			    outFD = open(stage->outname, O_WRONLY | O_CREAT 
			    | O_TRUNC, S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR
			    | S_IWGRP | S_IWOTH);
			    if(outFD == -1)
		 	    {
			        perror("Error opening output file\n");
			    }
			    /* redirect the output to the output file */
			    if(dup2(outFD, STDOUT_FILENO) == -1)
			    {
			        perror("Error duping outFD to STDOUT\n");
			        exit(1);
			    }	
			    /* close the output file */
			    close(outFD);
		        }
		    }
					
		    for(j = 0; j < num_stages - 1; j++)
		    {
			/* close all the pipes */
		        close(pipes[j][0]);
                        close(pipes[j][1]);
		    }

		    /* exec our command */
		    execvp(stage->argv[0], stage->argv);
		    perror(stage->argv[0]);
		    exit(1);
		}    

		/* parent! close all those pipes! */
		else
		{
		    if(i > 0)
		    {
		  	close(pipes[i - 1][READ]);
			close(pipes[i - 1][WRITE]);
		    }
		    if(i < (num_stages - 1))
		    {
			close(pipes[i][WRITE]);
		    }
		}
	    }

	    /* wait for all the children :) */
	    for(i = 0; i < num_stages; i++)
	    {
	        if(wait(NULL) == -1)
	        {
		    perror("Waiting for children\n");
		    exit(1);
	        }
  	    }   

	    /* free our vars */	    
	    free_pipeline(pipe_line);
	    free(input_command);

  	    /* print out PROMPT if interactive mode */
	    if(one_arg)
	    {
	        printf("%s", PROMPT);
	    }

	    /* read the next line */
	    input_command = readLongString(input_file);
	}
    }
	/* go to new line after process is terminated in interaective mode */
        if(one_arg)
        {
	    printf("\n");
        }

    free(input_command);
    return 0;
}

void block_sigInt()
{
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    sigprocmask(SIG_BLOCK, &set, NULL);
}

void unblock_sigInt()
{
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    sigprocmask(SIG_UNBLOCK, &set, NULL);
}
