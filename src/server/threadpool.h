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

#include "task.h"
#include "threadpool_worker.h"
#include "synchronizedqueue.h"

#include "config/serverconfig.h"

#include "common/monitorobject.h"
#include "common/logger.h"

#include <limits>
#include <boost/ptr_container/ptr_vector.hpp>

namespace fts3 {
namespace server {

namespace ThreadPool
{

class ThreadPool : public fts3::common::MonitorObject
{
private:
    typedef SynchronizedQueue<ITask, std::shared_ptr> _queue_t;

public:
    typedef _queue_t::element_type element_type;
    typedef boost::function0<void> op_type;

    ThreadPool(const size_t queueSize, const size_t workerNum);
    virtual ~ThreadPool();

    template<class OP_TYPE>	void enqueue(OP_TYPE& op)
    {
        element_type tptr(new Task<OP_TYPE>(op));
        //FTS3_COMMON_LOGGER_NEWLOG(DEBUG) << tptr->id() << " is waiting for enqueuing" << commit;
        _queue.push(tptr);
        //FTS3_COMMON_LOGGER_NEWLOG(DEBUG) << tptr->id() << " is enqueued" << commit;
    }

    element_type pop(const fts3::common::Timeout& td);

    static ThreadPool& instance()
    {
        static ThreadPool tp
        (
            std::numeric_limits<size_t>::max(),
            fts3::config::theServerConfig().get<size_t> ("ThreadNum")
        );

        return tp;
    }

    void wait();
    void stop();

private:
    _queue_t _queue;

    typedef boost::ptr_vector<Worker> _workers_t;
    _workers_t _workers;
    boost::thread_group _thgrp;
};

struct NoThreads
{
    static NoThreads& instance()
    {
        static NoThreads nt;
        return nt;
    }

    template<class OP_TYPE>	void enqueue(OP_TYPE& op, const std::string& desc = "")
    {
        FTS3_COMMON_LOGGER_NEWLOG(DEBUG) << "Task (" << desc << ") is executed immediately" << fts3::common::commit;
        op();
        //FTS3_COMMON_LOGGER_NEWLOG(DEBUG) << " Task is finished" << commit;
    }
};

} // namespace ThreadPool

} // namespace server
} // namespace fts3

