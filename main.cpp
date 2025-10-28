#include <iostream>
#include <unistd.h>
#include <limits.h>
#include <vector>
#include <csignal> // for signal handling
#include <readline/readline.h>
#include <pwd.h>    // for getpwuid() to get username
#include <sys/utsname.h> // for uname() to get system name
#include "parser.h"
#include "builtins.h"
#include "io.h"
#include "extras.h"
#include "readline_shell.h"

using namespace std;

string shellHome; // global string variable to store the initial home path where this program started
string userName;  // global string to store username 
string systemName; // global string to store system name
pid_t foregroundPid = -1; // global variable to track current foreground process (for signal handling)

// Function to show ~ for home directory
string getDisplayPath(const string& currentPath) {
    if (currentPath == shellHome) {
        return "~";
    } else if (currentPath.find(shellHome) == 0 && currentPath.length() > shellHome.length()) {
        // If current path starts with home directory, replace with ~
        return "~" + currentPath.substr(shellHome.length());
    }
    return currentPath;
}

int main()
{
    // Set shellHome only once at startup to remember the initial directory 
    char initialDir[PATH_MAX];
    if (getcwd(initialDir, sizeof(initialDir)) == NULL) {
        perror("Error in getting initial directory");
        return 1;
    }
    shellHome = string(initialDir); // Store the initial directory as home
    
    // Get actual username for prompt display
    struct passwd* pw = getpwuid(getuid());
    if (pw != nullptr) {
        userName = string(pw->pw_name);
    } else {
        userName = "unknown";
    }
    
    // Get system name for display at the prompt
    struct utsname unameData;
    if (uname(&unameData) == 0) {
        systemName = string(unameData.nodename);
    } else {
        // If uname fails, then get system name by gethostname()
        char hostName[200];
        if (gethostname(hostName, sizeof(hostName)) == 0) {
            systemName = string(hostName);
        } else {
            systemName = "unknown";
        }
    }

    // Registering the signal handlers
    signal(SIGINT, handle_sigint);   // Ctrl+C handler
    signal(SIGTSTP, handle_sigtstp); // Ctrl+Z handler

    while (true)
    {
        char cwd[PATH_MAX];
        if (getcwd(cwd, sizeof(cwd)) == NULL) {
            perror("Error in getcwd()");
            return 1;
        }
        

        string displayPath = getDisplayPath(string(cwd));
        string prompt = userName + "@" + systemName + ":" + displayPath + "> ";

        
        string input;
        if (!rl_readline(input, prompt)) { // Pass prompt to readline function
            cout << "Exiting the shell.." << endl;
            break; // exit the shell loop on Ctrl+D
        }

        // if the user enters blank string, then dont do anything, just display the prompt again for the next input
        if (input.empty())
            continue;

        // save command into history
        addHistory(input);

        // splitting the set of commands separated by ";" into separate commands
        vector<string> commands = splitByDelimiter(input, ';');

        for (string &cmd : commands)
        {
            // split each command by using the spaces present in the command
            vector<string> args = tokenize(cmd);

            // if the user enters blank string, then dont do anything, just display the prompt again for the next input
            if (args.empty())
                continue;

            if (try_redirection_or_pipeline(cmd)) // written in io.cpp
                continue;

            if (handleBuiltinCommands(args)) // written in builtins.cpp
                continue;

            // If the given command didn't run successfully- neither with redirection/pipelining,
            // nor as a command implemented by us, nor as a system command through execvp(), then show error
            cerr << "Unknown command: " << args[0] << endl;
        }
    }
    return 0;
}
