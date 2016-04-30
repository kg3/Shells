/*
 * Searches through a memory mapped file using threads.
 * Description: Searching through a memory mapped file and then using memcmp
 *				to compare strings (using addresses) instead of strngcmp. Joining
 *				Threads to keep track of bytes used to count successful compares. 
 *				or replacements.
 */
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <sys/time.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>
#include <ctype.h>

#define DEBUG 0					/* current coding: 0 False; 1 True */
#define MAX_INPUT 120			/* maximum amount of characters in a line of input */
#define MAX_CMND 4				/* maximum amount of commands */
#define CMND_LEN 32				/* maximum length of command */
#define DELIM " \r\n"			/* delimiters for tokenizing from strsep */
#define RDWRT "shakespeare.txt"	/* read and write from this file */
#define RESET "shakespeare_backup.txt"	/* restore from this file */
/* man pthread_create: error handling */
#define handle_error_en(en,msg) \
		do { errno = en; perror(msg); exit(EXIT_FAILURE); } while(0)

#define handle_error(msg) \
		do { perror(msg); exit(EXIT_FAILURE); } while(0)

/**
 * Structure of the each individual thread. 
 * Holds beginning & end of segment, count & option
 * Each thread is own struct, no mutex needed.
 **/
struct thread_info {	/* used an argument to thread_start() */
	pthread_t thread_id;	/* ID returned by pthread_create() */
	int	thread_num;			/* application-defined thread # */
	/* copys over string */
	char *sstring;
	char *rstring;			
	char *data_ptr;			/* only holds an address */
	
	int begin;
	int end;
	int option;
};

int count = 0;
pthread_mutex_t m_lock;

int input(char *commands[MAX_CMND],int argc, char *argv[]);
int SpecialCommands(char *commands[MAX_CMND]);
int returnTheInteger(char *strToInt);
void freeme(char *freeThisArray[MAX_CMND]);
void help();
void welcome();
void self_thread( char *commands[MAX_CMND], char *data, int size);
static void *search(void *arg);

int main( int argc, char *argv[] ) 
{
	int i=0, status, pflag = 1,fd,ret;
	struct stat sbuf;
	char *commands[MAX_CMND], *data;
	
	/* begin memmap file */
	/* load file in beginning of program into memory */
	if ((fd = open(RDWRT, O_RDWR)) == -1) 
	{
		perror("Cannot open RDWRT\n");
		exit(EXIT_FAILURE);
	}
    if (stat(RDWRT, &sbuf) == -1) 
	{
	    perror("Status of RDWRT changed\n");
	    exit(EXIT_FAILURE);
	}
	/* open the file into the map*/
	data = mmap(NULL, sbuf.st_size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	
	if (data == MAP_FAILED) 
	{
	    perror("error: mmap(NULL, sbuf.st_size, PROT_READ, MAP_SHARED, fd, 0) \n");
	    exit(EXIT_FAILURE);
	}
	/* end memmap file */
	
	/* WeLcOmE MeSsAgE */
	welcome();
	
	/* --BeginLoop-- */ 
	while(pflag)	
	{
	
		/* re-initialize or start grabbing memory space for input */
		for(i = 0; i < MAX_CMND; i++)
		{	/* restricting space that user can input, keep BO's out perhaps */
			commands[i] = (char*)malloc(CMND_LEN * sizeof(char));
			memset(commands[i],0, CMND_LEN);	/* set all memory to /0 */
		}
    	
    	
 		/* ShElL PrOmpT  BeGiN */
    	/* count will help to skip over forking if user doesn't input anything */
    	//inp_ret = input(commands);	/* start prompt; catch input; tokenize into array */
    	pflag = input(commands,argc,argv);
    	/* ShElL PrOmPt EnD */
    	
		/* If SpecialCommands is 0, then the right command(s) were entered; and checked over */
		if ( (SpecialCommands(commands) == 0) )
		{
			if( !(strcmp(commands[0],"reset")) )
			{	/* unmap RDWRT first */
				if (munmap(data, sbuf.st_size) == -1) 
				{	
					perror("Error un-mmapping the file: RDWRT\n");
			    }	
				close(fd);
				/* cp shakespeare_backup to shakespeare */
				ret = 0;
				if(!fork())
				{
					ret = execlp("cp","cp",RESET,RDWRT,NULL);
					exit(0);
				}
				else
				{
					wait(&status);
					/* reload mmapped file */
					if ((fd = open(RDWRT, O_RDWR)) == -1) 
					{
						perror("Cannot open RDWRT\n");
						exit(EXIT_FAILURE);
					}
				    if (stat(RDWRT, &sbuf) == -1) 
					{
					    perror("Status of RDWRT changed\n");
					    exit(EXIT_FAILURE);
					}
					data = mmap(NULL, sbuf.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
					
					if (data == MAP_FAILED) 
					{
					    perror("error: mmap(NULL, sbuf.st_size, PROT_READ, MAP_SHARED, fd, 0) \n");
					    exit(EXIT_FAILURE);
					}
					
					if(ret == -1)
					{
						perror("resetting RDWRT has failed");
						exit(0);
					}
					else
					{
						printf("Successfully reset %s with %s\n",RDWRT,RESET);
					}
				}
			}
			else
			{
				self_thread(commands,data,(int)sbuf.st_size);
			}
		} /* -- end of main loop */
		
		/* Clean up all the memory space */
		freeme(commands);

	}/* --EndLoop-- */
	
	/* clean up mmap */
	if (munmap(data, sbuf.st_size) == -1) 
	{
		perror("Error un-mmapping the file: RDWRT\n");
    }
	close(fd);
	/* If I'm here I want to exit */
    return 0 ;		
}

/**
 * Search for each thread; iterate over each selected portion, memcmp or memcpy
 * /params address to struct thread_info
 * /returns nothing
 **/
static void *search(void *arg)
{
	struct thread_info *tinfo = arg;
	int i;
	/* point to big data */
	char *data_ptr = tinfo->data_ptr;
	if(DEBUG)
	{
		printf("Thread %2d: \tsearch:[%s] replace:[%s] ",
				tinfo->thread_num,tinfo->sstring,tinfo->rstring);
		printf(" begin:[%d]\tend:[%d] option:%d\n",
				tinfo->begin,tinfo->end,tinfo->option);
	}
	
	for(i = tinfo->begin; i <= tinfo->end; i++)		
	{
		if(!memcmp(data_ptr+i,tinfo->sstring,strlen(tinfo->sstring)))
		{
			pthread_mutex_lock(&m_lock);
			count++;
			pthread_mutex_unlock(&m_lock);
			if(tinfo->option == 2) /* replace */
			{
				/* tinfo->rstring */
				memcpy(data_ptr+i,tinfo->rstring,strlen(tinfo->rstring));
			}
		}
	}
	return 0;
}

/**
 * multi threading handled from here with seperate options. Either replace
 * or search will do the same thing //replace-replaces. Pthread creates here
 * calls Search, wich does the work.
 * /params: 2D commands array, mmapped data; (int)sbuf.st_size
 * /returns: Nothing
 **/
void self_thread(char *commands[MAX_CMND], char *data, int size)
{
	struct timeval start, end;
	int i=0,workers,w,portionBegin,portionEnd,option;
	float ptime;
	char *copy;
	/* pthread stuff */
	int s;
	void *returnFromJoin;
	struct thread_info *tinfo;
	
	if(strcmp(commands[0],"search") == 0)	
	{
		option = 1;
		/* Verify commands[2] can be converted to int, 1 - 100*/
		workers = returnTheInteger(commands[2]);
	} 
	
	else if( strcmp(commands[0],"replace")==0 )	
	{
		option = 2;
		workers = returnTheInteger(commands[3]);
		/* copying length of "search" to "replace" */
		copy = (char*)malloc(strlen(commands[1]) * sizeof(char)+1);
		memset(copy,0,strlen(commands[1]));
		strncpy(copy,commands[2],strlen(commands[2]));
		copy[strlen(commands[2])]='\0';
		if(strlen(commands[1]) > strlen(commands[2]))
		{
			for( i=(strlen(commands[2]) - 1); i < (strlen(commands[1]) - 1); i++)
			{
				strcat(copy," ");
			}
			/* always end your strings with a NULL */
			copy[strlen(commands[1])]='\0';
		}

	} 
	
	/* GetTimeOfDay */
	gettimeofday(&start, NULL);
	if(workers != -1)	/* save execution if workers is bad input */
	{
		/* !!! INSERT MULTITHREADING !!! */
		count = 0;
		portionBegin = 0;
		
		tinfo = calloc(workers, sizeof(struct thread_info));
		if(tinfo == NULL) handle_error("calloc");
		
		/* create one thread for every worker */
		for(w=0; w < workers; w++)
		{
			/* divide up the work; W-> is w+1 */
			portionEnd = ( (w+1) / ((float)workers) ) * size;
			
			tinfo[w].thread_num = w + 1;
			tinfo[w].data_ptr = data;
			tinfo[w].begin = portionBegin;
			tinfo[w].end = portionEnd;
			tinfo[w].option = option;
			tinfo[w].sstring = (char*)malloc(strlen(commands[1])*sizeof(char)+1);
			strncpy(tinfo[w].sstring,commands[1],strlen(commands[1]));
			tinfo[w].sstring[ strlen(commands[1]) ] = '\0';
			if(option == 2)
			{
				tinfo[w].rstring = (char*)malloc(strlen(copy)*sizeof(char));
				strncpy(tinfo[w].rstring,copy,strlen(copy));
			}
			s = pthread_create(&tinfo[w].thread_id,NULL,&search,&tinfo[w]);
			
			if( s!=0) handle_error_en(s,"pthread_create");
			
			portionBegin = (( w + 1)/ ((float)workers) ) * size;
		}
		
		/* Join all the threads & add up count */
		for(w = 0; w < workers; w++)
		{   
			s = pthread_join(tinfo[w].thread_id,&returnFromJoin);
			if(s !=0) handle_error_en(s, "pthread_join");
			free(returnFromJoin);
		}
		
  		/* END MMAP; PARTITION-FORK; PIPE; MEMCMP */

		gettimeofday(&end, NULL);
		ptime = (end.tv_sec * 1000000 + end.tv_usec) - 
					(start.tv_sec * 1000000 + start.tv_usec);
					
		if(option == 1)
		{
			printf("Found %d instances of %s in %.4f microseconds\n",
				count,commands[1],ptime);
		}
		else if(option == 2)
		{
			printf("Replaced %d instances of %s with %s in %.4f microseconds\n",
				count,commands[1],commands[2],ptime);
		}
		free(tinfo);
	}
	//free(tinfo);
	if(option==2) free(copy);
	
	fflush( NULL );
}

/**
 * String to Integer. Return -1 on failure. Uses example from "man 3 strtol"
 * /returns: (Integer 1-100) OR -1 on error
 * /params: string
 **/
int returnTheInteger(char *strToInt)
{
	long value;
	char *endptr;
	errno=0;
	value = strtol(strToInt,&endptr,10); /* base 10 */
	if( value > 100 || value < 1 )
	{   /* out of range */
		printf("\n%s: is out of range 1-100\n",strToInt);
		return -1;
	}
	if(endptr == strToInt)
	{   /* no digits to parse */
		printf("\n%s: contains no integers\n",strToInt);
		return -1;
	}
	/* successfully found a number */
	return (int)value;
}

/**
 * Input function; collect input from user; prompt: msh>; tokenize
 * Just in case return the amount of commands entered if needed in future
 * /returns: (int) amount of commands & adds to (char) array; commands
 * /params: char array pointer to 2D char array (preallocated)
 * **/
int input(char *commands[MAX_CMND], int argc, char *argv[])
{
    char *buffer, *input = (char*) malloc( MAX_INPUT * sizeof(char) );
    int i = 0, countD=-1, opt;

    /* shell prompt */
	printf("> ");
	
	/* Create Debugging options */
	if( argc >= 2)
	{
		while(( opt = getopt(argc, argv, "s:r:")) != -1)
		{
			switch(opt)
			{	
				case 's':		/* debug search function */
					strncpy(commands[0],"search",strlen("search"));
					strncpy(commands[1],"the",strlen("the"));
					strncpy(commands[2],optarg,strlen(optarg));
					countD = 0;	/* sets the pflag for while loop to exit */
					break;
				case 'r':		/* debug replace function */
					strncpy(commands[0],"replace",strlen("replace"));
					strncpy(commands[1],"Desdemona",strlen("Desdemona"));
					strncpy(commands[2],"repl4ce",strlen("repl4ce"));
					strncpy(commands[3],optarg,strlen(optarg));
					countD = 0; /* sets the pflag for while loop to exit */
					break;
				default:
				   	if(DEBUG)
				   	{
				   		printf("!DEBUG ON!");
				   		printf("\nUsage: %s [-s #] [-r #]\n",argv[0]);
				   		//exit(EXIT_FAILURE);
				   		//countD = -1;
				   	}
					break;
			}
		}
	}
	
	if (countD == -1)
	{
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
					/* always add a NULL byte */
					commands[i+1]='\0';
					countD++;
					if( strlen( commands[i] ) == 0)
					{
						commands[i]= NULL;
					}
					/* placing iterator here, ignores lots of potential problems with input */
					i++;
				}
				else
				{	/* without this no NULLs are in commands array and execvp will return error */
					commands[i] = NULL;
				}
	
				if(i > MAX_CMND)
				{	/**
					 * More than MAX_CMND of delimiters have been entered and will 
					 * overwrite the stack; overwriting pflag from main() which will
					 * exit the loop and kill the program.  For protection we need to 
					 * prevent that in a number of different ways. This way works a bit.
					 **/
					printf("More than %d, delimiters entered which are potential commands\n",MAX_CMND); 
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
		countD=1;	/* setting the count if the input==pflag; keep loop going */
	}
	/* if input==pflag protect the loop from exiting */
	
	return countD;
}

/**
 * Handling all required commands like search,help,quit etc. 
 * Sending in commands array to responsibly free if exiting and for reading. 
 * Returning a 1 will break from the loop and start over. (almost like no commands entered)
 * Returning a zero, will enter into rest of the while loop. 
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
		return 1;
	}
	
	else if(commands[0] && strcmp(commands[0],"search")==0 )
	{	/* need to verify input before sending into process */

		if(commands[1] && commands[2]) /* verify correct arguments for search */
		{	/* all arguments accounted for */
			return 0;
		}
		
		else
		{	/* missing arguments */
			printf("missing input: search [word] [workers: 1-100]\n");
			return 1;
		}
	}
	
	else if(commands[0] && strcmp(commands[0],"replace")==0 )
	{	/* need to verify input before sending into process */

		if(commands[1] && commands[2] && commands[3]) /* verify correct arguments for replace */
		{	/* all arguments accounted for */
			return 0;
		}
		
		else
		{	/* missing arguments */
			printf("missing input: replace [word 1] [word 2] [workers: 1-100]\n");
			return 1;
		}
	}
	
	else if( commands[0] && strcmp(commands[0],"reset")==0 )
	{
		return 0;
	}
	
	else if( !(commands[0]) ) 
	{
		// print a line of nothing on empty input
	}
	else 
	{
		printf("Command not found. Try 'help'\n");
	}
	/* if no special commands executed return with a 1 skip over the loop */
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
 * Help print function; Prints the required help menu
 * /returns: NOTHING
 * /params: NONE
 **/
void help()
{
	
    printf("\nShakespeare Word Search Service Command Help\n");
    printf("----------------------------------------------\n");
    printf("help\t\t- displays this message\n");
    printf("quit\t\t- exits\n");
    printf("search [word] [workers] - searches the works of Shakespseare for [word] using\n");
    printf("                          [workers]. [workers] can be from 1 to 100.\n");
    printf("replace [word 1] [word 2] [workers] - search the works for Shakespeare for\n");
    printf("                          [word 1] using [workers] and replaces each\n");
    printf("                          instance with [word 2].  [workers] can be from\n");
    printf("                          1 to 100.\n");
    printf("reset   - will reset the memory mapped file back to its original state.\n");
}

/**
 * Display the welcome message
 * /returns:NOTHING
 * /params: NONE
 **/
void welcome()
{	// start with a fresh line for input
	printf("\nWelcome to the Shakespseare word count service.\n");
	printf("Enter: Search [word] [workers] to start your search.\n"); 
}
