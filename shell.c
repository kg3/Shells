/*
 * Simple Shell that forks commands to $PATH
 * Description: shell with exec & fork implementations; catching CRTL-Z&C
 *				will exit on quit or exit. Uses execvp; PATH is set by user env
 *				argv passed as an array.  Clamping down on unnecessary input to
 *				break anticipated buffer overflows.  Since strsep & strndup come from
 *				BSD & GNU trees of GCC; declaring source in the top of the code takes
 *				errors out of -pedantic & -ansi such as implicit declerations and
 *				integer to char casting.
 */
#define _BSD_SOURCE				/* w/o this impicit declerations happen with strsep */
#define _GNU_SOURCE				/* same w/ strsep but for strndup */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <assert.h>

#define DEBUG 0					/* current coding: 0 False; 1 True */
/* restricting obscure/bad/long input. forseeing user input as a potential problem. */
#define MAX_INPUT 120			/* maximum amount of characters in a line of input */
#define MAX_CMND 10				/* maximum amount of commands */
#define CMND_LEN 32				/* maximum length of command */
#define DELIM " \r\n"			/* delimiters for tokenizing from strsep */

int input(char *commands[MAX_CMND]);
int SpecialCommands(char *commands[MAX_CMND]);
void handle_signal(int sig);
void freeme(char *freeThisArray[MAX_CMND]);
void help();

int main( int argc, char *argv[] ) 
{
	int i=0,count=0, status, pflag = 1, exec_ret = 0;
	char *commands[MAX_CMND];
	
	/* --BeginLoop-- */ 
	while(pflag)	
	{
		/* Catch signals Ctrl-C, Ctrl-Z */
		signal(SIGINT,handle_signal);
	    signal(SIGTSTP,handle_signal);
		/* re-initialize or start grabbing memory space for input */
		
		
		for(i = 0; i < MAX_CMND; i++)
		{	/* restricting space that user can input, keep BO's out perhaps */
			commands[i] = (char*)malloc(CMND_LEN * sizeof(char));
			memset(commands[i],0, CMND_LEN);	/* set all memory to /0 */
		}
    	
    	/* ShElL PrOmpT  BeGiN */
    	/* count will help to skip over forking if user doesn't input anything */
    	count = input(commands);	/* start prompt; catch input; tokenize into array */
    	/* ShElL PrOmPt EnD */
    
        if( commands[0] == '\0')
        { 
        	continue;
        }
        
    	/* PrOcEsS CoMmAnDs BeGiN */
		/* One, just one, only one fork(); only on the condition there is input. */
		/* If SpecialCommands is 0, then there is input that's being processed by it */
		/* that's why we wouldn't want to fork with: "" or "quit","exit","help" etc. */
		if ( (count > 0) && (SpecialCommands(commands) != 0) )
		{
			pid_t pid = fork();

	  		/* take over child with execvp */
	    	if(pid == 0)  /* CHILD */
			{	/* execvp uses environment PATH set by shell; /bin/,/usr/bin, etc. */
			    printf("%s\n", commands[0]);
			    exec_ret=execvp(commands[0],commands);

				/* watching when an error happens, PATH can't find command, kill child*/
				if(exec_ret == -1)
    			{
    				printf("\n%s: Command not found.\n",commands[0]);
    				exit(0); /* Kill the child */
    			}
		    }
		    
		    wait( &status ); /* wait on child/everyone before proceeding */
	    	if (pid !=0)	/* PARENT */
	    	{
	    		fflush( NULL );  /* keep msh shell from bogging down with crap input */
	    	}
		} /* -- count, is blank, special may have been used, don't fork, continue */
		/* pRoCeSs cOmMaNdS eNd */
		
		/* Clean up all the memory space */
		freeme(commands);
	}	
	/* --EndLoop-- */
    exit(0);	/* If I'm here I want to exit */
}

/**
 * Input function; collect input from user; prompt: msh>; tokenize
 * Just in case return the amount of commands entered if needed in future
 * /returns: (int) amount of commands & adds to (char) array; commands
 * /params: char array pointer to 2D char array (preallocated)
 * **/
int input(char *commands[MAX_CMND])
{
    char *buffer, *input = (char*) malloc( MAX_INPUT * sizeof(char) );
    int i = 0, count=0;

    /* shell prompt */
	printf("\nmsh> ");
	
	/**
	 * forseeing scrubbing BO input, watching for NULL when user enters more than MAX_INPUT
	 * If user does, program automagically exits. If user inputs more commands than MAX_CMND,
	 * program exits automagically but the user needs to be notified as to why.
	 * Any input for commands beyond CMND_LEN is neglected and program continues.
	 **/
	if( fgets(input,MAX_INPUT,stdin) != NULL)
	{
		/* make temporary space to tokenize commands with for copying out of input */
		buffer = (char*) malloc( strlen(input) * sizeof(char) );
		i=0;
		while( (buffer = strsep(&input, DELIM) ) != NULL)
		{	/**
			 * with strncmp... trying to prevent exit when over MAX_CMND "spaces" 
			 * entered but doesn't help.
			 **/
			if( (strlen(buffer) >= 1) && (strcmp(buffer," ")!=0) )
			{	/* everything that is worthwhile to copy is in here or else is null */
				commands[i] = strndup(buffer,CMND_LEN);	/* Restrict length of commands */
				count++;
				if( strlen( commands[i] ) == 0)
				{
					commands[i]= NULL;
				}
			}
			else
			{	/* without this no NULLs are in commands array and execvp will return error */
				commands[i] = NULL;
			}
			i++;
			
			if(i > MAX_CMND)
			{	/**
				 * More than MAX_CMND of delimiters have been entered and will 
				 * overwrite the stack; overwriting pflag from main() which will
				 * exit the loop and kill the program.  For protection we need to 
				 * prevent that in a number of different ways. This way works a bit.
				 **/
				printf("More than %d, delimiters entered which are potential commands",MAX_CMND); 
				input=NULL;
			}
		}
	
		/* clear allocated buffer(s) to keep from overflows and junk gathering */
		free(buffer);
		free(input);
	}
	else
	{
		printf("\nSignals caught OR input error: %d max characters\n",MAX_INPUT);
	}
	
	return count;
}

/**
 * Handling all required commands like exit,help,quit etc. that are required outside
 * of PATH. Sending in commands array to responsibly free if exiting and for reading. 
 * Returning a 1 will continue in the loop and a 0 will stop the fork exec part 
 * and continue in loop.
 * /returns: a zero or a 1
 * /params: array pointers to clean memory
 **/
int SpecialCommands(char *commands[MAX_CMND])
{
	if( commands[0] && (strcmp(commands[0],"exit")==0) | (strcmp(commands[0],"quit")==0) )
	{	
	    printf("\nAdios!\n");
		/* free memory before exit, being responsible with memory */
		freeme(commands);
	    exit(0);	/* exiting the shell */
	}

	else if( commands[0] && strcmp(commands[0],"help")==0 )
	{
		help();
		/** returning a 0, will make SpecialCommands, zero, which will skip over fork
		 * and goto end of loop. That's what we want since this is suppose to act like 
		 * a real command and end after execution 
		 **/
		return 0;
	}
	
	/* if no special commands executed return with a 1 to continue to an exec fork */
	return 1;
}

/**
 * Free the 2D command array, reused a lot so here for convienience.
 * /returns: NOTHING
 * /params: 2D array
 **/
void freeme(char *freeThisArray[MAX_CMND])
{
	int i;
	for(i=0;i<MAX_CMND;i++)
	{
		free(freeThisArray[i]);
	}	/* no; free(freeThisArray), segfaults */
}

/**
 * Catch the signals handler, does essentially nothing, needed for signal()
 * /returns: NOTHING
 * /params: signal number caught (handled automagically by signal)
 **/
void handle_signal(int sig)
{
	if(DEBUG)
		printf("Caught signal %d\n",sig);
}

/**
 * Help print function; Prints the required help menu
 * /returns: NOTHING
 * /params: NONE
 **/
void help()
{
    printf("\n - This is the Help Menu - \n");
}
