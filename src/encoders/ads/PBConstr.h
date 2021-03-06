/**************************************************************************************[PRConstr.h]
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

#ifndef RCPSPT_EXACT_PBCONSTR_H
#define RCPSPT_EXACT_PBCONSTR_H

#include <vector>

using namespace std;

namespace RcpsptExact {
/**
 * Data structure representing a pseudo-boolean (PB) constraint.
 */
class PBConstr {
public:
    const int K;

    PBConstr(int K);

    int nTerms() const;

    /**
     * Adds a term to the summation.
     *
     * @param constant the integer constant
     * @param varIndex index of the boolean variable
     */
    void addTerm(int constant, pair<int,int> varIndex);

    /**
     * Gets the integer constant at index i in the summation.
     *
     * @param i index
     * @return the integer constant
     */
    int constant(int i) const;

    /**
     * Gets the index of the boolean variable at index i in the summation.
     *
     * @param i
     * @return
     */
    const pair<int,int>& var(int i) const;

private:
    vector<int> q;
    vector<pair<int,int>> y_ixs;
    int n;
};
}

#endif //RCPSPT_EXACT_PBCONSTR_H
