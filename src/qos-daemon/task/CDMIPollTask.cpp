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

#include "common/Logger.h"
#include "CDMIPollTask.h"

void CDMIPollTask::run(const boost::any&)
{
	FTS3_COMMON_LOGGER_NEWLOG(INFO) << "CDMIPollTask starting" << commit;

	// Retrieve all files with a state of QOS_REQUEST_SUBMITTED
	std::vector<QosTransitionOperation> files;
	ctx.cdmiGetFilesForQosRequestSubmitted(files, "QOS_REQUEST_SUBMITTED");

	int maxPollRetries = fts3::config::ServerConfig::instance().get<int>("StagingPollRetries");
	bool anyPending = false;

	for (const auto& file: files)
	{
	    try {
            // Check QoS of file
            std::string fileQoS = gfal2QoS.getFileQoS(file.surl, file.token);

            FTS3_COMMON_LOGGER_NEWLOG(DEBUG) << "CDMI check QoS of file " << file.surl << ": "
                                             << "(requested_target_qos=" << file.target_qos << ", actual_qos=" << fileQoS << ")" << commit;

            if (fileQoS == file.target_qos) {
                FTS3_COMMON_LOGGER_NEWLOG(INFO) << "CDMI check QoS of file " << file.surl << ": [Success] File was successfully transitioned" << commit;
                ctx.cdmiUpdateFileStateToFinished(file.jobId, file.fileId);
            } else {
                std::string targetQoS = gfal2QoS.getFileTargetQoS(file.surl, file.token);

                if (targetQoS == file.target_qos) {
                    FTS3_COMMON_LOGGER_NEWLOG(INFO) << "CDMI check QoS of file " << file.surl << ": [Pending] File has not been transitioned yet" << commit;
                    anyPending = true;
                } else if (targetQoS.empty()) {
                    // No target QoS found --> transition might have finished
                    fileQoS = gfal2QoS.getFileQoS(file.surl, file.token);

                    if (fileQoS == file.target_qos) {
                        FTS3_COMMON_LOGGER_NEWLOG(INFO) << "CDMI check QoS of file " << file.surl << ": [Success] File was successfully transitioned" << commit;
                        ctx.cdmiUpdateFileStateToFinished(file.jobId, file.fileId);
                    } else {
                        FTS3_COMMON_LOGGER_NEWLOG(INFO) << "CDMI check QoS of file " << file.surl << ": [Failed] Transition has finished but did not reach target QoS "
                                                        << "(requested_target_qos=" << file.target_qos << ", actual_qos=" << fileQoS << "). "
                                                        << "Marking file as FAILED " << commit;
                        ctx.cdmiUpdateFileStateToFailed(file.jobId, file.fileId);
                    }
                } else {
                    FTS3_COMMON_LOGGER_NEWLOG(INFO) << "CDMI check QoS of file " << file.surl << ": [Failed] Target QoS has changed to a different value "
                                                    << "(requested_target_qos=" << file.target_qos << ", actual_target_qos=" << targetQoS << "). "
                                                    << "Marking file as FAILED" << commit;
                    ctx.cdmiUpdateFileStateToFailed(file.jobId, file.fileId);
                }
            }
        } catch (const Gfal2Exception &ex) {
	        if (ctx.incrementErrorCountForSurl(file.surl) == maxPollRetries) {
                FTS3_COMMON_LOGGER_NEWLOG(INFO) << "CDMI POLL check QoS of file " << file.surl << " exceeded the max configured limit retry. "
                                                << "File has not been transitioned yet. Marking file as FAILED" << commit;
                ctx.cdmiUpdateFileStateToFailed(file.jobId, file.fileId);
	        }
	    }
	}

    // If not all transitions are finished, schedule a new poll
    if (anyPending) {
        time_t interval = getPollInterval(++nPolls), now = time(NULL);
        wait_until = now + interval;

        FTS3_COMMON_LOGGER_NEWLOG(INFO) << "CDMIPollTask resolution: Next QoS check attempt in " << interval << " seconds" << commit;
        ctx.getWaitingRoom().add(new CDMIPollTask(std::move(*this)));
    } else {
        FTS3_COMMON_LOGGER_NEWLOG(INFO) << "CDMIPollTask resolution: All tracked QoS transitions finished" << commit;
    }
}
