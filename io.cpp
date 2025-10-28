#include "io.h"
#include "parser.h" // to use parse_pipeline
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <iostream>
#include <cstdlib>

using namespace std;

void execute_pipeline(vector<vector<char *>> commands,
                      char *outputFile,
                      char *inputFile,
                      bool append);

// This function has been used in main.cpp.
// It parses the command, and decides :
// whether to call execute_with_redirection() or execute_pipeline().
// If the command contains redirection or pipeline, then it executes them accordingly and returns true.
// If no redirection, pipeline is found in the command, it returns false.
bool try_redirection_or_pipeline(const string &input_line)
{
    vector<vector<char *>> cmds;
    char *outFile; // output file if > or >> specified
    char *inFile;  // input file if < specified
    bool append;   // it is a flag, it will be true if >> is present in the command, false if > this present

    // Break the command into argv-like chunks and detect <, >, >>
    parse_pipeline(input_line, cmds, outFile, append, inFile); // in parser.cpp

    if (cmds.empty())
        return false;

    if (cmds.size() > 1) // If it contains multiple commands, then it needs to be executed through pipeline
    {
        execute_pipeline(cmds, outFile, inFile, append);
        return true;
    }
    else if (outFile != nullptr || inFile != nullptr) // If it contains a single command with Redirection only
    {
        execute_with_redirection(cmds[0], inFile, outFile, append);
        return true;
    }
    else // this command contains neither redirection not pipes,
    {    // so it should be processed normally like a single command through main.cpp->builtins.cpp
        return false;
    }
}

/*
MAIN IDEA:
A pipe in Linux is a unidirectional communication channel between processes.
It connects the output (stdout) of one process to the input (stdin) of another.
Data flows in one direction only (write → read).
It is implemented by the kernel using a temporary buffer in memory.

How a Pipe Works:
When you call pipe(pipefd), the kernel creates two file descriptors:
pipefd[0] → the read fd
pipefd[1] → the write fd
Data written to pipefd[1] is stored in the kernel’s pipe buffer.
Data can then be read from pipefd[0] by another process.
*/
// This function executes statements containing multiple commands which are serially connected by pipes.
// Example: "cat file.txt | grep hello > out.txt"
// Each command runs in its own process, with stdout of one
// connected to stdin of the next by using pipe structure.
// It handles optional input redirection on the first command,
// and output redirection on the last command.
// Input parameters:
// -commands: its a (vector of (vector of strings)),
//            where each vector in it contains the tokens of every command in the given pipelined statement
// -input file
// -output file
// -append flag
void execute_pipeline(vector<vector<char *>> commands,
                      char *outputFile,
                      char *inputFile,
                      bool append)
{
    int num_cmds = commands.size(); // total number of commands in pipelined statement
    int in_fd = STDIN_FILENO;       // in this we will store the file descriptor for the current input file
                                    // Here we are setting its initial value = STDIN (i.e. 0)
                                    // If some command has piped input given to it, then we need to read from
                                    // that file's output, not STDIN.
                                    // Then, we will change the value of this file descriptor to
                                    // the output file of the previous command in the pipeline.

    // If input redirection exists for the first command,
    // then open file and set as stdin for first command
    if (inputFile != nullptr)
    {
        in_fd = open(inputFile, O_RDONLY);
        if (in_fd < 0)
        {
            perror("Error occurred in opening the input file.");
            return;
        }
    }
    int i;

    // Iterate over all the commands in pipeline
    for (i = 0; i < num_cmds; i++)
    {
        int pipefd[2]; // creating an array to store the current pipe's input file descriptor & pipe's output file descriptor
        if (i < num_cmds - 1)
        {
            if (pipe(pipefd) < 0) // we will create a pipe for all commands except the last command in the pipeline
            {
                perror("error in creating a pipe");
                return;
            }
        }

        pid_t pid = fork(); // fork a process for this command
        if (pid == 0)       // CHILD PROCESS
        {
            // Redirect stdin from previous input (file or pipe) to current input source
            if (in_fd != STDIN_FILENO)
            {
                dup2(in_fd, STDIN_FILENO);
                close(in_fd);
            }

            // Redirect stdout to pipe OR file as given by the user in this command
            if (i < num_cmds - 1)
            {
                // If it is not the last command, then STDOUT goes to pipe's WRITE end
                dup2(pipefd[1], STDOUT_FILENO);
                close(pipefd[0]); // close pipe's input end
                close(pipefd[1]); // now there are 2 file descriptors pointing to STDOUT, so we close one of them
                // We can't close STDOUT because all the system functions have been defined to write their output to STDOUT
            }
            else
            {
                // If it is the last command, then output redirection is optional i.e. the user may/may not give it
                if (outputFile != nullptr) // If there is output redirection
                {
                    int fd_out;
                    if (append)                                                         // if ">>" was given in the command
                        fd_out = open(outputFile, O_WRONLY | O_CREAT | O_APPEND, 0644); // opening the file with APPEND flag
                    else                                                                // if ">" was given in the command
                        fd_out = open(outputFile, O_WRONLY | O_CREAT | O_TRUNC, 0644);  // opening the file without APPEND, so a new file will be created
                    if (fd_out < 0)
                    {
                        perror("error in opening the output file");
                        exit(1);
                    }
                    dup2(fd_out, STDOUT_FILENO); // Now, whatever will be meant to be written to STDOUT, will be written to the pipe's output
                    close(fd_out);               // as of now, STDOUT has 2 file descriptors so close one of them
                    // We can't close STDOUT because all the system functions have been defined to write their output to STDOUT
                }
            }

            // Execute command
            if (execvp(commands[i][0], commands[i].data()) < 0)
            {
                perror("Error in executing execvp()");
                exit(1);
            }
        } // child process end
        else // PARENT PROCESS
        {
            if (i < num_cmds - 1)
                close(pipefd[1]); // parent doesn't write to pipe, so close it's file descriptor for writing
            if (in_fd != STDIN_FILENO)
                close(in_fd); // close old input file descriptor
            if (i < num_cmds - 1)
                in_fd = pipefd[0]; // next command will read from pipe

            waitpid(pid, NULL, 0); // wait for child process to finish
        }
    }

    // Free memory allocated by strdup() in parser.cpp
    for (auto &cmd : commands)
    {
        for (auto arg : cmd)
        {
            if (arg != nullptr)
                free(arg);
        }
    }
}

// ------------------- execute_with_redirection() -------------------
// This function executes a single command with optional redirection (<, >, >>).
// Example: "sort < in.txt > out.txt"
// Steps:
// 1. Fork a child process.
// 2. In the child, if < was specified, dup2() the input file to stdin.
// 3. If > or >> was specified, dup2() the output file to stdout.
// 4. Call execvp() to execute the command.
// 5. Parent waits for child to finish.
void execute_with_redirection(vector<char *> args,
                              char *inputFile,
                              char *outputFile,
                              bool append)
{
    pid_t pid = fork(); // create a child process

    if (pid == 0) // CHILD PROCESS
    {
        // If input redirection exists ("< file")
        if (inputFile != nullptr)
        {
            int fd_in = open(inputFile, O_RDONLY); // open file for reading
            if (fd_in < 0)
            {
                perror("open input");
                exit(1);
            }
            dup2(fd_in, STDIN_FILENO); // replace stdin (fd=0) with file
            close(fd_in);              // close original fd since it's duplicated
        }

        // If output redirection exists (> or >>)
        if (outputFile != nullptr)
        {
            int fd_out;
            if (append)                                                         // If ">>" is presnt
                fd_out = open(outputFile, O_WRONLY | O_CREAT | O_APPEND, 0644); // opening the file in append mode
            // so the new contents will only get appended to the old contents of thw input file
            else                                                               // If ">" is presnt
                fd_out = open(outputFile, O_WRONLY | O_CREAT | O_TRUNC, 0644); // opening the file in append mode,
            // so all old contents will get odeleted and the file will be created afresh
            if (fd_out < 0)
            {
                perror("error in writing to output file");
                exit(1);
            }
            dup2(fd_out, STDOUT_FILENO); // replace stdout (fd=1) with file
            close(fd_out);               // now 2 descriptors point to this output file: STDOUT  & fd_out. So we can close fd_out.
            // We can't close STDOUT because all the system functions have been defined to write their output to STDOUT
        }

        // Run the command
        if (execvp(args[0], args.data()) < 0)
        {
            perror("execvp");
            exit(1);
        }
    }
    else // PARENT PROCESS
    {

        waitpid(pid, NULL, 0); // wait for child to finish
    }

    // Free memory allocated by strdup() in parser.cpp
    for (auto arg : args)
    {
        if (arg != nullptr)
            free(arg);
    }
}
