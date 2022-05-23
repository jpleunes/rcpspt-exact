/*****************************************************************************************[Main.cc]
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

#include <csignal>
#include <iostream>
#include <fstream>

#include "Problem.h"
#include "Parser.h"
#include "encoders/SmtEncoder.h"
#include "encoders/SatEncoder.h"
#include "utils/HeuristicSolver.h"

using namespace RcpsptExact;

bool checkValid(const Problem& problem, const vector<int>& solution) {
    // Initialize remaining resource availabilities
    int** available = new int*[problem.nresources];
    for (int k = 0; k < problem.nresources; k++) {
        available[k] = new int[problem.horizon];
        for (int t = 0; t < problem.horizon; t++)
            available[k][t] = problem.capacities[k][t];
    }

    for (int job = 0; job < problem.njobs; job++) {
        // Precedence constraints
        for (int predecessor: problem.predecessors[job]) {
            if (solution[job] < solution[predecessor] + problem.durations[predecessor]) {
                std::cout << "invalid precedence!" << std::endl;
                return false;
            }
        }
    }

    for (int job = 0; job < problem.njobs; job++) {
        // Resource constraints
        for (int k = 0; k < problem.nresources; k++) {
            for (int t = 0; t < problem.durations[job]; t++) {
                int curr = solution[job] + t;
                available[k][curr] -= problem.requests[job][k][t];
                if (available[k][curr] < 0) {
                    std::cout << "resource demand exceeds availability at t=" << curr << '!' << std::endl;
                    return false;
                }
            }
        }
    }

    for (int i = 0; i < problem.nresources; i++) delete[] available[i];
    delete[] available;

    return true;
}

YicesEncoder* enc;

void signal_handler(int signal_num) {
    if (enc == nullptr) exit(1);
//    std::cout << "received signal " << signal_num << std::endl;
    smt_status_t status = yices_context_status(enc->ctx);
    if (status == STATUS_SEARCHING) yices_stop_search(enc->ctx);
    else {
        enc->printResults();
        exit(1);
    }
}

int main(int argc, char** argv) {
    // register termination signal
    signal(SIGTERM, signal_handler);
    signal(SIGKILL, signal_handler);
    signal(SIGINT, signal_handler);
    signal(SIGABRT, signal_handler);

    if (argc < 3) {
        std::cout << "Please provide the following arguments: encoder[smt/sat] input[path_to_file]" << std::endl;
        return 1;
    }

    std::ifstream inpFile(argv[2]);
    Problem problem = Parser::parseProblemInstance(inpFile);
    inpFile.close();

    pair<int,int> bounds = calcBoundsPriorityRule(problem);
    if ("smt" == string(argv[1])) enc = new SmtEncoder(problem, bounds);
    else if ("sat" == string(argv[1])) enc = new SatEncoder(problem, bounds);
    else {
        std::cout << "Argument encoder[smt/sat] not recognised" << std::endl;
        return 1;
    }
    enc->encode();
    enc->optimise();
    enc->printResults();

    delete enc;
    return 0;
}
