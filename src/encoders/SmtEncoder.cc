/***********************************************************************************[SmtEncoder.cc]
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

#include <algorithm>
#include <iostream>
#include <cmath>

#include "SmtEncoder.h"
#include "ads/BDD.h"
#include "ads/PBConstr.h"

using namespace RcpsptExact;

void SmtEncoder::floydWarshall() {
    // Floyd-Warshall algorithm (https://en.wikipedia.org/wiki/Floyd-Warshall_algorithm)
    for (int k = 0; k < problem.njobs; k++) {
        for (int i = 0; i < problem.njobs; i++) {
            for (int j = 0; j < problem.njobs; j++) {
                if (l[i][k] + l[k][j] < l[i][j])
                    l[i][j] = l[i][k] + l[k][j];
            }
        }
    }
}

void SmtEncoder::preprocess() {
    // These preprocessing steps follow the paper by M. Bofill et al. (2020) (reference in README.md)

    // Calculate the extended precedence graph with time lags, using the Floyd-Warshall algorithm (https://en.wikipedia.org/wiki/Floyd-Warshall_algorithm)

    // Initialize matrix
    l.reserve(problem.njobs);
    for (int i = 0; i < problem.njobs; i++) {
        Estar.emplace_back();
        l.emplace_back(problem.njobs, INT32_MAX / 2);
    }
    for (int i = 0; i < problem.njobs; i++) {
        for (int j : problem.successors[i]) {
            l[i][j] = problem.durations[i];
        }
    }
    for (int i = 0; i < problem.njobs; i++) l[i][i] = 0;

    // Run Floyd-Warshall;
    floydWarshall();

    // Construct extended precedence graph
    Estar.reserve(problem.njobs);
    for (int i = 0; i < problem.njobs; i++) {
        for (int j = 0; j < problem.njobs; j++) {
            if (l[i][j] < INT32_MAX / 2)
                Estar[i].push_back(j);
        }
    }

    // Use energetic reasoning on precedences to improve accuracy of time lags

    // Find maximum capacity over time for each resource
    vector<int> maxCapacities(problem.nresources, 0);
    for (int k = 0; k < problem.nresources; k++)
        for (int t = 0; t < problem.horizon; t++)
            if (problem.capacities[k][t] > maxCapacities[k]) maxCapacities[k] = problem.capacities[k][t];

    // Update time lags
    for (int i = 0; i < problem.njobs; i++) {
        for (int j : Estar[i]) {
            if (i == j) continue;
            int maxRlb = -1;
            for (int k = 0; k < problem.nresources; k++) {
                int rlb = 0;
                for (int a: Estar[i]) {
                    if (a == j || l[a][j] == INT32_MAX / 2) continue;
                    for (int t = 0; t < problem.durations[a]; t++) rlb += problem.requests[a][k][t];
                }
                rlb /= maxCapacities[k];
                // Difference compared to the paper by M. Bofill et al. (2020): use maxRlb instead of durations[i]+maxRlb, the latter was likely a mistake in the paper
                if (rlb > maxRlb) maxRlb = rlb;
            }
            if (maxRlb > l[i][j]) {
                l[i][j] = maxRlb;
                // Rerun Floyd-Warshall to propagate update to other time lags
                floydWarshall();
            }
        }
    }

    // Set the time windows for the activities
    for (int i = 0; i < problem.njobs; i++) {
        ES[i] = l[0][i];
        EC[i] = l[0][i] + problem.durations[i];
        LS[i] = UB - l[i][problem.njobs - 1];
        LC[i] = UB - l[i][problem.njobs - 1] + problem.durations[i];
    }
}

void SmtEncoder::initialize() {
    yices_init();

    // Create the variables

    S.reserve(problem.njobs);
    for (int i = 0; i < problem.njobs; i++) {
        term_t startt = yices_new_uninterpreted_term(yices_int_type());
        S.push_back(startt);
    }

    y.reserve(problem.njobs);
    for (int i = 0; i < problem.njobs; i++) {
        y.emplace_back();
        for (int t = ES[i]; t <= LS[i]; t++) { // t in STW(i) (start time window of activity i)
            term_t startb = yices_new_uninterpreted_term(yices_bool_type());
            y[i].push_back(startb);
        }
    }

    // Create multi-check context, that uses Integer Difference Logic solver
    ctx_config_t* config = yices_new_config();
    yices_default_config_for_logic(config, "QF_IDL");
    yices_set_config(config, "mode", "multi-checks");
    ctx = yices_new_context(config);
    yices_free_config(config);
}

void SmtEncoder::encode() {
    // This SMT encoding follows the paper by M. Bofill et al. (2020) (reference in README.md)

    // Add precedence constraints

    vector<term_t> precedenceConstrs;

    // Initial dummy activity starts at 0
    precedenceConstrs.push_back(yices_arith_eq0_atom(S[0]));

    // Start variables must be within time windows
    for (int i = 1; i < problem.njobs; i++)
        precedenceConstrs.push_back(yices_arith_geq_atom(S[i], yices_int32(ES[i])));
    for (int i = 1; i < problem.njobs; i++)
        precedenceConstrs.push_back(yices_arith_leq_atom(S[i], yices_int32(LS[i])));

    // Enforce extended precedences
    for (int i = 0; i < problem.njobs; i++) {
        for (int j : Estar[i]) {
            if (i == j) continue;
            precedenceConstrs.push_back(yices_arith_geq_atom(yices_sub(S[j], S[i]), yices_int32(l[i][j])));
        }
    }

    // Enforce consistency for y_(i,t) variables
    for (int i = 0; i < problem.njobs; i++) {
        for (int t = ES[i]; t <= LS[i]; t++) { // t in STW(i)
            // y_(i,t) <=> (S_i = t) is encoded into (y_(i,t) => (S_i = t)) ^ ((S_i = t) => y_(i,t)), which is in turn
            // encoded into (~y_(i,t) v (S_i = t)) ^ (~(S_i = t) v y_(i,t))
            precedenceConstrs.push_back(yices_or2(yices_not(y[i][-ES[i] + t]), yices_arith_eq_atom(S[i], yices_int32(t))));
            precedenceConstrs.push_back(yices_or2(yices_not(yices_arith_eq_atom(S[i], yices_int32(t))), y[i][-ES[i] + t]));
        }
    }

    // Add resource constraints

    vector<term_t> resourceConstrs;

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
            term_t x = y[node->selector.first][node->selector.second];
            // Add two clauses
            resourceConstrs.push_back(yices_or2(node->fBranch->getAux(), yices_not(node->getAux())));
            resourceConstrs.push_back(yices_or3(node->tBranch->getAux(), yices_not(x), yices_not(node->getAux())));
        }
        // Add three unary clauses
        resourceConstrs.push_back(nodes[auxRoot]->getAux());
        resourceConstrs.push_back(yices_not(nodes[auxTerminalF]->getAux()));
        resourceConstrs.push_back(nodes[auxTerminalT]->getAux());

        for (BDD* node : nodes) if (!node->terminal()) delete node;
    }

    term_t f_precedence = yices_and(precedenceConstrs.size(), &precedenceConstrs.front());
    term_t f_resource = yices_and(resourceConstrs.size(), &resourceConstrs.front());
    formula = yices_and2(f_precedence, f_resource);
}

void SmtEncoder::solve(vector<int>& out) {
    // Pass formula to Yices
    int32_t code;
    code = yices_assert_formula(ctx, formula);
    if (code < 0) {
        std::cerr << "Assert failed: code = " << code << ", error = " << yices_error_code() << std::endl;
        yices_print_error(stderr);
    }

    switch (yices_check_context(ctx, NULL)) {
        case STATUS_SAT: {
            std::cout << "Satisfiable" << std::endl;
            model_t *model = yices_get_model(ctx, true);
            if (model == NULL) {
                std::cerr << "Error getting model" << std::endl;
                yices_print_error(stderr);
            }
            else {
                int32_t v;
                for (int i = 0; i < problem.njobs; i++) {
                    code = yices_get_int32_value(model, S[i], &v);
                    if (code < 0) {
                        std::cerr << "Cannot get model value " << i << std::endl;
                        yices_print_error(stderr);
                    }
                    else {
                        std::cout << "S_" << i << " = " << v << std::endl;
                        out.push_back(v);
                    }
                }

                yices_free_model(model);
            }
            break;
        }
        case STATUS_UNSAT:
            std::cout << "Unsatisfiable" << std::endl;
            break;
        case STATUS_UNKNOWN:
            std::cout << "Status unknown" << std::endl;
            break;
        case STATUS_IDLE:
        case STATUS_SEARCHING:
        case STATUS_INTERRUPTED:
        case STATUS_ERROR:
            std::cerr << "Status error" << std::endl;
            yices_print_error(stderr);
            break;
    }
}

vector<int> SmtEncoder::optimise() {
    // This optimisation procedure was inspired by the paper by M. Bofill et al. (2020) (reference in README.md)

    vector<int> solution(problem.njobs);
    int32_t code, v;
    code = yices_assert_formula(ctx, formula);
    if (code < 0) {
        std::cerr << "Assert failed: code = " << code << ", error = " << yices_error_code() << std::endl;
        yices_print_error(stderr);
    }
    smt_status_t status = yices_check_context(ctx, NULL);
    model_t* model;
    if (status == STATUS_SAT) {
        model = yices_get_model(ctx, true);
        if (model == NULL) {
            std::cerr << "Error getting model" << std::endl;
            yices_print_error(stderr);
        }
        else {
            for (int i = 0; i < problem.njobs; i++) {
                code = yices_get_int32_value(model, S[i], &v);
                if (code < 0) {
                    std::cerr << "Cannot get model value " << i << std::endl;
                    yices_print_error(stderr);
                }
                else solution[i] = v;
            }
            yices_free_model(model);
        }
        UB = solution.back() - 1;
    }
    else if (status == STATUS_UNSAT) return {};
    else {
        std::cerr << "Unknown status when checking satisfiability" << std::endl;
        return {};
    }
    while (status == STATUS_SAT && UB >= LB) {
        formula = yices_and2(formula, yices_arith_leq_atom(S.back(), yices_int32(UB)));
        code = yices_assert_formula(ctx, formula);
        if (code < 0) {
            std::cerr << "Assert failed: code = " << code << ", error = " << yices_error_code() << std::endl;
            yices_print_error(stderr);
        }
        status = yices_check_context(ctx, NULL);
        if (status == STATUS_SAT) {
            model = yices_get_model(ctx, true);
            if (model == NULL) {
                std::cerr << "Error getting model" << std::endl;
                yices_print_error(stderr);
            }
            else {
                for (int i = 0; i < problem.njobs; i++) {
                    code = yices_get_int32_value(model, S[i], &v);
                    if (code < 0) {
                        std::cerr << "Cannot get model value " << i << std::endl;
                        yices_print_error(stderr);
                    }
                    else solution[i] = v;
                }
                yices_free_model(model);
            }
            UB = solution.back() - 1;
            std::cout << "Current makespan: " << solution.back() << std::endl; // TODO: remove (this line is for debugging)
        }
        else if (status != STATUS_UNSAT) {
            std::cerr << "Unknown status when checking satisfiability" << std::endl;
            return {};
        }
    }
    return solution;
}
