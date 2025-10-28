#ifndef RL_SHELL_H
#define RL_SHELL_H

#include <string>

using namespace std;

// Returns false on Ctrl+D (EOF at empty prompt) so caller can "logout".
// On success, 'out' gets the typed line (no trailing newline) and function returns true.
bool rl_readline(string &out, const string &prompt);

#endif
