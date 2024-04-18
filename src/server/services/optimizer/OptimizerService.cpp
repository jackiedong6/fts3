/*
 * Copyright (c) CERN 2013-2015
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

#include <config/ServerConfig.h>
#include <monitoring/msg-ifce.h>
#include "OptimizerService.h"
#include "Optimizer.h"

#include "common/ThreadPool.h"
#include "db/generic/SingleDbInstance.h"
#include <chrono> 

using namespace fts3::common;
namespace fts3 {
namespace server {

using optimizer::Optimizer;
using optimizer::OptimizerCallbacks;
using optimizer::OptimizerDataSource;
using optimizer::PairState;

class OptimizerNotifier : public OptimizerCallbacks {
protected:
    bool enabled;
    Producer msgProducer;

public:
    OptimizerNotifier(bool enabled, const std::string &msgDir): enabled(enabled), msgProducer(msgDir)
    {}

    OptimizerNotifier(const OptimizerNotifier &) = delete;

    void notifyDecision(const Pair &pair, int decision, const PairState &current,
        int diff, const std::string &rationale)
    {
        if (!enabled) {
            return;
        }

        // Broadcast the decision
        OptimizerInfo msg;

        msg.source_se = pair.source;
        msg.dest_se = pair.destination;

        msg.timestamp = millisecondsSinceEpoch();

        msg.throughput = current.throughput;
        msg.avgDuration = current.avgDuration;
        msg.successRate = current.successRate;
        msg.retryCount = current.retryCount;
        msg.activeCount = current.activeCount;
        msg.queueSize = current.queueSize;
        msg.ema = current.ema;
        msg.filesizeAvg = current.filesizeAvg;
        msg.filesizeStdDev = current.filesizeStdDev;
        msg.connections = decision;
        msg.rationale = rationale;

        MsgIfce::getInstance()->SendOptimizer(msgProducer, msg);
    }
};


OptimizerService::OptimizerService(HeartBeat *beat): BaseService("OptimizerService"), beat(beat)
{
}


void OptimizerService::runService()
{
    typedef boost::posix_time::time_duration TDuration;

    auto optimizerInterval = config::ServerConfig::instance().get<TDuration>("OptimizerInterval");
    auto optimizerSteadyInterval = config::ServerConfig::instance().get<TDuration>("OptimizerSteadyInterval");
    auto maxNumberOfStreams = config::ServerConfig::instance().get<int>("OptimizerMaxStreams");
    auto maxSuccessRate = config::ServerConfig::instance().get<int>("OptimizerMaxSuccessRate");
    auto lowSuccessRate = config::ServerConfig::instance().get<int>("OptimizerLowSuccessRate");
    auto baseSuccessRate = config::ServerConfig::instance().get<int>("OptimizerBaseSuccessRate");

    auto emaAlpha = config::ServerConfig::instance().get<double>("OptimizerEMAAlpha");
    auto increaseStep = config::ServerConfig::instance().get<int>("OptimizerIncreaseStep");
    auto increaseAggressiveStep = config::ServerConfig::instance().get<int>("OptimizerAggressiveIncreaseStep");
    auto decreaseStep = config::ServerConfig::instance().get<int>("OptimizerDecreaseStep");
    auto optimizerPoolSize = config::ServerConfig::instance().get<int>("OptimizerThreadPool");
    
    OptimizerNotifier optimizerCallbacks(
        config::ServerConfig::instance().get<bool>("MonitoringMessaging"),
        config::ServerConfig::instance().get<std::string>("MessagingDirectory")
    );
    
    ThreadPool<Optimizer> optimizerPool(optimizerPoolSize);        
    auto end = std::chrono::system_clock::now();
    OptimizerDataSource *dataSource = db::DBSingleton::instance().getDBObjectInstance()->getOptimizerDataSource();
    while (!boost::this_thread::interruption_requested()) {
        try {
            boost::this_thread::sleep(optimizerInterval);
            if (beat->isLeadNode()) {
                FTS3_COMMON_LOGGER_NEWLOG(INFO) << "leadNode" << commit;
                std::list<Pair> pairs = dataSource->getActivePairs();
                int initial_size = pairs.size();
                // Make sure the order is always the same
                // See FTS-1094
                pairs.sort();
                auto start = std::chrono::system_clock::now();
                for (auto i = pairs.begin(); i != pairs.end(); ++i) {
                    OptimizerDataSource *ds = db::DBSingleton::instance().getDBObjectInstance()->getOptimizerDataSource();

                    Optimizer *optimizer = new Optimizer(ds, &optimizerCallbacks, *i, {});
                    optimizer->setSteadyInterval(optimizerSteadyInterval);
                    optimizer->setMaxNumberOfStreams(maxNumberOfStreams);
                    optimizer->setMaxSuccessRate(maxSuccessRate);
                    optimizer->setLowSuccessRate(lowSuccessRate);
                    optimizer->setBaseSuccessRate(baseSuccessRate);
                    optimizer->setEmaAlpha(emaAlpha);
                    optimizer->setStepSize(increaseStep, increaseAggressiveStep, decreaseStep);
                    FTS3_COMMON_LOGGER_NEWLOG(DEBUG) << "Submitting Optimizer to thread pool: Pair - " << *i << commit;
                    optimizerPool.start(optimizer);
                    FTS3_COMMON_LOGGER_NEWLOG(DEBUG) << "Optimizer submitted: Pair - " << *i << commit;
                }
                optimizerPool.join();
                int numPairs = optimizerPool.reduce(std::plus<int>());
                auto end = std::chrono::system_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
                FTS3_COMMON_LOGGER_NEWLOG(INFO) << "runOptimizerForPair duration: " <<  duration << commit;

                FTS3_COMMON_LOGGER_NEWLOG(INFO) << "Optimizerpool processed: " << initial_size
                << " files (" << numPairs << " have been proceesed)" << commit;

            }
        }
        catch (const boost::thread_interrupted&) {
            FTS3_COMMON_LOGGER_NEWLOG(ERR) << "Interruption requested in OptimizerService:runService" << commit;
            optimizerPool.interrupt();
            optimizerPool.join();
        }
        catch (std::exception &e) {
            FTS3_COMMON_LOGGER_NEWLOG(ERR) << "Process thread OptimizerService " << e.what() <<
            fts3::common::commit;
        }
        catch (...) {
            FTS3_COMMON_LOGGER_NEWLOG(ERR) << "Process thread OptimizerService unknown" << fts3::common::commit;
        }
        }
}


} // end namespace server
} // end namespace fts3
