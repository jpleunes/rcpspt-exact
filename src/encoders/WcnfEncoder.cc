/**********************************************************************************[WcnfEncoder.cc]
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

#include "WcnfEncoder.h"
#include "ads/BDD.h"
#include "ads/PBConstr.h"

#include <sstream>

using namespace RcpsptExact;

WcnfEncoder::WcnfEncoder(Problem& p, pair<int,int> bounds)
        : Encoder(p, bounds) {
    preprocessFeasible = preprocess();
}

bool WcnfEncoder::preprocess() {
    return calcTimeWindows();
}

void WcnfEncoder::encodeAndWriteToFile(const string& filePath) {
    ofstream outFile(filePath);
    if (!preprocessFeasible) {
        writeInfeasible(outFile);
        outFile.close();
        return;
    }

    int nextIndex = 0; // Index to be used by the next Boolean variable to be added

    vector<vector<int>> y; // indices of Boolean start variables y_(i,t)
    int ny = 0;
    y.reserve(problem.njobs);
    for (int i = 0; i < problem.njobs; i++) {
        y.emplace_back();
        for (int t = ES[i]; t <= LS[i]; t++) { // t in STW(i) (start time window of activity i)
            y[i].push_back(nextIndex++);
            ny++;
        }
    }
    vector<vector<int>> x; // indices of Boolean process variables x_(i,t)
    int nx = 0;
    x.reserve(problem.njobs);
    for (int i = 0; i < problem.njobs; i++) {
        x.emplace_back();
        for (int t = ES[i]; t <= LC[i]; t++) { // t in RTW(i) (run time window of activity i)
            x[i].push_back(nextIndex++);
            nx++;
        }
    }

    int top = INT32_MAX/2; // Weight to be used for hard clauses

    // The following mapping from indices to variables will be used for the output file:
    //  - indices [1,...,ny] are the start variables
    //  - indices [ny+1,...,ny+nx] are the process variables
    //  - from index ny+nx+1 onwards are auxiliary variables

    // Write a file header in the form of comments, containing information for converting from
    // a SAT model to a solution for the original problem.
    // First write the number of start and process variables
    outFile << "c " << ny << ' ' << nx << std::endl;
    outFile << 'c' << std::endl;
    // Write the earliest and latest feasible start time for each activity
    for (int i = 0; i < problem.njobs; i++)
        outFile << "c " << i+1 << ' ' << ES[i] << ' ' << LS[i] << std::endl;
    outFile << 'c' << std::endl;

    // Add precedence constraints

    vector<string> precedenceConstrs;

    // Consistency clauses
    for (int i = 0; i < problem.njobs; i++) {
        for (int s = ES[i]; s <= LS[i]; s++) { // s in STW(i)
            for (int t = s; t < s + problem.durations[i]; t++) {
                string clause = to_string(top)
                        + " -" + to_string(1 + y[i][-ES[i] + s])
                        + ' ' + to_string(1 + x[i][-ES[i] + t])
                        + " 0";
                precedenceConstrs.push_back(clause);
            }
        }
    }

    // Job 0 starts at 0
    precedenceConstrs.push_back(to_string(top) + ' ' + to_string(1 + y[0][0]) + " 0");

    // Precedence clauses
    for (int i = 1; i < problem.njobs; i++) {
        for (int j : problem.predecessors[i]) {
            for (int s = ES[i]; s <= LS[i]; s++) { // s in STW(i)
                string clause = to_string(top)
                        + " -" + to_string(1 + y[i][-ES[i] + s]);
                // Also check t <= LS[j], in addition to the definition by Horbach, because for RCPSP/t resource constraints can cause 'gaps' between activities (j,i)
                // Another difference: t <= ES[i]-durations[j] was replaced by t <= s-durations[j], the former definition was likely a mistake in the paper
                for (int t = ES[j]; t <= s-problem.durations[j] && t <= LS[j]; t++) {
                    clause += ' ' + to_string(1 + y[j][-ES[j] + t]);
                }
                clause += " 0";
                precedenceConstrs.push_back(clause);
            }
        }
    }

    // Start clauses
    for (int i = 1; i < problem.njobs; i++) {
        string clause = to_string(top);
        for (int s = ES[i]; s <= LS[i]; s++) { // s in STW(i)
            clause += ' ' + to_string(1 + y[i][-ES[i] + s]);
        }
        clause += " 0";
        precedenceConstrs.push_back(clause);
    }

    // Add redundant clauses that should improve runtime
    for (int i = 0; i < problem.njobs; i++) {
        for (int c = EC[i]; c < LC[i]; c++) {
            string clause = to_string(top)
                    + " -" + to_string(1 + x[i][-ES[i] + c])
                    + ' ' + to_string(1 + x[i][-ES[i] + c+1])
                    + ' ' + to_string(1 + y[i][-ES[i] + c-problem.durations[i]+1])
                    + " 0";
            precedenceConstrs.push_back(clause);
        }
    }

    // Add resource constraints

    vector<string> resourceConstrs;

    // List of pseudo-boolean (PB) constraints
    vector<PBConstr> pbConstrs;

    // Determine PB constraints
    int q_i;
    for (int k = 0; k < problem.nresources; k++) {
        for (int t = 0; t < UB; t++) {
            pbConstrs.emplace_back(problem.capacities[k][t]);
            for (int i = 0; i < problem.njobs; i++) {
                if (t < ES[i] || t >= LC[i]) continue; // only consider i if t in RTW(i)
                for (int e = 0; e < problem.durations[i]; e++) {
                    if (t-e < ES[i] || t-e > LS[i]) continue; // only consider e if t-e in STW(i)
                    q_i = problem.requests[i][k][e];
                    if (q_i == 0) continue;
                    pbConstrs.back().addTerm(q_i, {i, -ES[i] + t-e});
                }
            }
            if (pbConstrs.back().nTerms() == 0) pbConstrs.pop_back();
        }
    }

    // Encode each PB constraint
    for (const PBConstr& C : pbConstrs) {
        // Construct an ROBDD (Reduced Ordered BDD), following Algorithm 1 and Example 24: BDD-1 from the paper by I. Abío et al. (2012) (reference in README.md)
        BDD falseNode(false);
        BDD trueNode(true);
        vector<LSet> L;
        for (int i = 0; i <= C.nTerms(); i++) {
            int constsSum = 0;
            for (int j = i; j < C.nTerms(); j++) constsSum += C.constant(j);
            L.push_back(LSet({constsSum,INT32_MAX/2}, &trueNode));
            L.back().insert({INT32_MIN/2, -1}, &falseNode);
        }
        pair<pair<int,int>,BDD*> result = BDD::BDDConstruction(0, C, C.K, L);
        for (LSet& s : L) s.deleteTree();
        BDD* robdd = result.second;
        vector<BDD*> nodes;
        int auxRoot = robdd->flatten(nodes);

        // Add SAT clauses based on the ROBDD, following Example 24: BDD-1 from the paper by I. Abío et al. (2012) (reference in README.md)
        int auxTerminalF = -1;
        int auxTerminalT = -1;
        for (int i = 0; i < (int)nodes.size(); i++) {
            if (nodes[i]->terminal()) {
                if (nodes[i]->terminalValue()) auxTerminalT = i;
                else auxTerminalF = i;
            }
        }
        if (auxTerminalF == -1) continue; // Skip if the constraint cannot be falsified
        for (BDD* node : nodes) {
            if (node->terminal()) continue;
            term_t selector = y[node->selector.first][node->selector.second];
            // Add two clauses
            string clause1 = to_string(top)
                    + ' ' + to_string(1 + node->fBranch->getAuxWcnf(&nextIndex))
                    + " -" + to_string(1 + node->getAuxWcnf(&nextIndex))
                    + " 0";
            resourceConstrs.push_back(clause1);
            string clause2 = to_string(top)
                    + ' ' + to_string(1 + node->tBranch->getAuxWcnf(&nextIndex))
                    + " -" + to_string(1 + selector)
                    + " -" + to_string(1 + node->getAuxWcnf(&nextIndex))
                    + " 0";
            resourceConstrs.push_back(clause2);
        }
        // Add three unary clauses
        resourceConstrs.push_back(to_string(top) + ' ' + to_string(1 + nodes[auxRoot]->getAuxWcnf(&nextIndex)) + " 0");
        resourceConstrs.push_back(to_string(top) + " -" + to_string(1 + nodes[auxTerminalF]->getAuxWcnf(&nextIndex)) + " 0");
        resourceConstrs.push_back(to_string(top) + ' ' + to_string(1 + nodes[auxTerminalT]->getAuxWcnf(&nextIndex)) + " 0");

        for (BDD* node : nodes) if (!node->terminal()) delete node;
    }

    // Write the encoded problem to the file

    int nbvar = nextIndex;
    int nbclauses = (int)precedenceConstrs.size() + (int)resourceConstrs.size(); // TODO: include soft clauses
    outFile << "p wcnf " << nbvar << ' ' << nbclauses << ' ' << top << std::endl;
    for (const string& ln : precedenceConstrs) outFile << ln << std::endl;
    for (const string& ln : resourceConstrs) outFile << ln << std::endl;
    // TODO: soft clauses

    outFile.close();
}

string WcnfEncoder::getAndCheckSolution(const string &model) {
    if (!preprocessFeasible) return "-1, 1, ";

    vector<bool> lits;
    istringstream iss(model);
    string substr;
    while (getline(iss, substr, ' ')) {
        lits.push_back(substr[0] != '-');
    }

    vector<int> starts(problem.njobs, -1);
    int curr = 0;
    for (int i = 0; i < problem.njobs; i++) {
        for (int t = ES[i]; t <= LS[i]; t++) { // t in STW(i) (start time window of activity i)
            if (lits[curr++]) starts[i] = t;
        }
    }

    int makespan = starts.back();
    bool valid = ValidityChecker::checkValid(problem, starts);

    string output = to_string(makespan) + ", " + to_string(valid) + ", ";
    for (int s : starts) output += to_string(s) + '.';
    return output;
}

void WcnfEncoder::writeInfeasible(ofstream& outFile) {
    outFile << "p wcnf 1 1 1" << std::endl;
    outFile << "1 -1 0" << std::endl;
}

#include "WcnfEncoder.h"
