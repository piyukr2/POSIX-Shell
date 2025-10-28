#ifndef PARSER_H
#define PARSER_H
#include <vector>
#include <string>

using namespace std;

vector<string> splitByDelimiter(const string &input, char delimiter);

vector<string> tokenize(const string &command);

void parse_pipeline(const std::string &command,
                    std::vector<std::vector<char *>> &commands,
                    char *&outputFile,
                    bool &appendMode,
                    char *&inputFile);

#endif
