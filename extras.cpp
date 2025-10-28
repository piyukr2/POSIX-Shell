#include "extras.h"
#include <iostream>    // for input/output (cout, cin)
#include <fstream>     // for file handling (ifstream, ofstream)
#include <string>      // for using string class
#include <vector>      // for vector container
#include <dirent.h>    // for directory operations (opendir, readdir)
#include <unistd.h>    // for system calls like getpid(), readlink()
#include <sys/types.h> // for pid_t type
#include <sys/stat.h>  // for stat() function
#include <limits.h>    // for PATH_MAX (max length of a path)
#include <cstring>     // for C string functions like strcmp, strcpy
#include <signal.h>    // for signal handling
#include <sstream>     // for stringstream
using namespace std;

extern pid_t foregroundPid; // global variable to track current foreground process
extern string shellHome;    // global variable to store shell's home directory

void pinfo(pid_t pid)
{
    if (pid == 0) // If no PID specified, use current shell's PID
        pid = getpid();

    // Read process status from /proc/<pid>/stat for more comprehensive information
    string statPath = "/proc/" + to_string(pid) + "/stat";
    ifstream statFile(statPath);

    if (!statFile)
    {
        cout << "Process with PID " << pid << " does not exist" << endl;
        return;
    }

    // Parse /proc/pid/stat file for process state
    string line;
    getline(statFile, line);
    statFile.close();

    // Extract state (3rd field in /proc/pid/stat)
    istringstream iss(line);
    string field;
    char state = 'U'; // Unknown default
    for (int i = 0; i < 3 && iss >> field; i++)
    {
        if (i == 2)
        { // State is 3rd field (0-indexed: 0,1,2)
            state = field[0];
        }
    }

    // Check if process is in foreground (add + to status)
    string status;
    switch (state)
    {
    case 'R':
        status = (pid == foregroundPid) ? "R+" : "R";
        break;
    case 'S':
        status = (pid == foregroundPid) ? "S+" : "S";
        break;
    case 'D':
        status = "D";
        break; // Uninterruptible sleep
    case 'Z':
        status = "Z";
        break; // Zombie
    case 'T':
        status = "T";
        break; // Stopped
    default:
        status = "U";
        break; // Unknown
    }

    // Read virtual memory from /proc/<pid>/status
    string statusPath = "/proc/" + to_string(pid) + "/status";
    ifstream statusFile(statusPath);
    string vmSize = "0";

    if (statusFile)
    {
        string line;
        while (getline(statusFile, line))
        {
            if (line.find("VmSize:") == 0)
            {
                // Extract the memory value (remove "VmSize:" and "kB")
                size_t start = line.find_first_of("0123456789");
                if (start != string::npos)
                {
                    size_t end = line.find_first_not_of("0123456789", start);
                    vmSize = line.substr(start, end - start);
                }
                break;
            }
        }
        statusFile.close();
    }

    // Get executable path from /proc/<pid>/exe
    string exeLink = "/proc/" + to_string(pid) + "/exe";
    char exePath[PATH_MAX];
    ssize_t len = readlink(exeLink.c_str(), exePath, sizeof(exePath) - 1);

    string executablePath;
    if (len != -1)
    {
        exePath[len] = '\0';
        executablePath = string(exePath);
    }
    else
    {
        executablePath = "N/A";
    }

    cout << "pid -- " << pid << endl;
    cout << "Process Status -- " << status << endl;
    cout << "memory -- " << vmSize << " {Virtual Memory}" << endl;
    cout << "Executable Path -- " << executablePath << endl;
}

// This function checks if a file/folder exists in current directory or subdirectories.
bool searchFile(string basePath, string target)
{
    DIR *dir = opendir(basePath.c_str());
    if (!dir)
        return false;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        string name = entry->d_name;

        // skip "." and ".." entries
        if (name == "." || name == "..")
            continue;

        // if the name matches the target, we found it
        if (name == target)
        {
            closedir(dir);
            return true;
        }

        // prepare full path of this entry
        string newPath = basePath + "/" + name;

        // check if this entry is itself a directory
        struct stat st;
        if (stat(newPath.c_str(), &st) == 0 && S_ISDIR(st.st_mode))
        {
            // recursive call to search inside this folder
            if (searchFile(newPath, target))
            {
                closedir(dir);
                return true;
            }
        }
    }
    closedir(dir);
    return false;
}

//  send signals to foreground processes when they exist

// Handler for Ctrl+C (SIGINT)
void handle_sigint(int sig)
{
    (void)sig; // silence unused parameter warning

    //  If there's a foreground process, send SIGINT to it
    if (foregroundPid != -1)
    {
        cout << endl; // New line for clean output
        kill(foregroundPid, SIGINT);
        cout << "Process " << foregroundPid << " interrupted by SIGINT" << endl;
    }
    else
    {
        // No foreground process, just show prompt again
        cout << endl;
    }
}

// Handler for Ctrl+Z (SIGTSTP)
void handle_sigtstp(int sig)
{
    (void)sig; // silence unused parameter warning

    // If there's a foreground process, send SIGTSTP to it
    if (foregroundPid != -1)
    {
        cout << endl; // New line for clean output
        kill(foregroundPid, SIGTSTP);
        cout << "[1]+ Stopped    (Process " << foregroundPid << ")" << endl;
    }
    else
    {
        // No foreground process, just show prompt again
        cout << endl;
    }
}

// History file path - using environment HOME variable
string getHistoryFilePath()
{
    const char *home = getenv("HOME");
    if (home)
    {
        return shellHome + "/.my_shell_history";
    }
    return ".my_shell_history"; // fallback to current directory
}

// Load all history commands into vector
vector<string> loadHistory()
{
    vector<string> hist;
    string historyFile = getHistoryFilePath();
    ifstream fin(historyFile);
    string cmd;
    while (getline(fin, cmd))
    {
        if (!cmd.empty())
            hist.push_back(cmd);
    }
    fin.close();
    return hist;
}

// Save vector of history back into file (keeping only last 20)
void saveHistory(vector<string> hist)
{
    string historyFile = getHistoryFilePath();
    ofstream fout(historyFile, ios::trunc);
    int start = 0;
    if ((int)hist.size() > 20)
        start = hist.size() - 20; // only keep last 20
    for (int i = start; i < (int)hist.size(); i++)
    {
        fout << hist[i] << endl;
    }
    fout.close();
}

// Add a new command to history
void addHistory(string cmd)
{
    vector<string> hist = loadHistory(); // load existing history from file

    // If 20 commands are already stored, remove the oldest
    if ((int)hist.size() >= 20)
    {
        hist.erase(hist.begin());
    }

    hist.push_back(cmd); // add the latest command

    saveHistory(hist); // write exactly 20 or lesser commands back to file
}

// Show history (ny default, show only the last 10 commands)
void showHistory(int n)
{
    vector<string> hist = loadHistory();
    int start = hist.size() - n;
    if (start < 0)
        start = 0;
    for (int i = start; i < (int)hist.size(); i++)
    {
        cout << hist[i] << endl;
    }
}