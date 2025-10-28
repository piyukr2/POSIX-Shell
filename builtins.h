#ifndef BUILTINS_H
#define BUILTINS_H
#include <string>
#include <vector>



// Handles built-in shell commands (cd, pwd, echo, ls, pinfo, search, history, exit)
// Returns true if the command was a built-in command (handled), false otherwise
bool handleBuiltinCommands(const std::vector<std::string> &args);

// Executes system commands (non built-in commands) in foreground or background
// Parameters: 
//   args - vector of command arguments (args[0] is the command name)
//   background - true if command should run in background (& operator used)
void run_system_command(std::vector<std::string> args, bool background);

#endif