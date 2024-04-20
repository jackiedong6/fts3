/*
 * Copyright (c) CERN 2013-2016
 *
 * Copyright (c) Members of the EMI Collaboration. 2010-2013
 *  See  http://www.eu-emi.eu/partners for details on the copyright
 *  holders.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef FTS3_OPTIMIZERPAIR_H
#define FTS3_OPTIMIZERPAIR_H


#include <boost/any.hpp>
#include <db/generic/Pair.h>


namespace fts3 {
namespace optimizer {

class OptimizerPair {
public:
    Pair pair;
    Optimizer* optimizer;

    OptimizerPair(const Pair& pair, Optimizer* optimizer) : pair(pair), optimizer(optimizer) {}

    void run(boost::any& ctx) {
        if (ctx.empty()) {
            ctx = 0;
        }
        int &numPairs = boost::any_cast<int &>(ctx);

        if (boost::this_thread::interruption_requested()) {
            return;
        }
        optimizer->runOptimizerForPair(pair);

        numPairs += 1;
    }
};

}
}

#endif // OPTIMIZERPAIR_H_