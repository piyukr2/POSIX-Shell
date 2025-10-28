/*
parser.cpp: breaks the input into separate commands & detects <, >, >>, |.
*/

#include "parser.h"
#include <sstream>
#include <string.h>
#include <iostream>

using namespace std;

vector<string> splitByDelimiter(const string &input, char delimiter)
{
    vector<string> result;
    stringstream ss(input);
    string token;
    while (getline(ss, token, delimiter))
    {
        if (!token.empty())
            result.push_back(token);
    }
    return result;
}

vector<string> tokenize(const string &command)
{
    stringstream ss(command);
    string arg;
    vector<string> tokens;
    while (ss >> arg)
        tokens.push_back(arg);
    return tokens;
}

// This function takes the full command line string and breaks it down into:
//   - A list of commands (each command is argv-style vector<char*>)
//   - Input redirection file (if '<' found)
//   - Output redirection file (if '>' or '>>' found)
//   - A flag 'appendMode' (which is true if '>>' is found)
//
// Example:
//   Input: "cat in.txt | grep IIIT > out.txt"
//   Output:
//     commands = [ {"cat","in.txt"}, {"grep","IIIT"} ]
//     inputFile = nullptr
//     outputFile = "out.txt"
//     appendMode = false
void parse_pipeline(const string &command,
                    vector<vector<char *>> &commands,
                    char *&outputFile,
                    bool &appendMode,
                    char *&inputFile)
{
    // Step 1: split the command string by '|'
    vector<string> parts = splitByDelimiter(command, '|');

    // Initial values
    outputFile = nullptr; // this filename's value will be updated if the command contains ">"
    inputFile = nullptr;  // this filename's value will be updated if the command contains "<"
    appendMode = false;   // this flag will be updated if the command contains ">>"

    // Step 2: process each command present in the pipelined statement
    for (size_t i = 0; i < parts.size(); i++)
    {
        stringstream ss(parts[i]);
        string arg;
        vector<char *> args;

        // Step 3: parse tokens inside each segment
        while (ss >> arg)
        {
            if (arg == ">" || arg == ">>") // both are for output redirection case
            {
                
                appendMode = (arg == ">>"); // true if the command contains ">>"
                if (!(ss >> arg))
                {
                    cerr << "Error: missing filename after > or >>\n";
                    return;
                }
                outputFile = strdup(arg.c_str()); // store filename
                continue; // don't add to args for execvp
            }
            if (arg == "<") // this is for input redirection case
            {
                
                if (!(ss >> arg))
                {
                    cerr << "Error: missing filename after <\n";
                    return;
                }
                inputFile = strdup(arg.c_str()); // store filename
                continue; // don't add to args for execvp
            }
            else // some other normal argument, so push it into argv
            { 
                args.push_back(strdup(arg.c_str()));
            }
        }
        // after adding all the arguments to args vector, add NULL at the end
        args.push_back(nullptr); // we do this because execvp() expects argv to end with NULL

        // and add the above args vector to "commands" vector
        commands.push_back(args);
    }
}