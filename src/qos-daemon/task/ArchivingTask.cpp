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

#include "common/Exceptions.h"
#include "common/Logger.h"

#include "ArchivingTask.h"
#include "ArchivingPollTask.h"
#include "WaitingRoom.h"


boost::shared_mutex ArchivingTask::mx;

std::set<std::pair<std::string, std::string>> ArchivingTask::active_urls;


void ArchivingTask::run(const boost::any &)
{
    std::set<std::string> urlSet = ctx.getUrls();
    if (urlSet.empty())
        return;

    ctx.getWaitingRoom().add(new ArchivingPollTask(std::move(*this)));

}
