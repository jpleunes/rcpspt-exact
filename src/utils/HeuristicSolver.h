/******************************************************************************[HeuristicSolver.cc]
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

#ifndef RCPSPT_EXACT_HEURISTICSOLVER_H
#define RCPSPT_EXACT_HEURISTICSOLVER_H

#include <queue>
#include <random>

#define TOURN_FACTOR 0.5
#define OMEGA1 0.4
#define OMEGA2 0.6

using namespace std;

namespace RcpsptExact {

/**
 * Tournament heuristic using a priority rule, used for calculating initial lower and upper bounds on the makespan.
 *
 * @param problem problem instance to consider
 * @return pair of integers (lower_bound, upper_bound)
 */
pair<int, int> calcBoundsPriorityRule(Problem &problem) {
    // This function is based on the tournament heuristic that is described by Hartmann (2013) (reference in README.md)

    queue<int> q; // Use a queue for breadth-first traversal of the precedence graph
    vector<int> ef(problem.njobs, 0); // Earliest feasible finish time for each job
    q.push(0);
    while(!q.empty()) {
        int job = q.front();
        q.pop();
        int duration = problem.durations[job];
        // Move finish until it is feasible considering resource constraints
        bool feasibleFinal = false;
        while (!feasibleFinal) {
            bool feasible = true;
            for (int k = 0; feasible && k < problem.nresources; k++) {
                for (int t = duration - 1; feasible && t >= 0; t--) {
                    if (problem.requests[job][k][t] > problem.capacities[k][ef[job] - duration + t]) {
                        feasible = false;
                        ef[job]++;
                    }
                }
            }
            if (feasible) feasibleFinal = true;
            if (ef[job] > problem.horizon) return {0, problem.horizon};
        }
        // Update finish times, and enqueue successors
        for (int successor : problem.successors[job]) {
            int f = ef[job] + problem.durations[successor];
            if (f > ef[successor]) ef[successor] = f; // Use maximum values, because we are interested in critical paths
            q.push(successor); // Enqueue successor
        }
    }
    vector<int> ls(problem.njobs, problem.horizon); // Latest feasible start time for each job
    q.push(problem.njobs - 1);
    while (!q.empty()) {
        int job = q.front();
        q.pop();
        int duration = problem.durations[job];
        // Move start until it is feasible considering resource constraints
        bool feasibleFinal = false;
        while (!feasibleFinal) {
            bool feasible = true;
            for (int k = 0; feasible && k < problem.nresources; k++) {
                for (int t = 0; feasible && t < duration; t++) {
                    if (problem.requests[job][k][t] > problem.capacities[k][ls[job] + t]) {
                        feasible = false;
                        ls[job]--;
                    }
                }
            }
            if (feasible) feasibleFinal = true;
            if (ls[job] < 0) return {ef.back(), problem.horizon};
        }
        // Update start times, and enqueue predecessors
        for (int predecessor : problem.predecessors[job]) {
            int s = ls[job] - problem.durations[predecessor];
            if (s < ls[predecessor]) ls[predecessor] = s; // Use minimum values for determining critical paths
            q.push(predecessor); // Enqueue predecessor
        }
    }

    // Calculate extended resource utilization values, using the definition from Hartmann (2013) (reference in README.md)
    vector<double> ru(problem.njobs);
    q.push(problem.njobs - 1); // Enqueue sink job
    while (!q.empty()) {
        int job = q.front();
        q.pop();
        int duration = problem.durations[job];
        int demand = 0, availability = 0;
        for (int k = 0; k < problem.nresources; k++) {
            for (int t = 0; t < duration; t++) demand += problem.requests[job][k][t];
            for (int t = ef[job] - duration; t < ls[job] + duration; t++) availability += problem.capacities[k][t]; // t in RTW(job)
        }
        ru[job] = OMEGA1 * (((double) problem.successors[job].size() / (double) problem.nresources) *
                            ((double) demand / (double) availability));
        for (int successor : problem.successors[job]) ru[job] += OMEGA2 * ru[successor];
        if (isnan(ru[job]) || ru[job] < 0.0) ru[job] = 0.0; // Prevent errors from strange values here
        // Enqueue predecessors
        for (int predecessor : problem.predecessors[job]) q.push(predecessor);
    }

    // Calculate the CPRU (critical path and resource utilization) priority value for each activity, using the definition from Hartmann (2013) (reference in README.md)
    vector<double> cpru(problem.njobs);
    for (int job = 0; job < problem.njobs; job++) {
        int cp = problem.horizon - ls[job]; // Critical path length
        cpru[job] = cp * ru[job];
    }

    //random_device rd;
    default_random_engine eng(42); // Set seed for deterministic bounds to compare different encodings/solvers
    uniform_real_distribution<double> distribution(0, 1);
    // Run a number of passes ('tournaments'), as described by Hartmann (2013) (reference in README.md)
    vector<vector<int>> available(problem.nresources);
    vector<int> schedule(problem.njobs); // Finish(!) time for each process
    int bestMakespan = INT32_MAX/2;
    for (int pass = 0; pass < (problem.njobs - 2) * 5; pass++) { // Number of passes scales with number of jobs (njobs multiplied by a magic number 5, in this case)
        for (int i = 1; i < problem.njobs; i++) schedule[i] = -1;
        // Initialize remaining resource availabilities
        for (int k = 0; k < problem.nresources; k++)
            available[k] = problem.capacities[k];

        // Schedule the starting dummy activity
        schedule[0] = 0;

        // Schedule all remaining jobs
        for (int i = 1; i < problem.njobs; i++) {
            // Randomly select a fraction of the eligible activities (with replacement)
            vector<int> eligible;
            for (int j = 1; j < problem.njobs; j++) {
                if (schedule[j] >= 0) continue;
                bool predecessorsScheduled = true;
                for (int predecessor : problem.predecessors[j])
                    if (schedule[predecessor] < 0) predecessorsScheduled = false;
                if (predecessorsScheduled) eligible.push_back(j);
            }
            int Z = max((int)(TOURN_FACTOR * (int)eligible.size()), 2);
            vector<int> selected;
            for (int j = 0; j < Z; j++) {
                int choice = (int)(distribution(eng) * (int)eligible.size());
                selected.push_back(eligible[choice]);
            }
            // Select the activity with the best priority value
            int winner = -1;
            double bestPriority = -MAXFLOAT/2.0;
            for (int sjob : selected) {
                if (cpru[sjob] >= bestPriority) {
                    bestPriority = cpru[sjob];
                    winner = sjob;
                }
            }
            // Schedule it as early as possible
            int finish = -1;
            for (int predecessor : problem.predecessors[winner]) {
                int newFinish = schedule[predecessor] + problem.durations[winner];
                if (newFinish > finish) finish = newFinish;
            }
            int duration = problem.durations[winner];
            bool feasibleFinal = false;
            while (!feasibleFinal) {
                bool feasible = true;
                for (int k = 0; feasible && k < problem.nresources; k++) {
                    for (int t = duration - 1; feasible && t >= 0; t--) {
                        if (problem.requests[winner][k][t] > available[k][finish - duration + t]) {
                            feasible = false;
                            finish++;
                        }
                    }
                }
                if (feasible) feasibleFinal = true;
                if (finish > problem.horizon) {
                    feasibleFinal = false;
                    break;
                }
            }
            if (!feasibleFinal) break; // Skip the rest of this pass
            schedule[winner] = finish;
            // Update remaining resource availabilities
            for (int k = 0; k < problem.nresources; k++) {
                for (int t = 0; t < duration; t++)
                    available[k][finish - duration + t] -= problem.requests[winner][k][t];
            }
        }
        if (schedule.back() >= 0 && schedule.back() < bestMakespan) bestMakespan = schedule.back();
    }
    // For the lower bound we use earliest start of end dummy activity (start is same as finish for this activity)
    return {ef.back(), min(problem.horizon, bestMakespan)};
}
}

#endif //RCPSPT_EXACT_HEURISTICSOLVER_H
