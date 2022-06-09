/*********************************************************************************[YicesEncoder.cc]
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
#include <queue>

#include "YicesEncoder.h"

using namespace RcpsptExact;

YicesEncoder::YicesEncoder(Problem &p, pair<int, int> bounds, Measurements* m)
        : Encoder(p, bounds) {
    measurements = m;
}

YicesEncoder::~YicesEncoder() = default;

bool checkValid(const Problem& problem, const vector<int>& solution) {
    if (solution.empty()) return false;

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

void YicesEncoder::printResults() const {
    std::cout << measurements->file << ", ";
    std::cout << measurements->enc_n_boolv << ", ";
    std::cout << measurements->enc_n_intv << ", ";
    std::cout << measurements->enc_n_clause << ", ";
    std::cout << measurements->t_enc << ", ";
    std::cout << measurements->t_search << ", ";
    std::cout << (long)(clock() * 1000 / CLOCKS_PER_SEC) << ", ";
    if (measurements->schedule.empty()) std::cout << -1 << ", ";
    else std::cout << measurements->schedule.back() << ", ";
    std::cout << checkValid(problem, measurements->schedule) << ", ";
    std::cout << measurements->certified << ", ";
    for (int start : measurements->schedule) std::cout << start << ".";
    std::cout << std::endl;
}
