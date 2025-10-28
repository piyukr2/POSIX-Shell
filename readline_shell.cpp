#include "readline_shell.h"

#include <readline/readline.h>
#include <readline/history.h>

#include <cstring>      // for strlen, strncmp, strdup
#include <cstdlib>      // for getenv
#include <string>
#include <vector>
#include <algorithm>    // for sort, unique
#include <sys/stat.h>   // for stat()
#include <unistd.h>     // for access()
#include <dirent.h>     // for opendir(), readdir(), closedir()

using namespace std;


// list of all the functions we built. We will need to update this if we add/remove builtins.
static vector<string> g_builtins = {
    "cd", "pwd", "echo", "ls", "pinfo", "search", "history", "exit"
};


static string g_hist_file;   // to store the name of the shell history file 
static bool g_inited = false;     // to avoid doing init stuff more than once

// check if file is executable (very basic)
static bool is_executable(const string &path) {
    // access(..., X_OK) is simpler than reading st_mode bits, so using that
    return ::access(path.c_str(), X_OK) == 0;
}

// this function will add PATH executables that start with "prefix" into "out"
static void collect_path_commands(const char *prefix, vector<string> &out) {
    const char *path = getenv("PATH");
    if (!path) return;

    string P(path);
    size_t i = 0;
    size_t preLen = prefix ? strlen(prefix) : 0;

    // split PATH by ':'
    while (i <= P.size()) {
        size_t j = P.find(':', i);
        string dir = (j == string::npos) ? P.substr(i) : P.substr(i, j - i);
        if (dir.empty()) dir = "."; // empty means current dir

        DIR *d = opendir(dir.c_str());
        if (d) {
            dirent *ent;
            while ((ent = readdir(d)) != nullptr) {
                const char *nm = ent->d_name;
                if (preLen && strncmp(nm, prefix, preLen) != 0) continue;
                string full = dir + "/" + nm;
                if (is_executable(full)) {
                    out.push_back(nm); // push only the command name
                }
            }
            closedir(d);
        }

        if (j == string::npos) break;
        i = j + 1;
    }
}


/*
   Readline asks us to provide completions using these two function calls:
   1) my_completion(): decide *how* we want to complete (commands vs files)
   2) my_cmd_generator(): actually generate command matches one-by-one
*/

// this generates command names (builtins + PATH) that start with 'text'
static char *my_cmd_generator(const char *text, int state) {
    
    
    static vector<string> pool;
    static size_t idx;

    if (state == 0) {
        // build the list only on the first call
        pool.clear();
        idx = 0;

        // adding builtin functions
        for (size_t k = 0; k < g_builtins.size(); ++k) {
            const string &b = g_builtins[k];
            if (!text || strncmp(b.c_str(), text, strlen(text)) == 0) {
                pool.push_back(b);
            }
        }

        // add PATH executables
        collect_path_commands(text ? text : "", pool);

        // sort + unique because the same name can appear in multiple PATH dirs
        sort(pool.begin(), pool.end());
        pool.erase(unique(pool.begin(), pool.end()), pool.end());
    }

    if (idx >= pool.size()) {
        return nullptr; // no more matches
    }
    // readline expects us to return a malloc-allocated C-string
    return ::strdup(pool[idx++].c_str());
}

// this function decides whether we are completing command names or filenames
static char **my_completion(const char *text, int start, int end) {
    (void)end; // I am not using 'end' here

    // if we are at the first token (start == 0), complete command names
    if (start == 0) {
        return rl_completion_matches(text, my_cmd_generator);
    }
    // otherwise, let readline do its default thing (filename completion)
    return nullptr;
}

// ----------------------- one-time init -----------------------

static void rl_setup_once() {
    if (g_inited) return;
    g_inited = true;

    // attach our completion function
    rl_attempted_completion_function = my_completion;

    // figure out history file path: $HOME/.my_shell_history
    const char *home = getenv("HOME");
    if (home && *home) g_hist_file = string(home) + "/.my_shell_history";
    else               g_hist_file = ".my_shell_history"; // fallback if HOME missing

    // read previous history (ok if file does not exist yet)
    read_history(g_hist_file.c_str());

    // keep only last 20 commands in memory (assignment limit)
    stifle_history(20);
}


bool rl_readline(string &out, const string &prompt) {
    rl_setup_once();

    char *raw = ::readline(prompt.c_str());
    if (!raw) {
        // NULL means EOF (Ctrl+D) when line is empty
        out.clear();
        return false;
    }

    // convert to string and free the readline buffer
    out.assign(raw);
    ::free(raw);

    // don't add empty/whitespace lines to history
    bool only_ws = true;
    for (size_t i = 0; i < out.size(); ++i) {
        if (out[i] != ' ' && out[i] != '\t') { only_ws = false; break; }
    }

    if (!only_ws && !out.empty()) {
        // avoid adding the exact same last line twice in a row
        if (history_length == 0 || out != history_get(history_length)->line) {
            add_history(out.c_str());
            // append to file (if first time fails, write the whole file)
            if (append_history(1, g_hist_file.c_str()) != 0) {
                write_history(g_hist_file.c_str());
            }
            // also keep the file trimmed to 20 lines, same as memory
            history_truncate_file(g_hist_file.c_str(), 20);
        }
    }

    return true;
}
