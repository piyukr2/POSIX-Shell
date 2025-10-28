#ifndef IO_H
#define IO_H

#include <vector>
#include <string>

// Execute a single command with optional redirection (<, >, >>)
void execute_with_redirection(std::vector<char *> args,
                              char *inputFile,
                              char *outputFile,
                              bool append);

// Execute a pipeline of commands, with optional < input and > / >> output
void execute_pipeline(std::vector<std::vector<char *>> commands,
                      char *outputFile,
                      bool append,
                      char *inputFile);

// function to decide if a command line involves redirection/pipes
bool try_redirection_or_pipeline(const std::string &input_line);

#endif
