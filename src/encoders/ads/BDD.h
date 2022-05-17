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

#include "PBConstr.h"

namespace RcpsptExact {
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
    BDD(bool termValue)
            : selector({-1,-1}), fBranch(nullptr), tBranch(nullptr), id(-1), encoded(false) {
        if (termValue) term = 1;
        else term = 0;
    }

    /**
     * Construct a non-terminal node.
     *
     * @param selector variable in the PB constraint that is represented by this node
     * @param falseBranch BDD corresponding to 'False' assignment
     * @param trueBranch BDD corresponding to 'True' assignment
     */
    BDD(const pair<int,int>& selector, BDD* falseBranch, BDD* trueBranch)
            : selector(selector), term(-1), fBranch(falseBranch), tBranch(trueBranch), id(-1), encoded(false) {}

    const pair<int,int> selector; // Index of the decision variable y_(i,t)
    BDD* fBranch; // Child for the 'False' branch
    BDD* tBranch; // Child for the 'True' branch

    int id; // Node identifier, used for keeping track of auxiliary boolean variables
    bool encoded; // Indicates whether the SAT clauses for this (non-terminal) node have already been generated

    bool terminal() const {
        return term != -1;
    }

    bool terminalValue() const {
        return term == 1;
    }

    /**
     * TODO
     *
     * @param nLabeled
     * @return
     */
    int label(int nLabeled) {
        if (id >= 0) return nLabeled;
        if (terminal()) {
            id = nLabeled;
            return nLabeled + 1;
        }
        int l = fBranch->label(nLabeled);
        id = l;
        int r = tBranch->label(id + 1);
        return r;
    }
private:
    int term; // Indicates whether the node is terminal: -1 not terminal, 0 terminal w/ val. False, 1 terminal w/ val. True
};

/**
 * Binary tree-like data structure representing a set of pairs (interval, robdd).
 */
class LSet {
public:
    LSet(const pair<int,int>& interval, BDD* robdd)
            : interval(interval), robdd(robdd), l(nullptr), r(nullptr) { }

    pair<int,int> interval; // Interval of the ROBDD
    BDD* robdd; // Reduced Ordered BDD
    LSet* l;
    LSet* r;

    bool insert(const pair<int,int>& newInterval, BDD* newRobdd) {
        if (newInterval == interval) return false;
        if (newInterval.second < interval.first) {
            if (l == nullptr) {
                l = new LSet(newInterval, newRobdd);
                return true;
            }
            else return l->insert(newInterval, newRobdd);
        }
        if (newInterval.first > interval.second) {
            if (r == nullptr) {
                r = new LSet(newInterval, newRobdd);
                return true;
            }
            else return r->insert(newInterval, newRobdd);
        }

        std::cerr << "Invalid call to LSet::insert" << std::endl;
        return false;
    }

    pair<pair<int,int>,BDD*> search(int K) const {
        if (interval.first <= K && K <= interval.second) return {interval, robdd};
        if (K < interval.first && l != nullptr) return l->search(K);
        if (K > interval.second && r != nullptr) return r->search(K);
        return {{-1,-1}, nullptr};
    }
};

pair<pair<int,int>, BDD*> BDDConstruction(int i, const PBConstr& C, int KPrime, vector<LSet>& L) {
    // This function is fully based on ... TODO

    pair<pair<int,int>,BDD*> result = L[i].search(KPrime);
    if (result.second != nullptr) return result;

    pair<pair<int,int>, BDD*> resF = BDDConstruction(i+1, C, KPrime, L);
    pair<pair<int,int>, BDD*> resT = BDDConstruction(i+1, C, KPrime - C.constant(i), L);

    if (resF.first == resT.first)
        result = { { resT.first.first + C.constant(i), resT.first.second }, resT.second };
    else {
        BDD* robdd = new BDD(C.var(i), resF.second, resT.second);
        int intervalL = std::max(resF.first.first, resT.first.first + C.constant(i));
        int intervalR = std::min(resF.first.second, resT.first.second + C.constant(i));
        result = {{intervalL, intervalR}, robdd};
    }

    L[i].insert(result.first, result.second);
    return result;
}
}

#endif //RCPSPT_EXACT_BDD_H
