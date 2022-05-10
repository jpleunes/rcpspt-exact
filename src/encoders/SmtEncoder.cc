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
#include "yices.h"

using namespace RcpsptExact;

void SmtEncoder::floydWarshall() {
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
                rlb = ceil((1. / maxCapacities[k]) * rlb);
                if (rlb > maxRlb) maxRlb = rlb;
            }
            int lPrime = problem.durations[i] + maxRlb;
            if (lPrime > l[i][j]) l[i][j] = lPrime;
        }
    }

    // Rerun Floyd-Warshall to propagate updates to time lags
    floydWarshall();

    // Set the time windows for the activities

    ES.reserve(problem.njobs);
    EC.reserve(problem.njobs);
    LS.reserve(problem.njobs);
    LC.reserve(problem.njobs);

    for (int i = 0; i < problem.njobs; i++) {
        ES.push_back(l[0][i]);
        EC.push_back(l[0][i] + problem.durations[i]);
        LS.push_back(UB - l[i][problem.njobs - 1]);
        LC.push_back(UB - l[i][problem.njobs - 1] + problem.durations[i]);
    }
}

void SmtEncoder::encode() {
    // This SMT encoding follows the paper by M. Bofill et al. (2020) (reference in README.md)

    yices_init();

    // Create the variables

    // S_i: start time of activity i
    vector<term_t> S;
    for (int i = 0; i < problem.njobs; i++) {
        term_t startt = yices_new_uninterpreted_term(yices_int_type());
        S.push_back(startt);
    }

    // y_(i,t): boolean representing whether activity i starts at time t in STW(i)
    vector<vector<term_t>> y;
    for (int i = 0; i < problem.njobs; i++) {
        y.emplace_back();
        for (int t = ES[i]; t <= LS[i]; t++) { // t in STW(i) (start time window of activity i)
            term_t startb = yices_new_uninterpreted_term(yices_bool_type());
            y[i].push_back(startb);
        }
    }

    // Add precedence constraints

    vector<term_t> precedenceConstrs;

    // Initial dummy activity starts at 0
    precedenceConstrs.push_back(yices_arith_eq0_atom(S[0]));

    // Start variables must be within time windows
    for (int i = 0; i < problem.njobs; i++)
        precedenceConstrs.push_back(yices_arith_geq_atom(S[i], yices_int32(ES[i])));
    for (int i = 0; i < problem.njobs; i++)
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
        for (int t = 0; t <= LS[i] - ES[i]; i++) { // t in STW(i)
            // y_(i,t) <=> S_i is encoded into (y_(i,t) => S_i) ^ (S_i => y_(i,t)), which is in turn
            // encoded into (~y_(i,t) v S_i) ^ (~S_i v y_(i,t))
            precedenceConstrs.push_back(yices_and2(yices_or2(yices_not(y[i][t]), yices_arith_eq_atom(S[i], yices_int32(t))),
                                                   yices_or2(yices_not(yices_arith_eq_atom(S[i], yices_int32(t))), y[i][t])));
        }
    }

    // Add resource constraints

    vector<term_t> resourceConstrs;

    // TODO

    // Pass all constraints to Yices

    int32_t code;

    term_t f_precedence = yices_and(precedenceConstrs.size(), &precedenceConstrs.front());
    term_t f_resource = yices_and(resourceConstrs.size(), &resourceConstrs.front());
    term_t f_final = yices_and2(f_precedence, f_resource);

    context_t* ctx = yices_new_context(NULL);

    code = yices_assert_formula(ctx, f_final);
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

    yices_free_context(ctx);

    yices_exit();
}
