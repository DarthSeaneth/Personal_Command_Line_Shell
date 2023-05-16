Sean M. Patrick (smp429) & Fulton R. Wilcox (frw14)

CS214 Systems Programming Project II: My little shell

Implementation Description: 

MyShell takes in commands either though standard input or a text file, and commands are separated by a newline 
character regardless of the input source.  The commands are read using POSIX commands
and tokenized using an implemented and modularized tokenizer with extended syntax to support escape characters.
The tokenizer stores each token of a command in an arraylist, allowing commands
of undefined size.  The tokenizer also has the function of wildcard expansion by replacing wildcard arguments with
the matches in the given directory.  Once a command is tokenized and stored, and a newline character is detected,
the command can be processed.  The "exit", "cd", and "pwd" functions are implemented, while other functions
are called using execv(). 
The shell is essentially an input/output loop with most of its functionality happening in the background in between each command line input.
The processInput() function first checks to see if the implemented functions 
are being requested, in which case they are called and the command is complete.  Otherwise, the function
checks the first token of the command for a '/' character which indicates that the first token is a 
path to an executable.  If this is not the case, then the command must be a bare name.  In this case, the
shell will call a function to search through the given 6 directories for the command and return true
if the command is found and executed.  If this is not the case, then the shell does not recognize the command
and an error will be thrown.  Both bare name and path name executables will call the execute() function 
which takes in the path name and the arguments which have already been processed in the SearchCommands() or
Process_Custom_Executable() function, respectively.  The execute() function is what properly sets input and output 
for redirection and piping.  Conflicts between pipes and redirection result in an error.  The executable is then
executed in a child process and the exit status obtained.  
Our shell also supports the use of the home directory shortcut within a token containing a path, indicated by a path starting with "~/".
When a command token contains a path starting with "~/", the "~" in the token will be replaced with the user's home directory and then that new token will be passed.
When the command "cd" is called with no arguments, the working directory is changed to the user's home directory.
Wildcard expansion is done by searching through the desired directory for any files with the given criteria, and an arraylist is build with each matching file, which
is then used to expand the original wildcard token by replacing it with all of the matching files obtained. If no matches are found, the wildcard token will be passed 
unchanged.

Functions: 
    void interpret(char *cmdline, array_list *al, array_list *wildcard_al)
        - Input tokenizer
        - Iterates through buffer array which contains user input, characters of interest include spaces, newlines, pipes, redirects.
        - When a character of interest is reached, the input is copied into a temporary string using memcpy in the case of no escape characters
        and a custom implementation to handle escape characters if there is a '\' detected.
        - If any token containing a path contains the home directory shortcut "~", the "~" will be replaced with the user's home directory.
        - If any token is a wildcard (contains a "*"), wildcard expansion is performed and the wildcard arguments are added to the arraylist,
        replacing the original token. If no matches were found during wildcard expansion, the token will be passed unchanged.
        - A token is pushed into an arraylist containing all previous tokens.
        - If a newline character is detected, processInput() is called to execute the command.

    void process_Custom_Executable(array_list *al)
        - Checks executable using stat to verify existence of executable, returns failure and throws error if executable
        does not exist.  Otherwise, argument array is populated and passed into execute function.

    void processInput(array_list *list)
        - Takes pointer to tokenized arraylist as argument.  Self-implemented functions (cd, exit, pwd) are checked first and executed if they
        are a match.  Otherwise, first argument is searched for a '/' character which indicates that it is an executable, and calls 
        process_Custom_Executable if this is the case.  Otherwise, searchCommands is called to search through batch commands and returns 
        if searchCommands returns true.  Otherwise, the command is undefined and an error is thrown.

    int processWildcard(array_list *wildcard_al, char *wildcard_token)
        -Takes pointer to an arraylist specifically for building the expansion of the wildcard, and the wildcard string token itself as arguments
        -First the function gathers necessary information about the wildcard token string, such as if it contains an absolute path, if it contains a 
        specific file type to search for, where exactly the wildcard identifier "*" is, and if there are starting/ending patterns to search files for.
        -Based on the previous informtation, the function is broken up into specific cases based on the specific type of wildcard token.
        -Then we search through the desired directory, adding any files that match the criteria into the wildcard arraylist.
        -Returns 1 if the wildcard expansion was successful and file matches were found, or 0 if no files match the wildcard pattern.

    void pwd()
        Prints the path of the working directory by calling getcwd()

    void changeDir(char *path)
        - Takes in a string that is the desired directory, and changes the working directory if path is a valid path

    void cleanUp(char *cmdline)
        - Takes pointer to string represening the parsed command line
        - Resets variables and frees necessary data associated with building the parsed command line
        - Used in the function IOLoop()

    void IOLoop()
        - Main input/output loop of the shell
        - Uses POSIX function read() to read data from standard input which is the command line for the shell, and builds a string representing 
        the command line until no more data is present in the command line.
        - Calls the function interpret() when the command line is fully parsed and ready to be tokenized

    int searchCommands(array_list *al)
        - Searches through paths "/usr/local/sbin/", "/usr/local/bin/", "/usr/sbin/", "/usr/bin/", "/sbin/", "/bin/" in that
        order for a batch command, populates array with arguments and executes using execute function if it is found.  Returns false
        if nothing was executed and true if something was executed.

    void execute(char** args, int numArgs)
        - Main function to execute executables after setting input and output source.
        - Checks for '|', '>', '<' and throws error when a conflict is detected.  Combinations of |, <, > are divided into
        cases where dup() is called to save original stdin/stdout, and dup2() is subsequently called to set input and/or output
        based on the case.  
        - I/O can be changed between calls depending on the user input (pipes/multiple redirects)
        - callExec is called when input and output is set appropriately and executable is executed.
        - When execution is finished, saved stdin and stdout is restored.

    void callExec(char** args)
        - Calls fork to create a child process, then calles execv to execute if a child process was successfully created.
        Returns after child process is completed or fork/execv fails.

    char* getFileType(char *file_name)
        - Takes a pointer to a string representing the name of a specific file as an argument
        - Returns a string representing the type of file, or NULL if the file is an executable, a special directory entry ("." or ".."), or a hidden file
        - Used for wildcard expansion in processWildcard()

    char* getFileEndPattern(char *file_name, int patternLength)
        - Takes pointer to string representing the name of a file, and an integer representing the length of the file ending pattern to check for as arguments
        - Ignores the file extension
        - Returns string representing the ending pattern of the file, or NULL if the ending pattern length is larger than the ending pattern of the file
        - Used for wildcard expansion in proessWildcard()
        
    char* getFileEnd(char *file_name, int patternLength)
        - Takes pointer to string representing name of a file, and an integer representing the length of file end pattern to check for as arguments
        - Includes the file extension as part of the file name
        - Returns string representing ending pattern of the file, or NULL if ending pattern is larger than file ending pattern, or special directory entries
        - Used for wildcard expansion in processWildcard()

    char* getFileStartPattern(char *file_name, int patternLength)
        - Takes pointer to string representing the name of a file, and an integer representing the length of the file ending pattern to check for as arguments
        - Returns string representing the ending pattern of the file, or NULL if the ending pattern length is larger than the ending pattern of the file
        - Used for wildcard expansion in processWildcard()

    char* getFileName(char *file_name)
        - Takes pointer to string representing name of a file as an argument
        - Returns a string representing the name of the file, ignoring the file extension, or NULL if special directory entry or executable file because the function
        is only used when searching for file matches of a specific file type (non-executable)
        - Used for wildcard expansion in processWildcard()

    int isExecutable(char *file_name)
        - Takes pointer to string representing name of a file as an argument
        - Returns 1 if the file is an executable, 0 otherwise
        - Used for wildcard expansion in processWildcard()

    void handleWildcardMatch(int absolutePath, char *file_name, char *path, array_list *wildcard_al)
        - Takes int representing if an absolute path is present in wildcard token, pointer to string of file name, pointer to string of desired path of directory to
        search through, and pointer to wildcard arraylist as arguments
        - Pushes desired file name into the wildcard arraylist when a match is found, including the absolute path when necessary

    int containsWildcard(char *cmdstring)
        - Takes pointer to string representing parsed command line as argument
        - Returns 1 if a token is a wildcard (contains a "*"), 0 otherwise
        - Used in interpret() to check for wildcard tokens

    int containsHomeDirShortcut(char *cmdstring)
        - Takes pointer to string representing parsed command line as argument
        - Returns 1 if a token contains a path starting with "~/", 0 otherwise
        - Used in interpret() to check for home directory shortcut within a token containing a path

    char* specialHandlingMemCopy(char* src, int size)
        - Replaces memcpy used in tokenizing the command when escape characters are used.
        - Adjusts the size needed to store the new characters from the deletion of '\' characters and
        copies token as appropriate with the spacial handling of escape characters.
        - Returns the new token to be used in interpret().

Extensions:
    Home Directory: We implemented functionality for the home directory shortcut such that for any command token containing a path, if that path starts with
    "~/" which is the home directory shortcut, then the "~" will be replaced with the user's home directory and the new token will be passed
    Using the command "cd" with no arguments will also change the working directory to the user's home directory
    
    Escape Sequences: We implemented functionality to extend the command syntax to allow for "escaping" of special characters as described in the 
    assignment description

Test Plan:
    Our testing plan had many parts and we utilized the plan as we implemented each part of the shell
    After we implemented each part of the shell, we would then extensively test different scenarios of the command line input in order to perfect 
    the part we were implementing
    Upon implementing the main I/O loop and parsing input, it was important to check what our parsed input looked like
    This process allowed us to see the errors and how we were parsing the input which in turn allowed us to fix the main loop and successfully parse the input
    The next part of the process was building the argument list based on the parsed input, i.e., tokenizing the input
    It was important for us to check the contents of our arraylist that was being used to store each command token, and this allowed us to see any errors we had
    when we were interpretting the parsed input and building tokens, as well as this allowed us to see exactly how the tokens were being created
    The next part was processing tokenized input that was populated in the arraylist, and it was important for us to see the contents on the arraylist at each point
    in order to make sense of what commands were being called
    We then implemented the parts of the shell that were required to be manually implemented such as the commands "cd", "pwd", and "exit"
    We then extensively tested different cases for each of those commands, which allowed us to fix any errors we encountered
    The next part was to allow for the shell to execute built-in commands such as "ls", and many others as well as to execute different executable programs
    Upon implementing this, we tested different scenarios for these cases which revealed errors and allowed us to perfect this part
    At each part of the testing process, we also tested bad commands in order to ensure that the shell responded appropriately
    The next parts of the process was implementing file redirection, piping, wildcard expansion, home directory shortcut, and escape sequences.
    Upon implementing piping and file redirection, we extensively tested different scenarios of these cases which allowed us to find all of our mistakes, undefined 
    behavior, as well as other errors with our implementation. We also tested bad inputs to make sure we were handling those cases properly
    Upon implementing wildcard expansion, we thoroughly tested every possible wildcard token, whether it was a bare wildcard token or a wildcard token containing an 
    absolute path, as well as scenarios in which no matches were found in order to see if the files being found were correct, any errors with the process, and to check
    for the case when no matches are found and the wildcard token would be passed unchanged
    Upon implementing the home directory shortcut, we repeated many of the previous testing processes and used the home directory shortcut as part of any tokens
    containing a path in order to check if it was expanding the path correctly, which allowed us to perfect the implementation and successfully allow for the 
    home directory shortcut functionality
    After implementing everything, we then repeated many of the previous testing strategies by combining all of the different scenarios together. For instance,
    it was important to test commands that included many of the different possibilities that a command could have, such as testing commands that included file
    redirection, piping, wildcards, escape sequences, home directory shortcut, etc. This was arguably the most important part as we were able to see how our program 
    was handling all of the different special tokens and potential argument expansion, etc.
    All in all, after we tested each part of the implementation and then everything together, we were able to fix many issues and errors that arose as well as
    modularize our code and perfect the implementation.
    
    We have also included text files for batch mode testing as well as additional testing programs as described below:

    EscapeChar_Test.txt:
        - Tests a series of escape chars including pipes, redirects, and newlines using the echo function.  Test>.txt was created by executing memgrind
        and redirecting output, escape character was used to create text file.

    Piping/redirection:
        - TestProgram.c/TestProgram2.c both read from stdinput and print out what they receive, used for redirection and piping testing.  Designed to test for
        up to 500 chars.

    BadCommands.txt:
        - Tests a series of commands that are invalid or contains bad syntax and will cause some sort of error. This is to be used in batch mode by launching
        the shell program like so: ./mysh BadCommands.txt
        - This test shows how our shell handles bad input and outputs appropriate error messages as necessary
    
    WildcardsHomeDirTest.txt:
        - Tests a series of commands containing wildcard tokens as well as the home directory shortcut (and both simultaneously). This is to be used in batch mode
        by launching the shell program like so: ./mysh WildcardsHomeDirTest.txt
        - This test shows how our shell expands wildcard tokens when necessary, as well as how the shell expands the path containing the home directory shortcut.
        It shows how we correctly handle any matches found during the wildcard expansion process, and how we correctly build arguments based off of that expansion
