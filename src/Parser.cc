/***************************************************************************************[Parser.cc]
Copyright (c) 2022, Jelle Pleunes

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
**************************************************************************************************/

#include <iostream>
#include <fstream>
#include <regex>
#include <cassert>

#include "Parser.h"

using namespace RcpsptExact;

static void tokenize(const string& str, vector<string>& out) {
    out.clear();
    int i = 0;
    int n = (int)str.size();
    while (i < n) {
        while ((str[i] == ' ' || str[i] == '\n' || str[i] == '\r') && i < n) i++;
        std::string token;
        while (str[i] != ' ' && str[i] != '\n' && str[i] != '\r' && i < n) token.push_back(str[i++]);
        if (!token.empty()) out.push_back(token);
    }
}

Problem Parser::parseProblemInstance(ifstream& input) {
    string line;
    vector<string> tokens;

    int section = 0; // File sections are separated by a line of stars ('*')
    int njobs = -1, horizon = -1, nresources = -1;

    while(section <= 2 && getline(input, line)) {
        if (line.empty()) continue;
        if (line[0] == '*') {
            section++;
            continue;
        }

        if (section == 1) continue; // Section 1 does not contain relevant data

        tokenize(line, tokens);
        if (section == 2) {
            if (tokens.front() == "jobs") njobs = std::stoi(tokens.back());
            else if (tokens.front() == "horizon") horizon = std::stoi(tokens.back());
            else if (tokens.back() == "R") nresources = std::stoi(tokens[tokens.size() - 2]);
            continue;
        }
    }

    assert((njobs > -1) && "njobs was not successfully parsed");
    assert((horizon > -1) && "horizon was not successfully parsed");
    assert((nresources > -1) && "nresources was not successfully parsed");
    Problem result(njobs, horizon, nresources);

    int currJob = -1, currResource = 0; // Variables used for parsing related consecutive lines
    while (getline(input, line)) {
        if (line.empty()) continue;
        if (line[0] == '*') {
            section++;
            continue;
        }

        if (section == 3) continue; // Section "PROJECT INFORMATION" does not contain relevant data

        tokenize(line, tokens);
        if (section == 4) { // Section "PRECEDENCE RELATIONS"
            if (tokens.front() == "PRECEDENCE" || tokens.front() == "jobnr.") continue;
            int job = std::stoi(tokens.front()) - 1; // Subtract 1 for zero-indexed array indexing
            int nsucc = std::stoi(tokens[2]);
            result.successors[job].reserve(nsucc);
            for (int i = 0; i < nsucc; i++) {
                int successor = std::stoi(tokens[3 + i]) - 1;
                result.successors[job].push_back(successor);
                result.predecessors[successor].push_back(job);
            }
        }
        else if (section == 5) { // Section "REQUESTS/DURATIONS"
            if (tokens.front() == "REQUESTS/DURATIONS:") continue;
            if (tokens.front()[0] == '-') continue;
            if (tokens.front() == "jobnr.") continue;
            if (currResource == 0 && tokens.size() <= 3) { // This is a dummy job
                currJob = std::stoi(tokens.front()) - 1;
                result.durations.push_back(0);
                for (int i = 0; i < nresources; i++) result.requests[currJob][i].reserve(0);
                continue;
            }
            if (currResource == 0) { // First line for a job
                currJob = std::stoi(tokens.front()) - 1;
                int duration = std::stoi(tokens[2]);
                result.durations.push_back(duration);
                result.requests[currJob][currResource].reserve(duration);
                for (int i = 0; i < duration; i++)
                    result.requests[currJob][currResource].push_back(std::stoi(tokens[3 + i]));
            }
            else { // Remaining lines for a job
                result.requests[currJob][currResource].reserve(result.durations[currJob]);
                for (int i = 0; i < result.durations[currJob]; i++)
                    result.requests[currJob][currResource].push_back(std::stoi(tokens[i]));
            }
            currResource = (currResource + 1) % nresources;
        }
        else if (section == 6) { // Section "RESOURCEAVAILABILITIES"
            if ((int)tokens.size() <= 2 * nresources) continue;
            for (std::string& token : tokens)
                result.capacities[currResource].push_back(std::stoi(token));
            currResource = (currResource + 1) % nresources;
        }
    }

    return result;
}
