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
#include "yices.h"

using namespace std;

namespace RcpsptExact {

/**
 * Class for encoding an instance of Problem into SMT.
 */
class SmtEncoder {
public:
    // Constructor
    SmtEncoder(Problem& p, pair<int,int> bounds)
            : problem(p),
              LB(bounds.first),
              UB(bounds.second),
              ES(p.njobs),
              EC(p.njobs),
              LS(p.njobs),
              LC(p.njobs) {
        preprocess();
        initialise();
    }
    // Destructor
    ~SmtEncoder() {
        yices_free_context(ctx);
        yices_exit();
    }

    /**
     * Encodes the problem instance into SMT and stores the result in the 'formula' field.
     */
    void encode();

    /**
     * Calls Yices to solve the feasibility problem with the current encoding.
     *
     * @param out vector with the start time for each activity, will be empty if problem is unsatisfiable
     */
    void solve(vector<int>& out);

    /**
     * Finds the optimal solution by calling Yices repeatedly.
     * Starts with the given lower and upper bounds, and incrementally decreases the upper bound.
     *
     * @return vector with the start time for each activity, or an empty vector if the problem is infeasible
     */
    vector<int> optimise();

private:
    Problem& problem;

    vector<vector<int>> Estar; // List of successors for each activity, in the extended precedence graph
    vector<vector<int>> l;     // Time lags for all pairs of activities

    int LB, UB; // The lower and upper bounds for the makespan that are currently being used

    vector<int> ES, EC, LS, LC; // For each activity: earliest start, earliest close, latest start, and latest close time

    vector<term_t> S; // Variable S_i: start time of activity i
    vector<vector<term_t>> y; // Variable y_(i,t): boolean representing whether activity i starts at time t in STW(i)

    context_t* ctx; // Yices context

    term_t formula; // Formula that will be used when calling solve()

    void floydWarshall();

    /**
     * Perform preprocessing to reduce the amount of variables in the final encoding.
     */
    void preprocess();

    /**
     * Initialises Yices solver, and creates (integer and Boolean) variables representing start times of activities.
     */
    void initialise();
};
}

#endif //RCPSPT_EXACT_SMTENCODER_H
