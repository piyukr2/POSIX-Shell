#include "builtins.h"
#include "extras.h"
#include <dirent.h>    // for opendir(), readdir(), closedir()
#include <unistd.h>    // for chdir(), fork(), execvp()
#include <sys/stat.h>  // for stat()
#include <sys/types.h> // for various datatypes contained in the struct stat variable returned by the stat() function
#include <iostream>
#include <limits.h> // for PATH_MAX
#include <string.h>
#include <pwd.h>      // for getpwuid
#include <grp.h>      // for getgrgid()
#include <iomanip>    // for setw() function (used to fix the width of the output)
#include <sys/wait.h> // for waitpid()

using namespace std;

extern string shellHome; // global string variable which stores the initial home path
extern pid_t foregroundPid; // global variable to track foreground process for signal handling
static string prevDir; // static variable to remember previous directory for 'cd -'

void run_ls(vector<string> args);

bool handleBuiltinCommands(const vector<string> &args_input)
{
    vector<string> args = args_input;
    if (args.empty() == true)
        return false;

    if (args[0] == "exit")
    {
        cout << "Goodbye!" << endl;
        exit(0); 
    }
    else if (args[0] == "pwd")
    {
        char cwd[PATH_MAX];
        if (getcwd(cwd, sizeof(cwd)) == NULL)
        {
            perror("error in getcwd");
            return true; 
        }
        cout << cwd << endl;
        return true;
    }
    else if (args[0] == "echo")
    {
        for (size_t i = 1; i < args.size(); i++)
            cout << args[i] << " ";
        cout << endl;
        return true;
    }
    else if (args[0] == "ls")
    {
        run_ls(args);
        return true;
    }
    else if (args[0] == "cd")
    {
               
        if (args.size() > 2)
        {
            cerr << "Invalid arguments" << endl;
            return true;
        }
        
        // Get current directory before changing (for previous directory tracking)
        char currentDir[PATH_MAX];
        if (getcwd(currentDir, sizeof(currentDir)) == NULL) {
            perror("error in getting current directory");
            return true;
        }
        
        string target;
        if (args.size() == 1 || args[1] == "~")
        {
            // Use global shellHome variable 
            target = shellHome;
        }
        else if (args[1] == ".")
        {
            // Stay in current directory - no change needed
            return true;
        }
        else if (args[1] == "..")
        {
            // Go to parent directory
            target = "..";
        }
        else if (args[1] == "-")
        {
            // go to previous directory
            if (prevDir.empty())
            {
                cerr << "cd: OLDPWD not set" << endl; // Standard error message
                return true;
            }
            target = prevDir;
            // Print the directory we're switching to
            cout << target << endl;
        }
        else
        {
            target = args[1];
        }
        
        // Update prevDir before changing directory
        // This ensures that we remember which directory we came from
        prevDir = string(currentDir);
        
        if (chdir(target.c_str()) != 0) {
            perror("cd");
            
            // If chdir fails, revert prevDir to avoid inconsistent state
            char revertDir[PATH_MAX];
            if (getcwd(revertDir, sizeof(revertDir)) != NULL) {
                prevDir = string(revertDir);
            }
        }
        return true;
    }
    else if (args[0] == "pinfo")
    {
        if (args.size() == 1)
            pinfo(0); // If no pid has been provided, then show the pid of our shell process itself
        else
            pinfo(stoi(args[1])); // if pid is given
        return true; 
    }
    else if (args[0] == "search")
    {
        if (args.size() < 2) {
            cout << "Usage: search <filename>" << endl;
        } else {
            cout << (searchFile(".", args[1]) ? "True" : "False") << endl;
        }
        return true; 
    }
    else if (args[0] == "history")
    {
        if (args.size() == 1)
            showHistory(); // by default, it will show the last 20 commands
        else
            showHistory(stoi(args[1])); // user specified n
        return true; 
    }
    // else pass to system command handler
    else
    {
        // check if last argument is "&" 
        bool background = false;
        if (args[args.size() - 1] == "&")
        {
            background = true;
            args.pop_back(); // remove "&" before passing to execvp
        }

        run_system_command(args, background);
        return true;
    }
    return false;
}


void run_system_command(vector<string> args, bool background)
{
    // convert vector<string> to char* array (needed for execvp)
    vector<char *> argv;
    for (string &s : args)
    {
        char *arg = strdup(s.c_str()); 
        argv.push_back(arg);
    }
    argv.push_back(NULL); 

    // create a child process
    pid_t pid = fork();

    if (pid < 0) // If fork failed
    {
        perror("fork failed");
        // If the fork() call failed, then clean up allocated memory 
        for (char* arg : argv) {
            if (arg != nullptr) free(arg);
        }
        return;
    }
    else if (pid == 0)
    {
        // this is the child process
        // we will replace child with new program specified by the user
        if (execvp(argv[0], argv.data()) == -1)
        {
            perror("execvp() failed");
            exit(1); // exit child if execvp fails
        }
    }
    else
    {
        // this is the parent process
        if (background == true)
        {
            
            cout << "[1] " << pid << endl; 
        }
        else
        {
            // Track foreground process for signal handling
            foregroundPid = pid;
            
            // start the foreground process and wait until the child process finishes
            int status;
            waitpid(pid, &status, 0);
            
            // Reset foreground PID when process finishes
            foregroundPid = -1;
        }
    }
    
    // Clean up allocated memory
    for (char* arg : argv) {
        if (arg != nullptr) free(arg);
    }
}

void run_ls(vector<string> args)
{
    // Flags for "-a" (show hidden files) and "-l" (long listing)
    bool flag_a = false, flag_l = false;
    vector<string> paths; // store directories or files to list

    // Note: args[0] is "ls", so we start checking from args[1]
    for (size_t i = 1; i < args.size(); i++)
    {
        string arg = args[i];

        // Check for flags
        if (arg == "-a")
            flag_a = true; // show hidden files
        else if (arg == "-l")
            flag_l = true; // show long listing
        else if (arg == "-al" || arg == "-la")
        { // combined flags
            flag_a = true;
            flag_l = true;
        }
        else
        {
            // If it's not a flag, treat it as a directory/file name
            paths.push_back(arg);
        }
    }

    // If no directory/file is given, list current directory (".")
    if (paths.empty())
        paths.push_back(".");

    // Loop over each directory/file provided
    for (auto &path : paths)
    {
        // Handle multiple paths better - show directory name if multiple paths
        if (paths.size() > 1) {
            cout << path << ":" << endl;
        }
        
        // Open the directory
        DIR *dir = opendir(path.c_str());

        if (dir == NULL)
        {
            // If it fails, print error (like real ls does)
            perror(("ls: cannot access '" + path + "'").c_str());
            continue;
        }

        // Read entries of the above opened directory (files/folders) one by one
        struct dirent *directoryEntry;
        while ((directoryEntry = readdir(dir)) != NULL) 
        {
            // Skip hidden files if "-a" is not given
            if ((flag_a == false) && directoryEntry->d_name[0] == '.')
                continue;

            if (flag_l == true)
            {
                // For "-l", we need detailed information about the file
                struct stat st;

                // Full path = directory path + "/" + filename
                string fullpath = path + "/" + directoryEntry->d_name;

                // Get file info using stat()
                if (stat(fullpath.c_str(), &st) == -1)
                {
                    perror("Error in stat() in ls -l");
                    continue;
                }

                // Print file details like real `ls -l`
                // 1. File type and permissions
                cout << (S_ISDIR(st.st_mode) ? 'd' : '-');    // 'd' = directory, '-' = file
                cout << ((st.st_mode & S_IRUSR) ? 'r' : '-'); // owner read
                cout << ((st.st_mode & S_IWUSR) ? 'w' : '-'); // owner write
                cout << ((st.st_mode & S_IXUSR) ? 'x' : '-'); // owner execute
                cout << ((st.st_mode & S_IRGRP) ? 'r' : '-'); // group read
                cout << ((st.st_mode & S_IWGRP) ? 'w' : '-'); // group write
                cout << ((st.st_mode & S_IXGRP) ? 'x' : '-'); // group execute
                cout << ((st.st_mode & S_IROTH) ? 'r' : '-'); // others read
                cout << ((st.st_mode & S_IWOTH) ? 'w' : '-'); // others write
                cout << ((st.st_mode & S_IXOTH) ? 'x' : '-'); // others execute

                // 2. Number of links
                cout << " " << setw(3) << st.st_nlink;

                // 3. Owner and group name
                struct passwd* pwd = getpwuid(st.st_uid);
                struct group* grp = getgrgid(st.st_gid);
                cout << " " << (pwd ? pwd->pw_name : to_string(st.st_uid));
                cout << " " << (grp ? grp->gr_name : to_string(st.st_gid));

                // 4. File size in bytes
                cout << " " << setw(8) << st.st_size;

                // 5. Last modification time
                char timebuf[80];
                strftime(timebuf, sizeof(timebuf), "%b %d %H:%M",
                         localtime(&st.st_mtime));
                cout << " " << timebuf;

                // 6. File name
                cout << " " << directoryEntry->d_name << "\n";
            }
            else
            {
                // If "-l" not used, just print the name
                cout << directoryEntry->d_name << "\n";
            }
        }
        closedir(dir); // close directory after reading
        
        // Add blank line between multiple directories
        if (paths.size() > 1) {
            cout << endl;
        }
    }
}