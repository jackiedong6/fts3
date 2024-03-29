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

#include <event2/event.h>
#include <event2/util.h>

#include "HeartBeat.h"

#include "common/Logger.h"
#include "config/ServerConfig.h"
#include "db/generic/SingleDbInstance.h"
#include "server/DrainMode.h"
#include "server/Server.h"

using namespace fts3::common;
using namespace fts3::config;

namespace fts3 {
namespace server {

time_t retrieveRecords = time(0);
time_t updateRecords = time(0);
time_t stallRecords = time(0);


HeartBeat::HeartBeat(): BaseService("HeartBeat"), index(0), count(0), start(0), end(0)
{
}

struct HeartBeatCallbackData {
    HeartBeat* instance;
    boost::posix_time::time_duration interval;
};

static callback(evutil_socket_t fd, short event, void*arg)
{
    struct event *heartBeatEvent = (struct event *)arg;
    struct timeval failureSleepTime = {1, 0}; // If heartBeat update fails, we sleep for one second
    // TODO: Remove redundant call for pulling configuration
    struct timeval heartBeatTimeInterval = {ServerConfig::instance().get<int>("HeartBeatInterval"), 0};
    try
    {
        //if we drain a host, we need to let the other hosts know about it, hand-over all files to the rest
        if (DrainMode::instance())
        {
            FTS3_COMMON_LOGGER_NEWLOG(INFO)
                    << "Set to drain mode, no more transfers for this instance!"
                    << commit;
            struct timeval drainSleepTime = {15, 0}; // If heartBeat update fails, we sleep for one second
            evtimer_add(heartBeatEvent, &drainSleepTime);
            return;
        }

        if (criticalThreadExpired(retrieveRecords, updateRecords, stallRecords))
        {
            FTS3_COMMON_LOGGER_NEWLOG(CRIT)
                    << "One of the critical threads looks stalled"
                    << commit;
            // Note: Would be nice to get the pstack output in the log
            orderedShutdown();
        }

        std::string serviceName = "fts_server";

        db::DBSingleton::instance().getDBObjectInstance()
                ->updateHeartBeat(&index, &count, &start, &end, serviceName);
        FTS3_COMMON_LOGGER_NEWLOG(DEBUG)
                << "Systole: host " << index << " out of " << count
                << " [" << start << ':' << end << ']'
                << commit;

        // It the update was successful, we sleep here
        // If it wasn't, only one second will pass until the next retry
        evtimer_add(timer_event, &heartBeatTimeInterval);
    }
    catch (const boost::thread_interrupted&)
    {
        throw;
    }
    catch (const std::exception& e)
    {
        FTS3_COMMON_LOGGER_NEWLOG(ERR) << "Hearbeat failed: " << e.what() << commit;
        evtimer_add(heartBeatEvent, &failureSleepTime);
    }
    catch (...)
    {
        evtimer_add(heartBeatEvent, &failureSleepTime);
        FTS3_COMMON_LOGGER_NEWLOG(ERR) << "Hearbeat failed " << commit;
    }
}

void HeartBeat::runService()
{
    typedef boost::posix_time::time_duration TDuration;
    TDuration heartBeatInterval, heartBeatGraceInterval;

    try {
        heartBeatInterval = ServerConfig::instance().get<TDuration>("HeartBeatInterval");
        heartBeatGraceInterval = ServerConfig::instance().get<TDuration>("HeartBeatGraceInterval");
        if (heartBeatInterval >= heartBeatGraceInterval) {
            FTS3_COMMON_LOGGER_NEWLOG(CRIT)
                << "HeartBeatInterval >= HeartBeatGraceInterval. Can not work like this" << commit;
            _exit(1);
        }
    }
    catch (...) {
        FTS3_COMMON_LOGGER_NEWLOG(CRIT) << "Could not get the heartbeat interval" << commit;
        _exit(1);
    }
    FTS3_COMMON_LOGGER_NEWLOG(INFO) << "Using heartbeat interval " << heartBeatInterval << commit;
    FTS3_COMMON_LOGGER_NEWLOG(INFO) << "Using heartbeat grace interval " << heartBeatGraceInterval << commit;

    struct heartBeatCallbackData = {self, };
    struct event* heartBeatEvent = evtimer_new(baseEvent, callback, event_self_cbarg());

    // Start the event right away and reschedule based off of heartBeatInterval within the callback
    struct timeval startTime  = {0, 0};
    evtimer_add(heartBeatEvent, &startTime);
    event_base_dispatch(baseEvent);
    event_free(heartBeatEvent);
}


bool HeartBeat::criticalThreadExpired(time_t retrieveRecords, time_t updateRecords ,time_t stallRecords)
{
    double diffTime = 0.0;

    diffTime = std::difftime(std::time(NULL), retrieveRecords);
    if (diffTime > 7200)
    {
        FTS3_COMMON_LOGGER_NEWLOG(CRIT)
                << "Wall time passed retrieve records: " << diffTime
                << " secs " << commit;
        return true;
    }

    diffTime = std::difftime(std::time(NULL), updateRecords);
    if (diffTime > 7200)
    {
        FTS3_COMMON_LOGGER_NEWLOG(CRIT)
                << "Wall time passed update records: " << diffTime
                << " secs " << commit;
        return true;
    }

    diffTime = std::difftime(std::time(NULL), stallRecords);
    if (diffTime > 10000)
    {
        FTS3_COMMON_LOGGER_NEWLOG(CRIT)
                << "Wall time passed stallRecords and cancelation thread exited: "
                << diffTime << " secs " << commit;
        return true;
    }

    return false;
}

void HeartBeat::orderedShutdown()
{
    FTS3_COMMON_LOGGER_NEWLOG(INFO) << "Stopping other threads..." << commit;
    // Give other threads a chance to finish gracefully
    Server::instance().stop();
    boost::this_thread::sleep(boost::posix_time::seconds(30));

    FTS3_COMMON_LOGGER_NEWLOG(INFO) << "Exiting" << commit;
    exit(1);
}

bool HeartBeat::isLeadNode(bool bypassDraining)
{   
    if (DrainMode::instance() && !bypassDraining) {
        return false;
    } 

    return index == 0;
}

} // end namespace server
} // end namespace fts3
