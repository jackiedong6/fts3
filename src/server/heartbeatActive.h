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

#pragma once

#include "active_object.h"
#include "db/generic/SingleDbInstance.h"
#include "threadpool.h"
#include "common/Logger.h"

extern bool stopThreads;

namespace fts3 {
namespace server {

template <typename TRAITS>
class HeartBeatHandlerActive: public TRAITS::ActiveObjectType
{
protected:
    using TRAITS::ActiveObjectType::_enqueue;

public:

    HeartBeatHandlerActive(const std::string& desc = ""):
        TRAITS::ActiveObjectType("HeartBeatHandlerActive", desc) {}

    void beat()
    {

        boost::function<void() > op = boost::bind(&HeartBeatHandlerActive::beat_impl, this);
        this->_enqueue(op);
    }

private:

    void beat_impl(void)
    {
        while (!stopThreads)
            {
                try
                    {
                        db::DBSingleton::instance().getDBObjectInstance()->updateOptimizer();
                        sleep(60);
                    }
                catch(std::exception& e)
                    {
                        FTS3_COMMON_LOGGER_NEWLOG(ERR) << "Process thread HeartBeatHandlerActive " << e.what() << fts3::common::commit;
                        sleep(60);
                    }
                catch(...)
                    {
                        FTS3_COMMON_LOGGER_NEWLOG(ERR) << "Process thread HeartBeatHandlerActive unknown" << fts3::common::commit;
                        sleep(60);
                    }
            }
    }
};


//----------------------------
class HeartBeatActive;
struct HeartBeatTraitsActive
{
    typedef ActiveObject <ThreadPool::ThreadPool> ActiveObjectType;
};

/* -------------------------------------------------------------------------- */

class HeartBeatActive : public HeartBeatHandlerActive <HeartBeatTraitsActive>
{
public:
    explicit HeartBeatActive() {}
    virtual ~HeartBeatActive() {};
};

} // end namespace server
} // end namespace fts3

