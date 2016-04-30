# Shells
using C to code basic shell programs; takes special commands that perform basic functions.  Exercise in courses to learn operating systems.

# shell.c
  1. Your program will print out a prompt of msh> when it is ready to accept input. It must read a line of input and, if the command given is a supported shell command, it shall execute the command and display the output of the command.
  2. If the command is not supported your shell shall print the invalid command followed by “: Command not found.”
  3. If the command option is an invalid option then your shell shall print the command followed by “: invalid option --” and the option that was invalid as well as a prompt to try —help.
  4.  If the user types a blank line, your shell will, quietly and with no other output, print another prompt and accept a new line of input.
  5.  Your version of Mav shell shall support up to three command line parameters in addition to the command. Table 1 below shows which commands shall support optional parameters.
  6.  You do not have to support redirection or pipes: “>”, “<”, “|”.
  7.  You do not have to support backgrounding the process with “&”
  8.  Parameters may also be combined. For example, ps may be executed as: ps –aef orps –a –e -f
  9.  Mav shell shall be implemented using fork(), wait() and one of the exec family of functions. Your Mav shell shall not use system(). Use of system() will result in a grade of 0.
  9.  Typing ctrl-c or ctrl-z shall not stop or kill your shell. Your shell shall ignore those keystrokes.


# multithreaded_word_search.c
  1. You will be provided a large text file , shakespeare.txt, to use as your data file. The application will memory map this file and perform all searches on the file when it is memory resident.
  2. Your application process will spawn the requested number of threads and split the search task evenly among them. A command of search Mercutio 10 will spawn 10 threads which will each search 1/10 of the text.
  3. Your application will, using the system call gettimeofday calculate the time each query took and report that back to the client as well
  4. The output shall be formatted as:
    1. Found [number of instances found] of [word] in [number of microseconds] microseconds
  5. The replace command will search the file for [word 1] and replace every instance found with [word 2]. If [word 2] is small than [word 1] then pad [word 2] with spaces. For example, if [word 1] is “Mercutio” and [word 2] is “Steve” you would replace “Mercutio” with “Steve “. ( 3 spaces padding the right potion of the string )
  6. The reset command will un-map the file, copy/replace shakespeare.txt with shakespeare_backup.txt and re-map shakespeare.txt
  7. strcmp or strncmp shall not be used to search the file for the user specifiedword. You may use strcmp or strncmp to check for the command “quit”, “help”, and “search”.
