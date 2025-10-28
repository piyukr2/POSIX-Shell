/*
   extras.h
   Header file for additional shell functions: pinfo, search, signals, history.
*/

#ifndef EXTRAS_H
#define EXTRAS_H

#include <string>
#include <vector>
#include <sys/types.h> // for pid_t

using namespace std;

// Function for pinfo command - shows process information
void pinfo(pid_t pid);

// Function for search command - recursively searches for files/directories 
bool searchFile(std::string basePath, std::string target);

// Signal handler functions for Ctrl+C and Ctrl+Z
void handle_sigint(int sig);   // Handle SIGINT (Ctrl+C)
void handle_sigtstp(int sig);  // Handle SIGTSTP (Ctrl+Z) 

// History related functions
vector<string> loadHistory();          // Load command history from file
void saveHistory(vector<string> hist); // Save command history to file  
void addHistory(string cmd);           // Add new command to history
void showHistory(int n = 10);          // Display last n commands (default 10)

#endif