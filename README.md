# Shells
using C to code a basic shell program; takes special commands that perform basic functions

# shell.c

# multithreaded_word_search.c
  1. You will be provided a large text file , shakespeare.txt, to use as your data file. The application will memory map this file and perform all searches on the file when it is memory resident.
  2. Your application process will spawn the requested number of threads and split the search task evenly among them. A command of search Mercutio 10 will spawn 10 threads which will each search 1/10 of the text.
  3. Your application will, using the system call gettimeofday calculate the time each query took and report that back to the client as well
  4. The output shall be formatted as:
    1. Found [number of instances found] of [word] in [number of microseconds] microseconds
  5. The replace command will search the file for [word 1] and replace every instance found with [word 2]. If [word 2] is small than [word 1] then pad [word 2] with spaces. For example, if [word 1] is “Mercutio” and [word 2] is “Steve” you would replace “Mercutio” with “Steve “. ( 3 spaces padding the right potion of the string )
  6. The reset command will un-map the file, copy/replace shakespeare.txt with shakespeare_backup.txt and re-map shakespeare.txt
  7. strcmp or strncmp shall not be used to search the file for the user specifiedword. Youmayusestrcmporstrncmptocheckforthecommand“quit”, “help”, and “search”.
