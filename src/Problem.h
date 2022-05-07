/******************************************************************************* *******[Problem.h]
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

#ifndef RCPSPT_HEURISTIC_PROBLEM_H
#define RCPSPT_HEURISTIC_PROBLEM_H

#include <vector>

using namespace std;

namespace RcpsptExact {

/**
 * Class representing an instance of the RCPSP/t.
 */
class Problem {
public:
    // Constructor
    Problem(int njobs, int horizon, int nresources);
    // Destructor
    virtual ~Problem();

    // Data

    const int njobs;                      // Number of activities (including dummy start and end activities)
    const int horizon;                    // Planning horizon T
    const int nresources;                 // Number of renewable resources
    vector<vector<int>> successors;       // List of successors for each activity
    vector<vector<int>> predecessors;     // List of predecessors for each activity (for backwards traversal of the precedence graph)
    vector<int> durations;                // Duration for each activity
    vector<vector<vector<int>>> requests; // Request per time step, per resource, per activity
    vector<vector<int>> capacities;       // Capacity for each time step, per resource
};
}

#endif //RCPSPT_HEURISTIC_PROBLEM_H
