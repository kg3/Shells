/*
 * Name: Kurt Gibbons
 * Description: File system that has 5MB resident in memory. Indexed allocated
 *		like NTFS. File system is not persistent. Single directory.  Much like
 *        	CP/M with a longer filename
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

#define DEBUG 0				/* current coding: 0 False; 1 True */
#define MAX_INPUT 1024			/* maximum amount of chars in a line of input */
#define MAX_CMND 3			/* maximum amount of commands */
#define DELIM " \r\n"			/* delimiters for tokenizing from strsep */

/* for file system */
#define CMND_LEN 255			/* Needs to be 255 for file_system */
#define BLOCK_SIZE 4096			/* block size */
#define NUM_BLOCKS 1280			/* number of blocks: 5242880 / 4096 = 1280 */
#define DISK_SPACE 5242880		/* maximum disk space */
#define	MAX_SIZE 131072			/* maximum file size */
#define MAX_FILES 128			/* maximum files supported */
#define MAX_BLOCKS 32			/* maximum amount of blocks per file: 131072 / 4096 = 32  */
#define META 3				/* size of meta arrays */

int input(char *commands[MAX_CMND],int argc, char *argv[]);
int SpecialCommands(char *commands[MAX_CMND]);
void freeme(char *freeThisArray[MAX_CMND]);
void help();
void welcome();


/* Globally connected arrays; 5MB memory block & meta */
unsigned char file_data[NUM_BLOCKS][BLOCK_SIZE];
/* 0 is free; 1 is used */
int bits[NUM_BLOCKS];

typedef struct meta{
	char *meta[META];
	// [0] = File Name
	// [1] = date added
	int totals_and_flags[META];
	// [0] = file size
	// [1] = number of blocks used
	int pointers[MAX_BLOCKS];
	// [] indices used from file_data
} meta;

meta metaArray[MAX_FILES];
int put(char *commands[MAX_CMND]);
int get(char *commands[MAX_CMND]);
int list(char *commands[MAX_CMND]);
int del(char *commands[MAX_CMND]);
int freespace();


int main( int argc, char *argv[] ) 
{
    int i=0, pflag = 1;
	char *commands[MAX_CMND];
	
	for(i=0; i<NUM_BLOCKS; i++) 
	{	
		bits[i] = 0;
	}
	
    	/* WeLcOmE MeSsAgE */
	welcome();
	
	/* --BeginInfiniteLoop-- */ 
	while(pflag)	
	{
		/* re-initialize or start grabbing memory space for input */
		for(i = 0; i < MAX_CMND; i++)
		{	/* restricting space that user can input, keep BO's out perhaps */
			commands[i] = (char*)malloc(CMND_LEN * sizeof(char));
			memset(commands[i],0, CMND_LEN);	/* set all memory to /0 */
		}
    	
 		/* ShElL PrOmpT  BeGiN */
    	/* pflage will help to break wile loop if using debug statements */
    	//inp_ret = input(commands);	/* start prompt; catch input; tokenize into array */
    	pflag = input(commands,argc,argv);
    	/* ShElL PrOmPt EnD */
	    
	    if ( (SpecialCommands(commands) == 0) )
		{
		    if( commands[0] && strcmp(commands[0],"put")==0 )
			{
				//printf("put %s\n", commands[1]);
				if( put(commands) == 1)
				{
					printf("put <file>: Error\n");
				}
			}
			else if( commands[0] && strcmp(commands[0],"get")==0 )
			{
				if( get(commands) == 1)
				{
					printf("get <file>: Error\n");
				}
			}
			else if( commands[0] && strcmp(commands[0],"del")==0 )
			{
				if( del(commands) == 1)
				{
					printf("del <file>: Error\n");
				}
			}
			else if( commands[0] && strcmp(commands[0],"list")==0 )
			{
				if( list(commands) == 1)
				{
					printf("list: No files found.\n");
				}
			}
			else if( commands[0] && strcmp(commands[0],"df")==0 )
			{
				printf("%d bytes free.\n",freespace());
			}
		}
		
		/* Clean up all the memory space */
		freeme(commands);

	}/* --EndInfiniteLoop-- */
	
    /* If I'm here I want to exit */
    return 0 ;	    
}

/**
 * place the file into the memory space, returns just error flags
 * 
 * /params commands array
 * /returns 0-successful; 1-failure;
 **/
int put(char *commands[MAX_CMND])
{
	time_t t = time(NULL);
	struct stat buf;
	char tme[80];
	int status, copy_size, offset, block_index,i, bytes,b;
	int caught=0,meta_index;
	
	/* find a meta data spot */
	for(i=0; i < MAX_FILES; i++)
	{
		if( metaArray[i].meta[0] )
		{
			if( strcmp(metaArray[i].meta[0], commands[1]) == 0)
			{
				printf("Error: Same file exists.\n");
				return 1;
			}
		}
	}
	
	FILE *ifp = fopen( commands[1], "r");
	
	if(ifp == NULL)
	{
		perror("put: opening file error\n");
		return 1;
	}
	
	status = stat(commands[1],&buf);

    	if (status == -1) 
	{
	    perror("Status of file changed\n");
	    exit(EXIT_FAILURE);
	}
	
	if(buf.st_size > MAX_SIZE)
	{
		printf("put error: File is greater than maximum size allowed\n");
		return 1;
	}
	
	if( buf.st_size > freespace() )
	{
		printf("put error: Not enough disk space.\n");
		return 1;
	}
	
	/* find a block index */
	for(i=0; i<NUM_BLOCKS; i++)
	{
		if( bits[i] == 0 )
		{
			/* find first bit */
			block_index = i;
			/* change the bit */
			bits[i] = 1;
			/* only need to find first position */
			break;
		}
	}
	
	for(i=0; i < MAX_FILES; i++)
	{
		if( !(metaArray[i].meta[0]) )
		{ 	/* found a spot for our file's data */
			//metaArray[i] = malloc( sizeof(struct meta) );
			caught = 0;
			meta_index = i;
			break;
		}
		else
		{	/* changes back to one if never found a NULL */
			/* wait till end of loop to see if one block is empy */
			caught = 1;
		}
	}
	
	if(caught == 1)
	{	/* no room in array, exit function */
		printf("Maximum amount of files reached!\n");
		return 1;
	}
	
	/* alloting memory to structure being used for the put file */
	for(i=0; i < META; i++)
	{
		metaArray[meta_index].meta[i] = (char*)malloc(CMND_LEN * sizeof(char));
		memset( metaArray[meta_index].meta[i],0, CMND_LEN);
	}
	
	/* adding meta data; name */
	strncpy( metaArray[meta_index].meta[0], commands[1], strlen(commands[1]) );
	metaArray[meta_index].meta[0][strlen(commands[1])] = '\0';
	
	/* adding meta data; file size */
	metaArray[meta_index].totals_and_flags[0] = buf.st_size;
	
	/* place catchers into pointers array to prevent overwriting other file_data on del */
	for(i=0; i < MAX_BLOCKS; i++)
	{
		metaArray[meta_index].pointers[i] = -1;
	}
	
	copy_size = buf.st_size;
	offset = 0;
	i = 0; 
	
	while(copy_size > 0)
	{
		/* save the bit; saving pointers used on file_data array */
		metaArray[meta_index].pointers[i] = block_index;
		/* change the bit */
		bits[block_index] = 1;
		
		fseek(ifp,offset,SEEK_SET);
		bytes = fread( file_data[block_index], BLOCK_SIZE, 1, ifp );
		
		if( bytes == 0 && !feof(ifp) )
		{
			printf("\nError reading from file\n");
			return 1;
		}
		
		clearerr(ifp);
		
		copy_size -= BLOCK_SIZE;
		offset += BLOCK_SIZE;
		
		/* find the bit  - to use for block_index */
		for(b=block_index; b<NUM_BLOCKS; b++)
		{
			if( bits[b] == 0 )
			{
				/* find one next bit */
				block_index = b;
				/* wait to change the bit */
				break;
			}
		}
		
		/* saving amout of blocks used; even though this is not used right now */
		metaArray[meta_index].totals_and_flags[1] += i+1;
		i++;
	}
	
	fclose(ifp);
	
	/* insert time; structures time into sting; adds string to struct */
	struct tm *tm = localtime(&t);
	
	strncpy(tme,asctime(tm),80);
	
	for(i=0; i < strlen(tme); i++)
	{
		if( strcmp(&tme[i],"\n") == 0)
		{ 	/* remember to NULL */
			tme[i] = '\0';
		}
	}
	
	strncpy( metaArray[meta_index].meta[1], tme,strlen(tme) );
	/* remember to NULL */
	metaArray[meta_index].meta[1][strlen(metaArray[meta_index].meta[1])] = '\0';
	
	/* everything is good */
	return 0;
}

/**
 * gets the file and puts it to the memory space, returns just error flags
 * 
 * /params commands array
 * /returns 0-successful; 1-failure;
 **/
int get(char *commands[MAX_CMND])
{
	int block_index,copy_size,offset,meta_index,i,caught=0,num_bytes;
	FILE *ofp;
	
	/* search metaArray for file to copy from */
	for(i=0; i < MAX_FILES; i++)
	{
		if( metaArray[i].meta[0] )
		{
			if( strcmp( metaArray[i].meta[0],commands[1] ) == 0 )
			{
				meta_index=i;
				caught=1;
			}
		}
	}
	if(caught==0)
	{
		printf("get error: File not found.\n");
		return 1;
	}
	
	/* open output file; maybe commands[2] the new file name */
	if( commands[1] && !(commands[2]) )
	{
		ofp = fopen( commands[1], "w");
	
		if( ofp == NULL )
		{
			printf("get error: Could not open output file.\n");
			return 1;
		}
	}
	/* if user puts in their own file name */
	else if(commands[1] && commands[2] )
	{
		ofp = fopen( commands[2], "w");
	
		if( ofp == NULL )
		{
			printf("get error: Could not open output file.\n");
			return 1;
		}
	}
	
	/* Pull data out of metaArray */
	copy_size = metaArray[meta_index].totals_and_flags[0];
	offset = 0;
	i = 0;
	block_index = metaArray[meta_index].pointers[i];
	
	while(copy_size > 0)
	{
		// If the remaining number of bytes we need to copy is less than BLOCK_SIZE then
      		// only copy the amount that remains. If we copied BLOCK_SIZE number of bytes we'd
        	// end up with garbage at the end of the file.
		if( copy_size < BLOCK_SIZE )
		{
			num_bytes = copy_size;
		}
		else
		{
			num_bytes = BLOCK_SIZE;
		}
		
		fwrite( file_data[block_index],num_bytes,1,ofp );
		
		copy_size -= BLOCK_SIZE;
		offset += BLOCK_SIZE;
		
		fseek( ofp, offset, SEEK_SET );
		
		/* copy next block_index from meta structure */
		i++;
		block_index = metaArray[meta_index].pointers[i];
		
	} 
	
	fclose(ofp);
	/* everything is good */
	return 0;
}

/**
 * List all the files in the system. 
 * [size] [date] [file_name]
 * /params	commands
 * /return 0 on success; 1 on failure
 **/
int list(char *commands[MAX_CMND])
{
	int i, caught=0,j;
	
	for(i=0; i < MAX_FILES; i++)
	{
		if(metaArray[i].meta[0]) 
		{
			printf("%d \t %s \t %s \n",metaArray[i].totals_and_flags[0],
					metaArray[i].meta[1],metaArray[i].meta[0]);
			caught = 1;
			if(DEBUG)
			{
				printf("\n\tBlocks used: ");
				for(j=0; j<MAX_BLOCKS; j++)
				{
					if(metaArray[i].pointers[j] != -1)
					{
						printf(" %d ",metaArray[i].pointers[j]);
					}
				}
				printf("\n");
			}
		}
	}
	
	if(caught == 0)
	{
		return 1;
	}
	/* everything is good */
	return 0;
}

/**
 * delete a file from the metaArray & file_data
 * /params commands
 * /return 0 on success; 1 on failure
 **/
int del(char *commands[MAX_CMND])
{
	int i,j,caught=0;
	/* search metaArray for file to copy */
	for(i=0; i < MAX_FILES; i++)
	{
		if(metaArray[i].meta[0])
		{
			if( strcmp( metaArray[i].meta[0], commands[1] ) == 0 )
			{
				caught=1; 
				/* find the bits */
				for(j=0; j < MAX_BLOCKS; j++)
				{
					if( metaArray[i].pointers[j] != -1 )
					{	/* kill the bits */
						bits[metaArray[i].pointers[j]] = 0;
						/* kill the pointers array */
						metaArray[i].pointers[j]=-1;
					}
				}
				
				/* now that file_data is 'errased', void important metaArray data */
				for(j=0; j < META; j++)
				{	/* metaArray[i].meta[0] is used to find a spot for a new file. */
					metaArray[i].meta[j] = NULL;
					metaArray[i].totals_and_flags[j] = 0;
				}
			}
		}
	}
	
	if(caught==0)
	{
		printf("del error: File not found.\n");
		return 1;
	}
	
	/* everything is good */
	return 0;
}

/**
 * calculate the freespace left inside the array (free blocks)
 * /params Nothing
 * /returns integer 
 **/
int freespace()
{
	int i=0, count=0;
	for(i=0;i<MAX_FILES;i++)
	{	/* find all files using a filesize */
		if(metaArray[i].meta[0])
		{
			count += metaArray[i].totals_and_flags[0];
		}
	}
	
	return DISK_SPACE - count;
}

/**
 * Input function; collect input from user; prompt: msh>; tokenize
 * Just in case return the amount of commands entered if needed in future
 * /returns: (int) amount of commands & adds to (char) array; commands
 * /params: char array pointer to 2D char array (preallocated)
 **/
int input(char *commands[MAX_CMND], int argc, char *argv[])
{
    char *buffer, *input = (char*) malloc( MAX_INPUT * sizeof(char) );
    int i = 0, countD=-1, opt;

    /* shell prompt */
	printf("mfs> ");
	
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
					printf("More than %d, commands/arguments entered, ignored extra.\n",MAX_CMND); 
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
 * /returns: a zero or a 1(failure)
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
	else if( commands[0] && strcmp(commands[0],"put")==0 )
	{
		if(commands[1]) /* verify correct arguments for replace */
		{	
			if( strlen(commands[1]) > CMND_LEN ) 
			{
				printf("put error: Filename too long.");
				return 1;
			}
			/* all arguments accounted for */
			return 0;
		}
		else
		{	/* missing arguments */
			printf("missing input: put <filename>\n");
			return 1;
		}
	}
	else if( commands[0] && strcmp(commands[0],"get")==0 )
	{
		if( commands[1] ) /* verify correct arguments for replace */
		{	
			if( strlen(commands[1]) > CMND_LEN )
			{
				printf("get error: Filename too long.");
				return 1;
			}
			/* all arguments accounted for */
			return 0;
		}
		else if( commands[1] && commands[2] )
		{
			if( (strlen(commands[1]) > CMND_LEN ) || (strlen(commands[2]) > CMND_LEN) )
			{
				printf("get error: Filename too long.");
				return 1;
			}
			/* all arguments accounted for */
			return 0;
		}
		else
		{	/* missing arguments */
			printf("missing input: get <filename> OR get <filename> <newfilename>\n");
			return 1;
		}
	}
	else if( commands[0] && strcmp(commands[0],"del")==0 )
	{
		if(commands[1]) /* verify correct arguments for replace */
		{	/* all arguments accounted for */
			return 0;
		}
		else
		{	/* missing arguments */
			printf("missing input: del <filename>\n");
			return 1;
		}
	}
	else if( commands[0] && strcmp(commands[0],"list")==0 )
	{
		return 0;
	}
	else if( commands[0] && strcmp(commands[0],"df")==0 )
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
	
    printf("\nIndexed File System Help\n");
    printf("----------------------------------------------\n");
    printf("help\t\t- displays this message\n");
    printf("quit or exit\t- exits\n");
    printf("put <filename> \t\t Copy the file to the file system.\n");
    printf("get <filename> \t\t Retrieve the file from the file system.\n");
    printf("get <filename> <newfilename> Retrieve the file from the file system.\n");
    printf("                             and place it in the file named <newfilename>.\n");
    printf("del <filename>\t\tDelete the file from the file system.\n");
    printf("list \t\t List the files in the file system.\n");
    printf("df \t\tDisplay the amount of disk space left in the file system.\n\n");
}

/**
 * Display the welcome message
 * /returns:NOTHING
 * /params: NONE
 **/
void welcome()
{	// start with a fresh line for input
	printf("\n.::Indexed file System::.\n");
	printf("\t5 MB (5242880 bytes) of disk space\n"); 
	printf("\tfiles up to 131,072 bytes in size.\n");
    	printf("\tup to 128 files.\n");
    	printf("\tblock size of 4096 bytes.\n");
    	printf("\tfile names of up to 255 characters.\n"); 
}
