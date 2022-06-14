/*******************************************************************************************[BDD.h]
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

#ifndef RCPSPT_EXACT_BDD_H
#define RCPSPT_EXACT_BDD_H

#include <iostream>
#include <vector>

#include "PBConstr.h"
#include "yices.h"

using namespace std;

namespace RcpsptExact {

class LSet;

/**
 * Data structure representing a Binary Decision Diagram.
 */
class BDD {
public:
    /**
     * Construct a terminal node.
     *
     * @param termValue boolean value of the node
     */
    BDD(bool termValue);

    /**
     * Construct a non-terminal node.
     *
     * @param selector variable in the PB constraint that is represented by this node
     * @param falseBranch BDD corresponding to 'False' assignment
     * @param trueBranch BDD corresponding to 'True' assignment
     */
    BDD(const pair<int,int>& selector, BDD* falseBranch, BDD* trueBranch);

    const pair<int,int> selector; // Index of the decision variable y_(i,t)
    BDD* fBranch; // Child for the 'False' branch
    BDD* tBranch; // Child for the 'True' branch

    term_t getAuxYices(int* measure_bools);

    int getAuxWcnf(int* nextIndex);

    bool terminal() const;

    bool terminalValue() const;

    int flatten(vector<BDD*>& out);

    static pair<pair<int,int>, BDD*> BDDConstruction(int i, const PBConstr& C, int KPrime, vector<LSet>& L);

private:
    int term; // Indicates whether the node is terminal: -1 not terminal, 0 terminal w/ val. False, 1 terminal w/ val. True
    bool visited; // Indicates whether the node has been visited (used for flatten(out))
    term_t auxYices; // Auxiliary Boolean variable used for creating a SAT encoding of an ROBDD (only for Yices encoders)
    int auxWcnf; // Index of auxiliary Boolean variable used for creating a SAT encoding of an ROBDD (only for WCNF encoder)
};

/**
 * Binary tree-like data structure representing a set of pairs (interval, robdd).
 */
class LSet {
public:
    LSet(const pair<int,int>& interval, BDD* robdd);

    pair<int,int> interval; // Interval of the ROBDD
    BDD* robdd; // Reduced Ordered BDD
    LSet* l;
    LSet* r;

    bool insert(const pair<int,int>& newInterval, BDD* newRobdd);

    pair<pair<int,int>,BDD*> search(int K) const;

    void deleteTree();
};
}

#endif //RCPSPT_EXACT_BDD_H
