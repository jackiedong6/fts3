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

#include "process_service_handler.h"
#include "UrlCopyCmd.h"

extern bool stopThreads;
extern time_t retrieveRecords;


namespace fts3 {
namespace server {

template
<
typename TRAITS
>
class ProcessServiceReuseHandler : public ProcessServiceHandler<TRAITS>
{

public:

    /* ---------------------------------------------------------------------- */

    typedef ProcessServiceReuseHandler <TRAITS> OwnType;

    /* ---------------------------------------------------------------------- */

    /** Constructor. */
    ProcessServiceReuseHandler(std::string const & desc = "") : ProcessServiceHandler<TRAITS>(desc) {}

    /* ---------------------------------------------------------------------- */

    /** Destructor */
    virtual ~ProcessServiceReuseHandler() {}

    /* ---------------------------------------------------------------------- */

    void executeTransfer_p()
    {
        boost::function<void() > op = boost::bind(&ProcessServiceReuseHandler::executeTransfer_a, this);
        this->_enqueue(op);
    }

protected:

    using ProcessServiceHandler<TRAITS>::siteResolver;
    using ProcessServiceHandler<TRAITS>::ftsHostName;
    using ProcessServiceHandler<TRAITS>::allowedVOs;
    using ProcessServiceHandler<TRAITS>::infosys;
    using ProcessServiceHandler<TRAITS>::monitoringMessages;
    using ProcessServiceHandler<TRAITS>::execPoolSize;
    using ProcessServiceHandler<TRAITS>::cmd;

    void writeJobFile(const std::string job_id, const std::vector<std::string>& files)
    {
        std::ofstream fout;
        try
            {
                std::vector<std::string>::const_iterator iter;
                std::string filename = "/var/lib/fts3/" + job_id;
                fout.open(filename.c_str(), std::ios::out);
                for (iter = files.begin(); iter != files.end(); ++iter)
                    {
                        fout << *iter << std::endl;
                    }
                fout.close();
            }
        catch(...)
            {
                fout.close();
            }
    }

    std::map<int, std::string> generateJobFile(const std::string& job_id, const std::list<TransferFile> files)
    {
        std::vector<std::string> urls;
        std::map<int, std::string> fileIds;
        std::ostringstream line;

        for (auto it = files.begin(); it != files.end(); ++it)
            {
                PROFILE_SCOPE("executeUrlcopy::for[reuse]");

                fileIds.insert(std::make_pair(it->fileId, it->jobId));

                std::string fileMetadata = UrlCopyCmd::prepareMetadataString(it->fileMetadata);
                if (fileMetadata.empty())
                    fileMetadata = "x";

                std::string bringOnlineToken = it->bringOnlineToken;
                if (bringOnlineToken.empty())
                    bringOnlineToken = "x";

                std::string checksum = it->checksum;
                if (checksum.empty())
                    checksum = "x";

                line << std::fixed << it->fileId << " " << it->sourceSurl << " " << it->destSurl
                                   << " " << checksum
                                   << " " << boost::lexical_cast<long long>(it->userFilesize)
                                   << " " << fileMetadata
                                   << " " << bringOnlineToken;
                urls.push_back(line.str());
                line.str(std::string());
            }

        writeJobFile(job_id, urls);

        return fileIds;
    }

    void getFiles(const std::vector<QueueId>& queues)
    {
        //now get files to be scheduled
        std::map< std::string, std::queue< std::pair<std::string, std::list<TransferFile> > > > voQueues;
        DBSingleton::instance().getDBObjectInstance()->getReadySessionReuseTransfers(queues, voQueues);

        std::map< std::string, std::queue< std::pair<std::string, std::list<TransferFile> > > >::iterator vo_it;

        bool empty = false;

        while(!empty)
            {
                empty = true;
                for (vo_it = voQueues.begin(); vo_it != voQueues.end(); ++vo_it)
                    {
                        std::queue< std::pair<std::string, std::list<TransferFile> > > & vo_jobs = vo_it->second;
                        if (!vo_jobs.empty())
                            {
                                empty = false; //< if we are here there are still some data
                                std::pair< std::string, std::list<TransferFile> > const job = vo_jobs.front();
                                vo_jobs.pop();
                                startUrlCopy(job.first, job.second);
                            }
                    }
            }
    }

    void startUrlCopy(std::string const & job_id, std::list<TransferFile> const & files)
    {
        GenericDbIfce *db = DBSingleton::instance().getDBObjectInstance();
        UrlCopyCmd cmd_builder;

        // Set parameters from the "representative", without using the source and destination url, and other data
        // that is per transfer
        TransferFile const & representative = files.front();
        cmd_builder.setFromTransfer(representative, true);

        // Generate the file containing the list of transfers
        std::map<int, std::string> fileIds = generateJobFile(representative.jobId, files);


        /*check if manual config exist for this pair and vo*/
        std::vector< std::shared_ptr<ShareConfig> > cfgs;
        ConfigurationAssigner cfgAssigner(representative);
        cfgAssigner.assign(cfgs);

        FileTransferScheduler scheduler(representative, cfgs);
        int currentActive = 0;
        if (!scheduler.schedule(currentActive)) return;   /*SET TO READY STATE WHEN TRUE*/

        boost::optional<ProtocolResolver::protocol> user_protocol = ProtocolResolver::getUserDefinedProtocol(representative);

        if (user_protocol.is_initialized())
            {
                cmd_builder.setManualConfig(true);
                cmd_builder.setFromProtocol(user_protocol.get());
            }
        else
            {
                ProtocolResolver::protocol protocol;

                int level = db->getBufferOptimization();
                cmd_builder.setOptimizerLevel(level);
                if(level == 2)
                    {
                        protocol.nostreams = db->getStreamsOptimization(representative.sourceSe, representative.destSe);
                        if(protocol.nostreams == 0)
                            protocol.nostreams = DEFAULT_NOSTREAMS;
                    }
                else
                    {
                        protocol.nostreams = DEFAULT_NOSTREAMS;
                    }

                protocol.urlcopy_tx_to = db->getGlobalTimeout();
                if(protocol.urlcopy_tx_to == 0)
                    {
                        protocol.urlcopy_tx_to = DEFAULT_TIMEOUT;
                    }
                else
                    {
                        cmd_builder.setGlobalTimeout(protocol.urlcopy_tx_to);
                    }
                int secPerMB = db->getSecPerMb();
                if(secPerMB > 0)
                    {
                        cmd_builder.setSecondsPerMB(secPerMB);
                    }

                cmd_builder.setFromProtocol(protocol);
            }

        if (!cfgs.empty())
            {
                FTS3_COMMON_LOGGER_NEWLOG(INFO) << "Check link config for: " << representative.sourceSe << " -> " << representative.destSe << commit;
                ProtocolResolver resolver(representative, cfgs);
                bool protocolExists = resolver.resolve();
                if (protocolExists)
                    {
                        cmd_builder.setManualConfig(true);
                        ProtocolResolver::protocol protocol;

                        protocol.nostreams = resolver.getNoStreams();
                        protocol.tcp_buffer_size = resolver.getTcpBufferSize();
                        protocol.urlcopy_tx_to = resolver.getUrlCopyTxTo();

                        cmd_builder.setFromProtocol(protocol);
                    }

                if (resolver.isAuto())
                    {
                        cmd_builder.setAutoTuned(true);
                    }
            }

        std::string proxy_file = DelegCred::getProxyFile(representative.userDn, representative.credId);
        if (!proxy_file.empty())
            cmd_builder.setProxy(proxy_file);

        std::string oauth_file = fts3::generateOauthConfigFile(db, representative);
        if (!oauth_file.empty())
            cmd_builder.setOAuthFile(oauth_file);

        // Send initial message
        SingleTrStateInstance::instance().sendStateMessage(job_id, -1);

        // Set all to ready, special case for session reuse
        int updatedFiles = db->updateFileStatusReuse(representative, "READY");
        if (updatedFiles <= 0) {
            FTS3_COMMON_LOGGER_NEWLOG(WARNING)
                    << "Transfer " << representative.jobId << " with session reuse enabled"
                    << " not updated. Probably picked by another node" << commit;
            return;
        }

        // Debug level
        unsigned debugLevel = db->getDebugLevel(representative.sourceSe, representative.destSe);
        if (debugLevel > 0)
            {
                cmd_builder.setDebugLevel(debugLevel);
            }

        // Enable monitoring?
        cmd_builder.setMonitoring(monitoringMessages);

        // Infosystem
        cmd_builder.setInfosystem(infosys);

        // FTS3 name
        cmd_builder.setFTSName(ftsHostName);

        // Show user dn
        cmd_builder.setShowUserDn(db->getUserDnVisible());

        // Current number of actives
        cmd_builder.setNumberOfActive(currentActive);


        bool ready = db->isFileReadyStateV(fileIds);

        if (ready)
            {
                std::string params = cmd_builder.generateParameters();
                FTS3_COMMON_LOGGER_NEWLOG(INFO) << "Transfer params: " << cmd << " " << params << commit;
                ExecuteProcess pr(cmd, params);
                /*check if fork failed , check if execvp failed, */
                std::string forkMessage;
                if (-1 == pr.executeProcessShell(forkMessage))
                    {
                        if(forkMessage.empty())
                            {
                                FTS3_COMMON_LOGGER_NEWLOG(ERR) << "Transfer failed to spawn " << commit;
                                db->forkFailedRevertStateV(fileIds);
                            }
                        else
                            {
                                FTS3_COMMON_LOGGER_NEWLOG(ERR) << "Transfer failed to spawn " << forkMessage << commit;
                                db->forkFailedRevertStateV(fileIds);
                            }
                    }
                else
                    {
                        db->setPidForJob(job_id, pr.getPid());
                    }
                std::map<int, std::string>::const_iterator iterFileIds;
                for (iterFileIds = fileIds.begin(); iterFileIds != fileIds.end(); ++iterFileIds)
                    {
                        struct message_updater msg2;
                        if(std::string(job_id).length() <= 37)
                            {
                                strncpy(msg2.job_id, std::string(job_id).c_str(), sizeof(msg2.job_id));
                                msg2.job_id[sizeof(msg2.job_id) - 1] = '\0';
                                msg2.file_id = iterFileIds->first;
                                msg2.process_id = (int) pr.getPid();
                                msg2.timestamp = milliseconds_since_epoch();
                                ThreadSafeList::get_instance().push_back(msg2);
                            }
                        else
                            {
                                FTS3_COMMON_LOGGER_NEWLOG(ERR) << "Message length overun" << std::string(job_id).length() << commit;
                            }
                    }
            }
    }

    void executeUrlcopy()
    {
        try
            {
                std::vector<QueueId> queues;

                try
                    {
                        DBSingleton::instance().getDBObjectInstance()->getQueuesWithSessionReusePending(queues);
                    }
                catch (std::exception& e)
                    {
                        //try again if deadlocked
                        sleep(1);
                        try
                            {
                                queues.clear();
                                DBSingleton::instance().getDBObjectInstance()->getQueuesWithSessionReusePending(queues);
                            }
                        catch (std::exception& e)
                            {
                                FTS3_COMMON_LOGGER_NEWLOG(ERR) << "Exception in process_service_handler " << e.what() << commit;
                            }
                        catch (...)
                            {
                                FTS3_COMMON_LOGGER_NEWLOG(ERR) << "Exception in process_service_handler!" << commit;
                            }
                    }
                catch (...)
                    {
                        //try again if deadlocked
                        sleep(1);
                        try
                            {
                                queues.clear();
                                DBSingleton::instance().getDBObjectInstance()->getQueuesWithSessionReusePending(queues);
                            }
                        catch (std::exception& e)
                            {
                                FTS3_COMMON_LOGGER_NEWLOG(ERR) << "Exception in process_service_handler " << e.what() << commit;
                            }
                        catch (...)
                            {
                                FTS3_COMMON_LOGGER_NEWLOG(ERR) << "Exception in process_service_handler!" << commit;
                            }
                    }

                if(queues.empty()) return;

                getFiles(queues);
            }
        catch (std::exception& e)
            {
                FTS3_COMMON_LOGGER_NEWLOG(ERR) << "Exception in process_service_handler " << e.what() << commit;
            }
        catch (...)
            {
                FTS3_COMMON_LOGGER_NEWLOG(ERR) << "Exception in process_service_handler!" << commit;
            }
    }

    /* ---------------------------------------------------------------------- */

    void executeTransfer_a()
    {
        static bool drainMode = false;

        while (true)
            {
                retrieveRecords = time(0);

                try
                    {
                        if (stopThreads)
                            {
                                return;
                            }

                        if (DrainMode::getInstance())
                            {
                                if (!drainMode)
                                    FTS3_COMMON_LOGGER_NEWLOG(INFO) << "Set to drain mode, no more transfers for this instance!" << commit;
                                drainMode = true;
                                sleep(15);
                                continue;
                            }
                        else
                            {
                                drainMode = false;
                            }

                        executeUrlcopy();
                    }
                catch (std::exception& e)
                    {
                        FTS3_COMMON_LOGGER_NEWLOG(ERR) << "Exception in process_service_handler " << e.what() << commit;
                        sleep(2);
                    }
                catch (...)
                    {
                        FTS3_COMMON_LOGGER_NEWLOG(ERR) << "Exception in process_service_handler!" << commit;
                        sleep(2);
                    }
                sleep(2);
            } /*end while*/
    }

    /* ---------------------------------------------------------------------- */
    struct TestHelper
    {

        TestHelper()
            : loopOver(false)
        {
        }

        bool loopOver;
    }
    _testHelper;
};

} // end namespace server
} // end namespace fts3

