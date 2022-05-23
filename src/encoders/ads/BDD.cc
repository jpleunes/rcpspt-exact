/******************************************************************************************[BDD.cc]
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

#include "BDD.h"

using namespace RcpsptExact;

BDD::BDD(bool termValue)
        : selector({-1,-1}), fBranch(nullptr), tBranch(nullptr), visited(false), aux(-1) {
    if (termValue) term = 1;
    else term = 0;
}

BDD::BDD(const pair<int, int> &selector, BDD *falseBranch, BDD *trueBranch)
        : selector(selector), fBranch(falseBranch), tBranch(trueBranch), term(-1), visited(false), aux(-1) {}

term_t BDD::getAux(int* measure_bools) {
    if (aux == -1) {
        aux = yices_new_uninterpreted_term(yices_bool_type());
        (*measure_bools)++;
    }
    return aux;
}

bool BDD::terminal() const {
    return term != -1;
}

bool BDD::terminalValue() const {
    return term == 1;
}

int BDD::flatten(vector<BDD *> &out) {
    visited = true;
    int rootIndex;
    if (terminal()) {
        rootIndex = (int)out.size();
        out.push_back(this);
        return rootIndex;
    }
    if (!fBranch->visited) fBranch->flatten(out);
    rootIndex = (int)out.size();
    out.push_back(this);
    if (!tBranch->visited) tBranch->flatten(out);
    return rootIndex;
}

LSet::LSet(const pair<int, int> &interval, BDD *robdd)
        : interval(interval), robdd(robdd), l(nullptr), r(nullptr) { }

bool LSet::insert(const pair<int, int> &newInterval, BDD *newRobdd) {
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

pair<pair<int, int>, BDD *> LSet::search(int K) const {
    if (interval.first <= K && K <= interval.second) return {interval, robdd};
    if (K < interval.first && l != nullptr) return l->search(K);
    if (K > interval.second && r != nullptr) return r->search(K);
    return {{-1,-1}, nullptr};
}

void LSet::deleteTree() {
    if (l != nullptr) l->deleteTree();
    delete l;
    l = nullptr;
    if (r != nullptr) r->deleteTree();
    delete r;
    r = nullptr;
}

pair<pair<int,int>,BDD*> BDD::BDDConstruction(int i, const PBConstr& C, int KPrime, vector<LSet>& L) {
    // This function is fully based on Algorithm 2 in the paper by I. Abío et al. (2012) (reference in README.md)

    pair<pair<int,int>,BDD*> result = L[i].search(KPrime);
    if (result.second != nullptr) return result;

    pair<pair<int,int>, BDD*> resF = BDDConstruction(i+1, C, KPrime, L);
    pair<pair<int,int>, BDD*> resT = BDDConstruction(i+1, C, KPrime - C.constant(i), L);

    if (resF.first == resT.first) {
    result = { { resT.first.first + C.constant(i), resT.first.second }, resT.second };

    if (resT.second != resF.second && resF.second != nullptr) {
        vector<BDD*> nodesToDelete;
        resF.second->flatten(nodesToDelete);
        for (BDD* node : nodesToDelete) if (!node->terminal()) delete node;
    }
    }
    else {
        BDD* robdd = new BDD(C.var(i), resF.second, resT.second);
        int intervalL = std::max(resF.first.first, resT.first.first + C.constant(i));
        int intervalR = std::min(resF.first.second, resT.first.second + C.constant(i));
        result = {{intervalL, intervalR}, robdd};
    }

    L[i].insert(result.first, result.second);
    return result;
}
