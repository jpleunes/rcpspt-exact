/**********************************************************************************[YicesEncoder.h]
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

#ifndef RCPSPT_EXACT_YICESENCODER_H
#define RCPSPT_EXACT_YICESENCODER_H

#include <ctime>

#include "../Problem.h"
#include "yices.h"

namespace RcpsptExact {
/**
 * TODO
 */
struct Measurements {
    string file; // Input file path
    int enc_n_boolv = 0; // Number of Boolean variables in encoding
    int enc_n_intv = 0; // Number of integer variables in encoding
    int enc_n_clause = 0; // Number of clauses in encoding
    long t_enc = 0; // Time in ms spent on encoding
    long t_solve = 0; // Time in ms spent on solving
    int makespan = -1; // Best makespan so far (-1 if no solution)
    bool valid = false; // Whether the current best solution has been checked for validity
    bool certified = false; // Whether the current best solution has been proven optimal (or infeasible)
    vector<int> schedule = {}; // Current best solution
};

/**
 * Abstract base class for encoders that use the Yices C API.
 */
class YicesEncoder {
public:
    virtual ~YicesEncoder();
    virtual void encode() = 0;
    virtual vector<int> solve() = 0;
    virtual vector<int> optimise() = 0;

    /**
     * Outputs measurement results to the console, in the following format:
     * file, enc_n_boolv, enc_n_intv, enc_n_clause, t_enc, t_solve, t_total, makespan, valid, certified, schedule
     *
     * An example would look like this:
     * path/to/file.smt, 12, 5, 60, 65, 128, 300, 20, 1, 1, 0.0.3.4.7.
     */
    void printResults() const;

    context_t* ctx; // Yices context
    Measurements* measurements;
};
}

#endif //RCPSPT_EXACT_YICESENCODER_H
