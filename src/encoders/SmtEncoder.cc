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
        for (int j = 0; j < problem.njobs; j++) {
            if (l[i][j] == INT32_MAX / 2) continue;
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
    // Trying out the Yices library:

    yices_init();
    term_t x = yices_new_uninterpreted_term(yices_bool_type());
    yices_set_term_name(x, "x");
    term_t y = yices_new_uninterpreted_term(yices_bool_type());
    yices_set_term_name(y, "y");
    term_t f = yices_or2(x, y);
    yices_pp_term(stdout, f, 80, 20, 0);

    context_t* ctx = yices_new_context(NULL);
    int32_t code = yices_assert_formula(ctx, f);

    switch(yices_check_context(ctx, NULL)) {
        case STATUS_SAT:
            model_t* model = yices_get_model(ctx, true);
            code = yices_pp_model(stdout, model, 80, 4, 0);
            int32_t vx, vy;
            yices_get_bool_value(model, x, &vx);
            yices_get_bool_value(model, y, &vy);
            std::cout << "x, y: " << vx << ", " << vy << std::endl;
            break;
    }

    yices_free_context(ctx);
    yices_exit();
}
