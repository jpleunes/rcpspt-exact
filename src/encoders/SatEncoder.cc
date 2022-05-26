/***********************************************************************************[SatEncoder.cc]
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
#include <queue>

#include "SatEncoder.h"
#include "ads/BDD.h"
#include "ads/PBConstr.h"

using namespace RcpsptExact;

SatEncoder::SatEncoder(Problem &p, pair<int, int> bounds, Measurements* m)
        : YicesEncoder(p, bounds, m) {
    preprocessFeasible = preprocess();
    initialise();
}

bool SatEncoder::preprocess() {
    return calcTimeWindows();
}

void SatEncoder::initialise() {
    yices_init();

    // Create the variables

    y.reserve(problem.njobs);
    for (int i = 0; i < problem.njobs; i++) {
        y.emplace_back();
        for (int t = ES[i]; t <= LS[i]; t++) { // t in STW(i) (start time window of activity i)
            term_t startb = yices_new_uninterpreted_term(yices_bool_type());
            y[i].push_back(startb);
            measurements->enc_n_boolv++;
        }
    }

    x.reserve(problem.njobs);
    for (int i = 0; i < problem.njobs; i++) {
        x.emplace_back();
        for (int t = ES[i]; t <= LC[i]; t++) {
            term_t processb = yices_new_uninterpreted_term(yices_bool_type());
            x[i].push_back(processb);
            measurements->enc_n_boolv++;
        }
    }

    // Create multi-check context, that uses propositional logic solver
    ctx_config_t* config = yices_new_config();
    yices_default_config_for_logic(config, "NONE");
    yices_set_config(config, "mode", "multi-checks");
    ctx = yices_new_context(config);
    yices_free_config(config);
}

void SatEncoder::encode() {
    /**
     * This SAT encoding is an adaptation of the encoding from the paper by M. Bofill et al. (2020) (reference in README.md).
     * The encoding for precedence constraints no longer uses Integer Difference Logic,
     * but instead CNF clauses in the way described by Horbach (2010) (reference in README.md).
     */

    if (!preprocessFeasible) {
//        std::cout << "Preprocessing found instance to be infeasible" << std::endl;
        formula = yices_false();
        return;
    }

    // Add precedence constraints

    vector<term_t> precedenceConstrs;

    // Consistency clauses
    for (int i = 0; i < problem.njobs; i++) {
        for (int s = ES[i]; s <= LS[i]; s++) { // s in STW(i)
            for (int t = s; t < s + problem.durations[i]; t++) {
                precedenceConstrs.push_back(yices_or2(yices_not(y[i][-ES[i] + s]), x[i][-ES[i] + t]));
                measurements->enc_n_clause++;
            }
        }
    }

    // Job 0 starts at 0
    precedenceConstrs.push_back(y[0][0]);
    measurements->enc_n_clause++;

    // Precedence clauses
    for (int i = 1; i < problem.njobs; i++) {
        for (int j : problem.predecessors[i]) {
            for (int s = ES[i]; s <= LS[i]; s++) { // s in STW(i)
                vector<term_t> clause;
                clause.push_back(yices_not(y[i][-ES[i] + s]));
                // Also check t <= LS[j], in addition to the definition by Horbach, because for RCPSP/t resource constraints can cause 'gaps' between activities (j,i)
                // Another difference: t <= ES[i]-durations[j] was replaced by t <= s-durations[j], the former definition was likely a mistake in the paper
                for (int t = ES[j]; t <= s-problem.durations[j] && t <= LS[j]; t++) {
                    clause.push_back(y[j][-ES[j] + t]);
                }
                precedenceConstrs.push_back(yices_or(clause.size(), &clause.front()));
                measurements->enc_n_clause++;
            }
        }
    }

    // Start clauses
    for (int i = 1; i < problem.njobs; i++) {
        vector<term_t> clause;
        for (int s = ES[i]; s <= LS[i]; s++) { // s in STW(i)
            clause.push_back(y[i][-ES[i] + s]);
        }
        precedenceConstrs.push_back(yices_or(clause.size(), &clause.front()));
        measurements->enc_n_clause++;
    }

    // Add redundant clauses that should improve runtime
    for (int i = 0; i < problem.njobs; i++) {
        for (int c = EC[i]; c < LC[i]; c++) {
            precedenceConstrs.push_back(yices_or3(yices_not(x[i][-ES[i] + c]), x[i][-ES[i] + c+1], y[i][-ES[i] + c-problem.durations[i]+1]));
            measurements->enc_n_clause++;
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
        int* measure_bools = &(measurements->enc_n_boolv); // Keep track of the number of boolean variables that is being created
        for (BDD* node : nodes) {
            if (node->terminal()) continue;
            term_t selector = y[node->selector.first][node->selector.second];
            // Add two clauses
            resourceConstrs.push_back(yices_or2(node->fBranch->getAux(measure_bools), yices_not(node->getAux(measure_bools))));
            resourceConstrs.push_back(yices_or3(node->tBranch->getAux(measure_bools), yices_not(selector), yices_not(node->getAux(measure_bools))));
            measurements->enc_n_clause += 2;
        }
        // Add three unary clauses
        resourceConstrs.push_back(nodes[auxRoot]->getAux(measure_bools));
        resourceConstrs.push_back(yices_not(nodes[auxTerminalF]->getAux(measure_bools)));
        resourceConstrs.push_back(nodes[auxTerminalT]->getAux(measure_bools));
        measurements->enc_n_clause += 3;

        for (BDD* node : nodes) if (!node->terminal()) delete node;
    }

    term_t f_precedence = yices_and(precedenceConstrs.size(), &precedenceConstrs.front());
    term_t f_resource = yices_and(resourceConstrs.size(), &resourceConstrs.front());
    formula = yices_and2(f_precedence, f_resource);
}

vector<int> SatEncoder::solve() {
    vector<int> solution;

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
                    bool started = false;
                    for (int s = ES[i]; s <= LS[i]; s++) {
                        code = yices_get_bool_value(model, y[i][-ES[i] + s], &v);
                        if (code < 0) {
                            std::cerr << "Cannot get model value " << i << std::endl;
                            yices_print_error(stderr);
                            break;
                        }
                        else {
                            if (v) {
                                std::cout << "S_" << i << " = " << s << std::endl;
                                solution.push_back(s);
                                started = true;
                                break;
                            }
                        }
                    }
                    if (!started) std::cerr << "Job " << i << " was not started" << std::endl;
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
    return solution;
}

void SatEncoder::optimise() {
    // This optimisation procedure was inspired by the paper by M. Bofill et al. (2020) (reference in README.md)

    int32_t code, v;
    code = yices_assert_formula(ctx, formula);
    if (code < 0) {
        std::cerr << "Assert failed: code = " << code << ", error = " << yices_error_code() << std::endl;
        yices_print_error(stderr);
    }
    smt_status_t status = yices_check_context(ctx, NULL);
    model_t* model;
    int UB_old;
    if (status == STATUS_SAT) {
        model = yices_get_model(ctx, true);
        if (model == NULL) {
            std::cerr << "Error getting model" << std::endl;
            yices_print_error(stderr);
        }
        else {
            for (int i = 0; i < problem.njobs; i++) {
                bool started = false;
                for (int s = ES[i]; s <= LS[i]; s++) {
                    code = yices_get_bool_value(model, y[i][-ES[i] + s], &v);
                    if (code < 0) {
                        std::cerr << "Cannot get model value " << i << std::endl;
                        yices_print_error(stderr);
                        break;
                    }
                    else {
                        if (v) {
                            measurements->schedule[i] = s;
                            started = true;
                            break;
                        }
                    }
                }
                if (!started) std::cerr << "Job " << i << " was not started" << std::endl;
            }
            yices_free_model(model);
        }
        UB_old = UB;
        UB = measurements->schedule.back() - 1;
    }
    else if (status == STATUS_INTERRUPTED) {
//        std::cout << "Search was interrupted" << std::endl;
        return;
    }
    else if (status == STATUS_UNSAT) {
        measurements->schedule.clear();
        return;
    }
    else {
        std::cerr << "Unknown status " << status << " when checking satisfiability" << std::endl;
        return;
    }
    while (status == STATUS_SAT && UB >= LB) {
//        std::cout << "Current makespan: " << measurements->schedule.back() << std::endl; // line for debugging
        for (int t = UB; t < UB_old; t++)
            formula = yices_and2(formula, yices_not(y.back()[-ES.back() + t + 1]));
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
                    bool started = false;
                    for (int s = ES[i]; s <= LS[i]; s++) {
                        code = yices_get_bool_value(model, y[i][-ES[i] + s], &v);
                        if (code < 0) {
                            std::cerr << "Cannot get model value " << i << std::endl;
                            yices_print_error(stderr);
                            break;
                        }
                        else {
                            if (v) {
                                measurements->schedule[i] = s;
                                started = true;
                                break;
                            }
                        }
                    }
                    if (!started) std::cerr << "Job " << i << " was not started" << std::endl;
                }
                yices_free_model(model);
            }
            UB_old = UB;
            UB = measurements->schedule.back() - 1;
        }
        else if (status == STATUS_INTERRUPTED) {
//            std::cout << "Search was interrupted" << std::endl;
            return;
        }
        else if (status != STATUS_UNSAT) {
            std::cerr << "Unknown status when checking satisfiability" << std::endl;
            return;
        }
    }

    if (status == STATUS_UNSAT || UB < LB) measurements->certified = true;
}
