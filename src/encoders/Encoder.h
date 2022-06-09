/***************************************************************************************[Encoder.h]
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

#ifndef RCPSPT_EXACT_ENCODER_H
#define RCPSPT_EXACT_ENCODER_H

#include "../Problem.h"

namespace RcpsptExact {

/**
 * Abstract base class for all encoders.
 */
class Encoder {
public:
    virtual ~Encoder();

    /**
     * TODO
     *
     * @return
     */
    bool calcTimeWindows();

protected:
    Encoder(Problem& p, pair<int,int> bounds);

    Problem& problem;

    int LB, UB; // The lower and upper bounds for the makespan that are currently being used

    vector<int> ES, EC, LS, LC; // For each activity: earliest start, earliest close, latest start, and latest close time
};
}

#endif //RCPSPT_EXACT_ENCODER_H
