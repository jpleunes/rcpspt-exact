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

#include "YicesEncoder.h"

using namespace std;

namespace RcpsptExact {

/**
 * Class for encoding an instance of Problem into SMT.
 */
class SmtEncoder : public YicesEncoder {
public:
    // Constructor
    SmtEncoder(Problem& p, pair<int,int> bounds, Measurements* m);
    // Destructor
    ~SmtEncoder() {
        yices_free_context(ctx);
        yices_exit();
    }

    /**
     * Encodes the problem instance into SMT and stores the result in the 'formula' field.
     */
    void encode() override;

    /**
     * Calls Yices to solve the feasibility problem with the current encoding.
     *
     * @return vector with the start time for each activity, will be empty if problem is unsatisfiable
     */
    vector<int> solve() override;

    /**
     * Finds the optimal solution by calling Yices repeatedly.
     * Starts with the given lower and upper bounds, and incrementally decreases the upper bound.
     *
     * While Yices is searching and interruption signal (SIGTERM) can be sent, stopping the search.
     * The best found solution so far can then be found in the Measurements struct.
     */
    void optimise() override;

private:
    vector<vector<int>> Estar; // List of successors for each activity, in the extended precedence graph
    vector<vector<int>> l;     // Time lags for all pairs of activities

    vector<term_t> S; // Variable S_i: start time of activity i
    vector<vector<term_t>> y; // Variable y_(i,t): boolean representing whether activity i starts at time t in STW(i)

    void floydWarshall();

    bool preprocessFeasible;

    /**
     * Perform preprocessing to reduce the amount of variables in the final encoding.
     *
     * @return false if preprocessing finds the instance to be infeasible, true otherwise
     */
    bool preprocess();

    /**
     * Initialises Yices solver, and creates (integer and Boolean) variables representing start times of activities.
     */
    void initialise();
};
}

#endif //RCPSPT_EXACT_SMTENCODER_H
