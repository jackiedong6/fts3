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


#include <map>

#include "common/Uri.h"
#include "db/generic/SingleDbInstance.h"
#include "server/DrainMode.h"

#include "CDMIFetchQosTransition.h"
#include "../task/QoSTransitionTask.h"
#include "../task/CDMIPollTask.h"


void CDMIFetchQosTransition::fetch()
{
    FTS3_COMMON_LOGGER_NEWLOG(INFO) << "CDMIFetchQosTransition starting" << commit;

    while (!boost::this_thread::interruption_requested()) {
        try {
            boost::this_thread::sleep(boost::posix_time::seconds(10));
            //if we drain a host, no need to check if url_copy are reporting being alive
            if (fts3::server::DrainMode::instance()) {
                FTS3_COMMON_LOGGER_NEWLOG(INFO) << "Set to drain mode, no more checking stage-in files for this instance!" << commit;
                continue;
            }

            std::map<std::string, CDMIQosTransitionContext> tasks;
            std::vector<QosTransitionOperation> files;

            db::DBSingleton::instance().getDBObjectInstance()->getFilesForQosTransition(files);

            for (auto it_f = files.begin(); it_f != files.end(); ++it_f)
            {
                std::string storage = Uri::parse(it_f->surl).host;

                //std::cerr << "Storage detected: " << storage << std::endl;

                auto it_t = tasks.find(storage);
                if (it_t == tasks.end()) {
                    tasks.insert(std::make_pair(storage, CDMIQosTransitionContext( QoSServer::instance())));
                    it_t = tasks.find(storage);
                }
                it_t->second.add(*it_f);
            }


            for (auto it_t = tasks.begin(); it_t != tasks.end(); ++it_t)
            {
                try
                {
                	//for (auto it = it_t->second.getSurls().begin(); it != it_t->second.getSurls().end(); ++it) {
                	//	      std::cerr << "Printing surl in fetch: " << std::get<0>(*it) << std::endl;
                	//	  }
                	//std::cerr << "Checking the context: " << it_t->second.getSurls() << std::endl;
                    threadpool.start(new QoSTransitionTask(it_t->second));
                }
                catch(UserError const & ex)
                {
                    FTS3_COMMON_LOGGER_NEWLOG(WARNING) << ex.what() << commit;
                }
                catch(...)
                {
                    FTS3_COMMON_LOGGER_NEWLOG(ERR) << "Unknown exception, continuing to see..." << commit;
                }
            }

        }
        catch (const std::exception& e) {
            FTS3_COMMON_LOGGER_NEWLOG(ERR) << "CDMIFetchQosTransition " << e.what() << commit;
        }
        catch (const boost::thread_interrupted&) {
            FTS3_COMMON_LOGGER_NEWLOG(INFO) << "CDMIFetchQosTransition interruption requested" << commit;
            break;
        }
        catch (...) {
            FTS3_COMMON_LOGGER_NEWLOG(ERR) << "CDMIFetchQosTransition Fatal error (unknown origin)" << commit;
        }
    }

    FTS3_COMMON_LOGGER_NEWLOG(INFO) << "CDMIFetchQosTransition exiting" << commit;
}
