/**
 * Chris Gilardi - 820489339 - cssc0101
 * CS 570 - Dr. J. Carroll
 * Program Four - Due 11/28/18 (Extra Credit Deadline)
 **/

#include <stdio.h>
#include <signal.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <limits.h>
#include <dirent.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <pwd.h>
#include "CHK.h"
#include "p2.h"
#include "getword.h"


/*
 * DEFINITIONS:
 * - MAXARGS represents the maximum number of arguments that can be sent to a single command (100)
 * - MAXSTORAGE represents the resultant maximum total size of n arguments where n <= 100.
 */
#define MAXARGS 100
#define MAXSTORAGE 25500

/**
 * STORAGE/VARS
 * - argv: Holds a (possibly) long, null-terminated string that includes all arguments sent by the user (loaded in parse())
 * - newargv: A char pointer array that allows each argument to be referenced easily, as there is one 'string' referenced per index.
 * - inputFile: Stores a pointer to somewhere in argv that holds the name of an input file (for input redirection)
 * - outputFile: Stores a pointer to somewhere in argv that holds the name of an output file (for output redirection)
 * - pipeArgsIndexes: The array of indexes where the arguments AFTER a pipe should start, so that execvp doesn't simply run the same command
 **/
char argv[MAXSTORAGE];
char* newargv[MAXARGS];

char* inputFile;
char* outputFile;
char* hereis_delimiter;

int pipeArgsIndexes[10];
int numPipes = 0;
// The pipe is defined here to avoid re-creating it every time. It is perfectly OK to re-use this pipe for any subsequent pipe commands.
// DEBUG: SET TO 20 SO WE CAN HANDLE 10 PIPES
int pipeFileDescriptor[20];

int tempPipe[2];

int DEBUG = 0; // REMOVE THIS

/**
 * FLAGS (note: for each 'bool_' variable, 0 == FALSE, !0 == TRUE
 * - bool_encounteredInput: Will be TRUE if parse() encountered an input directive '<'
 * - bool_encounteredOutput: Will be TRUE if parse() encountered an output directive '>'
 * - bool_encounteredPipe: Will be TRUE if parse() encountered a pipe directive '|'
 * - bool_nowait: Will be TRUE if parse() encountered a "continue directive" (&) at the end of the line.
 * - bool_encounteredHereis: Will be TRUE if parse() encountered a hereis directive '<<'.
 * - bool_hasChangedDir: Will be TRUE as soon as cd is called to change the directory.
 * -
 **/
int bool_encounteredInput = 0;
int bool_encounteredOutput = 0;
int bool_encounteredPipe = 0;
int bool_nowait = 0;
int bool_encounteredHereis = 0;
int bool_hasChangedDir = 0;

/* cwd holds the string representation of the current working directory.*/
char cwd[256];

/**
 * Function Headers
 **/
void printPrompt();
void handleSignal(int signal);
int parse();
void resetFlags();
int isMetacharacter(char* stringToCheck);
void handleMultiplePipes(void);

int main() {
    
    // This line registers our signal handler (handleSignal) to handle a system SIGTERM (kill). It flags to the shell that we will be handling this, to deflect the default behavior.
    (void) signal(SIGTERM, handleSignal);
    
    // We set our process group ID here to ensure that running a SIGTERM or exiting the program (or using killpg) will not crash Carroll's autograder, and will make sure children are included in the same process group.
    setpgid(0,0);
    
//    temp = tmpfile();
    
    /**
     * RUN LOOP
     * This loop handles much of the logic in the program.
     * We BREAK from this loop if the first argument is EOF, otherwise we continue, or fork() to create a child process to run each command.
     **/
    while(1) {
        
        int numArgs = 0;
        pid_t procId;
        
        // Prints ":570: ", to show the user that the next prompt is ready.
        printPrompt();
        
        // Parses stdin string and returns the number of args, OR
        // 		returns -255 for EOF on first word OR
        // 		returns -1 for an error
        //      returns 0 for an empty line
        // SIDE EFFECTS
        //    - fills newargv[] and (by extension) argv[] with the contents of each word on stdin.
        //    - Sets above flags based on input, for use later in the program.
        numArgs = parse();
        
        // If EOF has been found to be the first word (i.e. the input stream is over - from a file usually), break and end the program.
        if(numArgs == EOF) {
            break;
        }
        
        // numArgs will be < 0 if parse() encountered any kind of error. To recover, we reset any flags that may have been set in parse() and re-run the loop.
        if(numArgs < 0) {
            resetFlags();
            continue;
        }
        
        // If the input line is empty (i.e. by just pressing return with nothing else) we run this block.
        if(numArgs == 0) {
            // If we found a left carat '<' in the output (like '< myfile.txt'), that's an error. Let the user know before we continue.
            if(bool_encounteredInput) {
                fprintf(stderr, "error: Cannot redirect input to nothing.\n");
            }
            resetFlags();
            continue;
        }
        
        /**
         * BUILT-IN COMMAND cd
         * This command is one of the few that does not need to be fork()'ed to work correctly (it would be pointless to fork)
         * Since cd cannot take arguments other than one representing the folder to move through (except '&' as a background process), any other args should throw an error.
         **/
        if(strcmp(newargv[0], "cd") == 0) {
            if(bool_nowait) {
                bool_nowait = 0;
            }
            
            // If 'cd' is run by itself, change directory to $HOME
            if(numArgs < 2) {
                // Get the environment variable for $HOME
                // TODO: HANDLE ERRORS
                char* homeDir = getenv("HOME");
                if(chdir(homeDir) < 0) {
                    switch (errno) {
                        case EACCES: // permission denied
                            fprintf(stderr, "cd: %s: permission denied.\n", homeDir);
                            break;
                        case ENOENT: // does not name existing directory
                            fprintf(stderr, "cd: %s is not a file or directory.\n", homeDir);
                            break;
                        case ENOTDIR: // The file is not a directory
                            fprintf(stderr, "cd: %s is not a directory.\n", homeDir);
                            break;
                        default: // Unknown error
                            fprintf(stderr, "cd: an unknown error has occurred.\n");
                            break;
                    }
                    continue;
                }
            } else if(numArgs > 2) {
                // If there is more than one argument after 'cd', throw an error.
                fprintf(stderr, "%s", "cd: Too many arguments.\n");
                continue;
            } else {
                // If chdir is run normally ('cd mydir'), we should end up here.
                // We check for errors - permissions, not found, etc. 'errno' gets set automatically (as a global, extern var).
                if(chdir(newargv[1]) < 0) {
                    // Though we werent asked specifically to handle multiple kinds of errors, I wanted to implement this kind of error checking for my own improvement as well as to make my program more complete
                    switch (errno) {
                        case EACCES: // permission denied
                            fprintf(stderr, "cd: %s: permission denied.\n", newargv[1]);
                            break;
                        case ENOENT: // does not name existing directory
                            fprintf(stderr, "cd: %s is not a file or directory.\n", newargv[1]);
                            break;
                        case ENOTDIR: // The file is not a directory
                            fprintf(stderr, "cd: %s is not a directory.\n", newargv[1]);
                            break;
                        default: // Unknown error
                            fprintf(stderr, "cd: an unknown error has occurred.\n");
                            break;
                    }
                    continue;
                }
                
            }
            if(!bool_hasChangedDir) {
                bool_hasChangedDir = 1;
            }
            continue;
        }
        
        /**
         * BUILT IN COMMAND environ
         * This command takes one or two arguments, 1 arg prints the value of the environemnt variable, 2 arg sets it.
         */
        if(strcmp(newargv[0], "environ") == 0) {
            // Check arg counts
            if(numArgs < 2) {
                /// We need at least 'environ [WORD]'
                fprintf(stderr, "%s\n", "environ: Not enough arguments.");
            } else if(numArgs > 3) {
                /// more than 'environ [WORD] [newval]' would be ambiguous.
                fprintf(stderr, "%s\n", "environ: Too many arguments.");
            } else if(numArgs == 2) {
                // Hooray! We have the right amount of arguments to read a var.
                char* env = getenv(newargv[1]);
                if(env != NULL) {
                    printf("%s\n", env);
                } else {
                    // If the var is null, print a blank line.
                    printf("\0\n");
                }
            } else {
                /// Otherwise, we want to set the variable. Print an error if there is one.
                if(setenv(newargv[1], newargv[2], 1) == -1) {
                    fprintf(stderr, "environ: %s\n", strerror(errno));
                }
            }
            continue;
        }
        
        // Make sure there are no extraneous characters in stdout/stderr, print everything we got last time, if anything's left.
        fflush(stdout);
        fflush(stderr);
        
        /**
         * CHILD PROCESS EXECUTION LOGIC
         * This is the most important part of the program, as it handles execution, output redirection, waiting, error handling, and everything else in the program.
         * The process begins by fork()ing itself. The parent program then wait()s for its child (unless told not to by placing an un-escaped & at the end of the line. See below for a more detailed explanation of each action within.
         **/
        if((procId = fork()) == 0) {
            
            // Setup file descriptors. In case we need to change stdin/stdout later in the program, declare the ints here.
            int inputFileDescriptor = 0;
            int outputFileDescriptor = 0;
            int nullIn = 0;
            
            // If the input flags are set, we want to change the child's stdin to the specified file.
            if(bool_encounteredInput) {
                // NOTE ON open(): Will return -1 if there is an error, the error value is stored in errno, which we can then test after (hence the switch statements).
                // We attempt to open the input file relative to the current working directory.
                if((inputFileDescriptor = open(inputFile, O_RDONLY)) < 0) {
                    // Though we werent asked specifically to handle multiple kinds of errors, I wanted to implement this kind of error checking for my own improvement as well as to make my program more complete
                    switch (errno) {
                        case EACCES:
                            fprintf(stderr, "error: %s: permission denied.\n", inputFile);
                            break;
                        case ENOENT:
                            fprintf(stderr, "error: %s: file does not exist.\n", inputFile);
                            break;
                        default:
                            break;
                    }
                    exit(1);
                }
                
                // If the open succeeds, copy the new file descriptor into the process's STDIN.
                dup2(inputFileDescriptor, STDIN_FILENO);
            }
            
            // This logic handles program input redirection when the command should be run in background (&) and DOES NOT already have a file piping to stdin.
            if(bool_nowait == 1 && !bool_encounteredOutput) {

                // Open the /dev/null file.
                // NOTE: /dev/null is an empty file, used specifically for the purpose (in this context) of un-setting STDIN, so it doesn't get extra (unintended) input.
                if((nullIn = open("/dev/null", O_WRONLY)) < 0) {
                    fprintf(stderr, "error: Unable to open /dev/null.\n");
                    exit(2);
                }
                
                // Dup the null output to the process's stdout.
                dup2(nullIn, STDIN_FILENO);
            }
            
            // This block handles a command whose output should be redirected to a file (after finding '>').
            if(bool_encounteredOutput) {
                
                // Flags here are used to indicate that:
                //  - O_WRONLY: Open for writing
                //  - O_EXCL: Only write if the file does not already exist (do not append)
                //  - O_CREAT: Create the file if it does not exist
                int flags = O_WRONLY | O_EXCL | O_CREAT;
                
                // filePerms gives the user Read (IRUSR)/Write (IWUSR) access only.
                int filePerms = S_IRUSR | S_IWUSR;
                
                // NOTE ON open(): Will return -1 if there is an error, the error value is stored in errno, which we can then test after (hence the switch statements).
                // We attempt to open the input file relative to the current working directory.
                if((outputFileDescriptor = open(outputFile, flags, filePerms)) < 0) {
                    // Though we werent asked specifically to handle multiple kinds of errors, I wanted to implement this kind of error checking for my own improvement as well as to make my program more complete
                    switch (errno) {
                        case EEXIST:
                            fprintf(stderr, "error: %s: file already exists.\n", outputFile);
                            break;
                        default:
                            break;
                    }
                    exit(3);
                }
                dup2(outputFileDescriptor, STDOUT_FILENO);
            }
            
            /// This block handles a command where the input will come on multiple lines delmited by a delimiter ('hereis': <<).
            if(bool_encounteredHereis) {
                // Storage required to check each line.
                char* line = NULL;
                size_t len = 0;
                ssize_t nread;
                
                // Piping seemed to be the easiest way to both read from AND write to a file, so I decided to use it over tmpfile().
                pipe(tempPipe);
                
                // Read the entire file.
                while((nread = getline(&line, &len, stdin)) != -1) {
                    // Null-terminate each line, which removes the \n character (for checking).
                    line[strlen(line) - 1] = '\0';
                    // If this line exactly equals the delimiter, break out of the inner loop.
                    if(strcmp(line, hereis_delimiter) == 0) {
                        goto afterloop;
                    }
                    
                    // Add the newline character back so that there are seperate lines in the pipe.
                    line[strlen(line)] = '\n';
                    
                    // Write the line to the input end of the pipe.
                    write(tempPipe[1], line, nread);
                }
                
                // IF WE REACH THIS POINT, we have read an entire input file and did NOT find the delimiter. We say as such and exit.
                fprintf(stderr, "hereis: Did not find delimiter in file.\n");
                exit(4);
            afterloop:
                // Clean up, close the read end of the pipe and then dup it to our current process's STDIN.
                close(tempPipe[1]);
                dup2(tempPipe[0], STDIN_FILENO);
                close(tempPipe[0]);
            }
            
            // This block of code sets up a vertical piping mechanism between the current child and a grandchild.
            if(bool_encounteredPipe) {
                if(numPipes > 1) {
                    // Break into a different subroutine.
                    handleMultiplePipes();
                } else {
                    int pipeProc;
                    
                    // This line "pipes" (links) two file descriptors - pipeFileDescriptor[0] and pipeFileDescriptor[1].
                    // It is created here because we need a copy of both in this process and its child, so it can be closed in both and not lock the program.
                    pipe(pipeFileDescriptor);
                    
                    fflush(stdout);
                    fflush(stderr);
                    // This creates a 'grandchild' relative to the shell's process.
                    pipeProc = fork();
                    
                    // This grandchild runs the LEFT side of the pipe, because there is no feasible way to wait for the parent to complete first (as that would kill the child)
                    if(pipeProc == 0) {
                        // Setup the 'input' end of the pipe to the OUTPUT of the first process to run.
                        dup2(pipeFileDescriptor[1], STDOUT_FILENO);
                        
                        // Close the file descriptors in this child before executing, or they will never be closed.
                        close(pipeFileDescriptor[1]);
                        close(pipeFileDescriptor[0]);
                        
                        // Run the FIRST program for the pipe (the one on the left)
                        execvp(newargv[0], newargv);
                        break;
                    } else {
                        // Setup the 'output' end of the pipe to the INPUT of this process (the RIGHT side of the pipe)
                        dup2(pipeFileDescriptor[0], STDIN_FILENO);
                        
                        // File descriptors must also be closed here, to ensure the pipe is completely closed.
                        close(pipeFileDescriptor[0]);
                        close(pipeFileDescriptor[1]);
                        
                        // Here we start the execvp array at pipeArgsIndex.
                        // This is because the first part of newargv is not automatically cleared/consumed, so we must start from where the args of the pipe start (including the command).
                        if(execvp(newargv[pipeArgsIndexes[1]], newargv + pipeArgsIndexes[1]) < 0) {
                            fprintf(stderr, "error: Unable to pipe into program \'%s\': ", newargv[1]);
                            switch (errno) {
                                case EACCES:
                                    fprintf(stderr, "%s: Permission denied.\n", newargv[1]);
                                    break;
                                case ENOENT:
                                    fprintf(stderr, "%s: File not found.\n", newargv[1]);
                                    break;
                                default:
                                    break;
                            }
                            exit(5);
                        }
                        break;
                    }
                }
            } else {
                // We hit this block if we do NOT have a pipe command.
                // This code simply runs the program using execvp().
                
                // Try to run the program, print an appropriate error to stderr if it fails.
                if(execvp(newargv[0], newargv) < 0) {
                    fprintf(stderr, "error: Unable to start program \'%s\': ", newargv[0]);
                    switch (errno) {
                        case EACCES:
                            fprintf(stderr, "permission denied.\n");
                            break;
                        case ENOENT:
                            fprintf(stderr, "file not found.\n");
                            break;
                        default:
                            break;
                    }
                    exit(6);
                }
                break;
            }
        } else {
            if(bool_encounteredHereis && !bool_encounteredInput) {
                // Storage required to check each line.
                char* line = NULL;
                size_t len = 0;
                ssize_t nread;
                // Read the entire file.
                while((nread = getline(&line, &len, stdin)) != -1) {
                    // Null-terminate each line, which removes the \n character (for checking).
                    line[strlen(line) - 1] = '\0';
                    // If this line exactly equals the delimiter, break out of the inner loop.
                    if(strcmp(line, hereis_delimiter) == 0) {
                        goto after;
                    }
                }
            after:;
            }
            // If parse() found an & and set bool_nowait to true, we want to print the process name and continue.
            if(bool_nowait) {
                printf("%s [%d]\n", newargv[0], procId);
            } else {
                // This block ensures that the parent waits for completion (i.e. through exit(..)) of its child program.
                for(;;) {
                    pid_t pid;
                    pid = wait(NULL);
                    if(pid == procId) {
                        break;
                    }
                }
            }
        }
        
        // Reset all flags for the next command.
        resetFlags();
    }
    
    //We were required to use this line to kill our program after reaching an EOF or breaking the loop above.
    // These three lines will kill all children and print a custom exit message, then exit success (0).
    killpg(getpgrp(), SIGTERM);
    printf("p2 terminated.\n");
    exit(0);
}

/**
 * This method simply resets all of the flags used in the main() method, through various means, either by setting their value to 0, NULL, or INT_MAX (for pipe args)
 **/
void resetFlags() {
    // strings
    inputFile = NULL;
    outputFile = NULL;
    hereis_delimiter = NULL;
    
    // booleans
    bool_encounteredInput = 0;
    bool_encounteredOutput = 0;
    bool_encounteredPipe = 0;
    bool_nowait = 0;
    bool_encounteredHereis = 0;
    
    // other
    pipeArgsIndexes[0] = 0;
    numPipes = 0;
}

/**
 * This method prints ":570: ", signaling the start of a new line
 * I only made this its own method for readability (and therefore understandability) in main().
 **/
void printPrompt() {
    if(bool_hasChangedDir) {
        if(getcwd(cwd, sizeof(cwd)) != NULL) {
            if(strcmp(cwd, "/") == 0) {
                // Special case for being at the root directory, we don't want to strip away the '/'.
                printf("/:570: ");
            } else {
                // Otherwise, we want the last word of the path with no trailing '/'.
                char* endOfPath = strrchr(cwd, '/');
                printf("%s:570: ", endOfPath + 1);
            }
        }
    } else {
        printf(":570: ");
    }
}

/**
 * This method interfaces with getword to take input from stdin and tokenize it.
 * The method also sets various flags to signal to main() what it should do after the fact.
 * RETURNS: The amount of tokens (args) it's gathered.
 **/
int parse() {
    
    int wordLength = 0;
    int pointerLocation = 0;
    int wordCount = 0;
    
    int bool_inputNext = 0;
    int bool_outputNext = 0;
    int bool_pipeNext = 0;
    int bool_escAmp = 0;
    int bool_hereisNext = 0;
    int bool_escTilde = 0;
    
    int error = 0;
    
    // We loop until wordLength == 0. This would signify a line terminator.
    while((wordLength = getword(argv + pointerLocation)) != 0) {
        char* currentWord;
        
        // If getword returns EOF and this is the first word on the line, we want to end execution.
        if(wordCount == 0 && wordLength == -255) {
            return EOF;
        }
        
        // We change this back, because if it is TRUE after the loop, that is flagged as an error.
        if(bool_pipeNext) {
            bool_pipeNext = 0;
        }
        
        // wordLength will be -256 if the character is an ESCAPED ampersand (\&)
        if(wordLength == -256) {
            bool_escAmp = 1;
            wordLength = 1;
        }
        
        
        // Word length will be < -256 if there is an escaped tilde. To find the correct length, we add 258 back to the length.
        if(wordLength < -256) {
            bool_escTilde = 1;
            wordLength = abs(wordLength + 258);
        }
        
        // Here is where we fill argv and newargv
        
        // Set this char* to the correct position in argv
        newargv[wordCount] = argv + pointerLocation;
        
        // This block replaces environment variable keys with values.
        if(wordLength < 0 && !bool_escAmp && !bool_escTilde) {
            // Get the environment variable with the name of the word we're looking at.
            char* env = getenv(argv + pointerLocation);
            if(env != NULL) {
                // The environment variable exists.
                // The next 4 lines copy the value of the environment variable, null terminate, and move the pointer location
                strcpy(argv + pointerLocation, env);
                wordLength = wordLength * -1;
                pointerLocation += strlen(argv + pointerLocation);
                argv[pointerLocation] = 0;
                pointerLocation++;
                // If this IS the name of an input file, output file, or hereis delimiter, we still have things to do, otherwise go to the next word.
                if(!bool_inputNext && !bool_outputNext && !bool_hereisNext) {
                    wordCount++;
                    continue;
                }
            } else {
                // The environment variable doesn't exist.
                fprintf(stderr, "%s: Undefined variable.\n", argv + pointerLocation);
                error = -16;
                continue;
            }
        }
        
        // Null-terminate each word
        argv[pointerLocation + abs(wordLength)] = '\0';
        
        // Move the pointer forward to right after the null-termination.
        pointerLocation += abs(wordLength) + 1;
        
        // Set this pointer for ease-of-use in the function.
        currentWord = newargv[wordCount];
        
        // This block handles tilde (~) repalcement.
        if(currentWord[0] == '~') {
            char* homeDir;
            // If it's by itself, replace it with the value of $HOME.
            if(wordLength == 1 && !bool_escTilde) {
                homeDir = getenv("HOME");
            } else if(bool_escTilde) {
                // If the tilde is escaped, we don't need to do anything with it, so skip over this section.
                goto after;
            } else {
                /// This portion handles accessing users' home directories via something like ~cs570.
                
                // Storage for the results.
                struct passwd* result;
                char* secondHalf = strdup(currentWord);
                
                // Get just the very first word (if this is part of a path)
                //    i.e. ~cs570/Data4 -> ~cs570.
                homeDir = strsep(&secondHalf, "/");
                
                // If there is something beyond a '/'.
                if(secondHalf != NULL) {
                    if(strlen(homeDir) == 1) {
                        // This is something like ~/, get the current user's passwd.
                        result = getpwuid(getuid());
                    } else {
                        // This is something like ~cs570. So get passwd for '~cs570' + 1 ('cs570')
                        result = getpwnam(homeDir + 1);
                    }
                    
                    // Check to make sure we got a valid result, if not, error.
                    if(!result) {
                        fprintf(stderr, "Unknown user: %s.\n", currentWord + 1);
                        error = -14;
                    } else {
                        // This struct lets you get the pw_dir ($HOME) for the user.
                        homeDir = result->pw_dir;
                    }
                    
                    // Add the trailing slash to the newly-found directory string.
                    strcat(homeDir, "/");
                    
                    // Add the original 'second half' of the directory string.
                    strcat(homeDir, secondHalf);
                } else {
                    // If there's not a path after the word, we have it easy. Just strip the leading tilde and get passwd for the user.
                    
                    result = getpwnam(currentWord + 1);
                    
                    if(!result) {
                        fprintf(stderr, "Unknown user: %s.\n", currentWord + 1);
                        error = -10;
                    } else {
                        // This struct lets you get the pw_dir ($HOME) for the user.
                        homeDir = result->pw_dir;
                    }
                }
            }
            
            // Ensure the argv pointer points to the correct location now that it's been replaced.
            // Back up the length of the username (~, ~cs570, etc.)
            pointerLocation -= abs(wordLength) + 1;
            // Copy the homeDir string (that we found above) into argv, overwriting the above username string.
            strcpy(argv + pointerLocation, homeDir);
            // Move the pointer along to right after the null-termination done by strcpy.
            pointerLocation += strlen(argv + pointerLocation) + 1;
            // Increment the word counter.
            wordCount++;
            continue;
        }
        
    after:
        
        // Set the input file flag if the last pass found a '<'
        if(bool_inputNext) {
            inputFile = currentWord;
            bool_encounteredInput = 1;
            bool_inputNext = 0;
            continue;
        }
        
        // Set the output file flag if the last pass found a '>'
        if(bool_outputNext) {
            if(wordLength < 1) {
                fprintf(stderr, "error: Cannot redirect output to nothing.\n");
                error = -1;
            } else if(isMetacharacter(currentWord)) {
                // This handles cases like '> >', '> &', etc.
                fprintf(stderr, "error: Cannot redirect output to metacharacters (%s).\n", currentWord);
                error = -12;
            }
            outputFile = currentWord;
            bool_encounteredOutput = 1;
            bool_outputNext = 0;
            continue;
        }
        
        if(bool_hereisNext) {
            if(wordLength < 1 || isMetacharacter(currentWord)) {
                // error : need a delimiter
                fprintf(stderr, "hereis: Must have a valid delimiter.");
                error = -8;
            }
            hereis_delimiter = currentWord;
            bool_encounteredHereis = 1;
            bool_hereisNext = 0;
            continue;
        }
        
        // Set flags based on this character to be used in the next pass
        if(strcmp(currentWord, ">") == 0) {
            if(bool_encounteredOutput) {
                fprintf(stderr, "error: Ambiguous output redirect.\n");
                error = -2;
            }
            bool_outputNext = 1;
            continue;
        } else if(strcmp(currentWord, "<") == 0) {
            bool_inputNext = 1;
            if(bool_encounteredInput) {
                fprintf(stderr, "error: Ambiguous input redirect.\n");
                error = -3;
            }
            continue;
        } else if(strcmp(currentWord, "|") == 0) {
            if(wordCount == 0) {
                fprintf(stderr, "pipe: Cannot pipe from nothing.\n");
                error = -7;
            }
            bool_pipeNext = 1;
            bool_encounteredPipe = 1;
            pipeArgsIndexes[++numPipes] = wordCount + 1;
            newargv[wordCount] = NULL;
        } else if(strcmp(currentWord, "<<") == 0) {
            if(bool_encounteredHereis) {
                fprintf(stderr, "hereis: Cannot have two hereis commands.");
                error = -15;
            }
            bool_hereisNext = 1;
            continue;
        }
        
        wordCount++;
    }
    
    if(error == 0 && bool_hereisNext) {
        fprintf(stderr, "hereis: Must have a valid delimiter.\n");
        error = -8;
    }
    
    if(error == 0 && bool_inputNext) {
        fprintf(stderr, "error: Cannot get input from nothing.\n");
        error = -3;
    }
    
    // We will go into this block, and that for the pipe, if the loop broke after setting it. If that happens, we have an error and should exit.
    if(error == 0 && bool_outputNext) {
        fprintf(stderr, "error: Cannot redirect nothing to output.\n");
        error = -5;
    }
    
    if(error == 0 && bool_pipeNext) {
        fprintf(stderr, "pipe: Cannot pipe to nothing.\n");
        error = -6;
    }
    
    if(error == 0 && wordCount > 0) {
        if(strcmp(newargv[wordCount - 1], "&") == 0) {
            if(!bool_escAmp) {
                bool_nowait = 1;
                bool_escAmp = 0;
                wordCount--;
            }
        }
    }
    
    newargv[wordCount] = NULL;
    
    // If we have an error, make sure we return it so main() knows.
    if(error != 0) {
        return error;
    }
    
    return wordCount;
}

/**
 * This method does a simple check to see if the "string" passed in is a metacharacter. This is useful for error checking.
 * I only made this a function so it woudld be easier to read in parse() above.
 **/
int isMetacharacter(char* stringToCheck) {
    if(strcmp(stringToCheck, "&") == 0 || strcmp(stringToCheck, ">") == 0 || strcmp(stringToCheck, "<") == 0 || strcmp(stringToCheck, "|") == 0) {
        return 1;
    }
    return 0;
}

/**
 * This method handles the case where a command containes multiple pipe directives (denoted by |).
 */
void handleMultiplePipes(void) {
    
    // pipeIndex keeps track of which command we should be running.
    int pipeIndex = 0;
    
    // i is used as an iteration index to close pipes.
    int i = 0;
    
    while(pipeIndex < numPipes) {
        int pipeProc;
        // Pipe the two ends of the pipe required for this process. We only need to pipe 2 per process because the other pipes will be passed down by the fork() below each time it happens.
        pipe(pipeFileDescriptor + (pipeIndex * 2));
        
        // Get rid of any garbage in stdout/stderr that might ruin things.
        fflush(stdout);
        fflush(stderr);
        
        pipeProc = fork();
        
        if(pipeProc == 0) {
            // In child, we just continue, ensuring that the CHILD runs the next command.
            pipeIndex++;
        } else {
            int pipeCloserIndex = 0;
            
            // Since this method works backwards, all but the very first command (handled by the last child OUTSIDE the loop) must have the output of the previous pipe hooked up to its input.
            dup2(pipeFileDescriptor[pipeIndex * 2], STDIN_FILENO);
            
            // This would be every command EXCEPT the very LAST command entered by the user. If we're here, we also have to hook up the process's output to the input of the current pipe in use.
            if(pipeIndex != 0) {
                dup2(pipeFileDescriptor[(2 * (pipeIndex - 1)) + 1], STDOUT_FILENO);
            }
            
            // After 'hooking up' the pipes, we can close all of the input and output ends of all pipes in THIS PROCESS's memory space. This function does not close all the pipes, only the ones passed to it after the fork().
            for( ; pipeCloserIndex < 2 * (pipeIndex + 1) ; pipeCloserIndex++) {
                close(pipeFileDescriptor[pipeCloserIndex]);
            }
            
            // Run the command at index numPipes - pipeIndex (i.e. work backwards).
            if(execvp(newargv[pipeArgsIndexes[numPipes - pipeIndex]], newargv + pipeArgsIndexes[numPipes - pipeIndex]) < 0) {
                fprintf(stderr, "error: Unable to start program \'%s\': ", newargv[0]);
                switch (errno) {
                    case EACCES:
                        fprintf(stderr, "permission denied.\n");
                        break;
                    case ENOENT:
                        fprintf(stderr, "file not found.\n");
                        break;
                    default:
                        break;
                }
            }
        }
    }
    
    // We reach this section at the first command on the line (it's the last thing to run).
    // Since STDIN comes from stdin or wherever, it does not need to be dup()'ed.
    // STDOUT gets dup()'ed to the input end of the very last pipe.
    dup2(pipeFileDescriptor[(numPipes * 2) - 1], STDOUT_FILENO);
    
    // This closes all pipes used by the command.
    for( ; i < numPipes * 2 ; i++) {
        close(pipeFileDescriptor[i]);
    }
    
    // Run the first command, let's get this show rolling!
    if(execvp(newargv[0], newargv) < 0) {
        fprintf(stderr, "error: Unable to start program \'%s\': ", newargv[0]);
        switch (errno) {
            case EACCES:
                fprintf(stderr, "permission denied.\n");
                break;
            case ENOENT:
                fprintf(stderr, "file not found.\n");
                break;
            default:
                break;
        }
    }
}

void handleSignal(int signal) {
}
