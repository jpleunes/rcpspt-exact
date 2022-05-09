/************************************************************************************[SmtEncoder.h]
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

#ifndef RCPSPT_EXACT_SMTENCODER_H
#define RCPSPT_EXACT_SMTENCODER_H

#include <vector>

#include "../Problem.h"

using namespace std;

namespace RcpsptExact {

/**
 * Class for encoding an instance of Problem into SMT.
 */
class SmtEncoder {
public:
    // Constructor
    SmtEncoder(Problem& p)
            : problem(p),
              UB(problem.horizon) {
        preprocess();
    }
    // Destructor
    ~SmtEncoder() = default;

    /**
     * TODO
     */
    void encode();

private:
    Problem& problem;

    vector<vector<int>> Estar; // List of successors for each activity, in the extended precedence graph
    vector<vector<int>> l;     // Time lags for all pairs of activities

    int UB; // The upper bound for the makespan that is currently being used TODO: calculate better UB using PSGS and afterwards update LS and LC

    vector<int> ES, EC, LS, LC; // For each activity: earliest start, earliest close, latest start, and latest close time

    void floydWarshall();

    /**
     * Perform preprocessing to reduce the amount of variables in the final encoding.
     */
    void preprocess();
};
}

#endif //RCPSPT_EXACT_SMTENCODER_H
