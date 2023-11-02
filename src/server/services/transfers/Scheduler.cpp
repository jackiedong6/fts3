#include "Scheduler.h"

using namespace fts3::common;

namespace fts3 {
namespace server {

Scheduler::SchedulerFunction Scheduler::getSchedulingFunction(string algorithm) {
    Scheduler::SchedulerFunction function;

    switch (algorithm) {
        case Scheduler::RANDOMIZED_ALGORITHM:
            function = &Scheduler::doRandomizedSchedule;
            break;
        case Scheduler::DEFICIT_ALGORITHM:
            function = &Scheduler::doDeficitSchedule;
            break;
        case default:
            // Use randomized algorithm as default
            function = &Scheduler::doRandomizedSchedule;
            break;
    }

    return function;
}

std::map<std::string, std::list<TransferFile>> Scheduler::doRandomizedSchedule(
    std::map<Pair, int> &slotsPerLink, 
    std::vector<QueueId> &queues, 
    int availableUrlCopySlots
){
    std::map<std::string, std::list<TransferFile> > scheduledFiles;
    std::vector<QueueId> unschedulable;

    // Apply VO shares at this level. Basically, if more than one VO is used the same link,
    // pick one each time according to their respective weights
    queues = applyVoShares(queues, unschedulable);
    // Fail all that are unschedulable
    failUnschedulable(unschedulable);

    if (queues.empty())
        return scheduledFiles;

    auto db = DBSingleton::instance().getDBObjectInstance();

    time_t start = time(0);
    db->getReadyTransfers(queues, scheduledFiles); // TODO: move this out of db?
    time_t end =time(0);
    FTS3_COMMON_LOGGER_NEWLOG(INFO) << "DBtime=\"TransfersService\" "
                                    << "func=\"doRandomizedSchedule\" "
                                    << "DBcall=\"getReadyTransfers\" " 
                                    << "time=\"" << end - start << "\"" 
                                    << commit;

    if (voQueues.empty())
        return;

    return scheduledFiles;
}

std::map<std::string, std::list<TransferFile>> Scheduler::doDeficitSchedule(
    std::map<Pair, int> &slotsPerLink, 
    std::vector<QueueId> &queues, 
    int availableUrlCopySlots
){
    std::map<std::string, std::list<TransferFile> > scheduledFiles;

    if (queues.empty())
        return scheduledFiles;

    // (1) For each VO, fetch activity share.
    //     We do this here because this produces a mapping between each vo and all activities in that vo,
    //     which we need for later steps. Hence this reduces redundant queries into the database.
    //     The activity share of each VO also does not depend on the pair.
    std::map<std::string, std::map<std::string, double>> voActivityShare;
    for (auto i = queues.begin(); i != queues.end(); i++) {
        if (voActivityShare.count(i->voName) > 0) {
            // VO already exists in voActivityShare; don't need to fetch again
            continue;
        }
        // Fetch activity share for that VO
        std::map<std::string, double> activityShare = getActivityShareConf(i->voName);
        voActivityShare[i->voName] = activityShare;
    }
    
    std::map<Scheduler::LinkVoActivityKey, double> allDeficits;

    for (auto i = slotsPerLink.begin(); i != slotsPerLink.end(); i++) {
        const Pair &p = i->first;
        const int maxSlots = i->second;

        // (2) Compute the number of active / submitted transfers for each activity in each vo,
        //     as well as the number of pending (submitted) transfers for each activity in each vo.
        //     We do this here because we need this for computing should-be-allocated slots.
        std::map<std::string, std::map<std::string, int>> queueActiveCounts = computeActiveCounts(p->source, p->destination, voActivityShare);
        std::map<std::string, std::map<std::string, int>> queueSubmittedCounts = computeSubmittedCounts(p->source, p->destination, voActivityShare);

        // (3) Compute the number of should-be-allocated slots.
        std::map<std::string, std::map<std::string, int>> queueShouldBeAllocated = computeShouldBeSlots(
            p, maxSlots,
            voActivityShare,
            queueActiveCounts,
            queueSubmittedCounts);

        // (4) Compute deficit.
        std::map<std::string, std::map<std::string, int>> deficits = computeDeficits(queueActiveCounts, queueShouldBeAllocated);

        // (5) Store deficit into allDeficits.
        for (auto j = deficits.begin(); j != deficits.end(); j++) {
            std::string voName = j->first;
            std::map<std::string, int> activityDeficits = j->second;
            for (auto k = activityDeficits.begin(); k != deficits.end(); k++) {
                std::string activityName = k->first;
                int deficit = k->second;

                LinkVoActivityKey key = std::make_tuple(p->source, p->destination, voName, activityName);
                allDeficits[key] = deficit;
            }
        }
    }

    // (6) Schedule using priority queue on deficits.
    // TODO

    return scheduledFiles;
}

/**
 * Compute the number of active transfers for each activity in each vo for the pair.
 * @param src Source node
 * @param dest Destination node
 * @param voActivityShare Maps each VO to a mapping between each of its activities to the activity's weight
*/
std::map<std::string, std::map<std::string, long long>> computeActiveCounts(
    std::string src,
    std::string dest,
    std::map<std::string, std::map<std::string, double>> &voActivityShare
){
    auto db = DBSingleton::instance().getDBObjectInstance(); // TODO: fix repeated db
    std::map<std::string, std::map<std::string, long long>> result;

    for (auto i = voActivityShare.begin(); i != voActivityShare.end(); i++) {
        std::string voName = i->first;
        result[voName] = db->getActiveCountInActivity(src, dest, vo);
    }

    return result;
}

/**
 * Compute the number of submitted transfers for each activity in each vo for the pair.
 * @param src Source node
 * @param dest Destination node
 * @param voActivityShare Maps each VO to a mapping between each of its activities to the activity's weight
*/
std::map<std::string, std::map<std::string, long long>> computeSubmittedCounts(
    std::string src,
    std::string dest,
    std::map<std::string, std::map<std::string, double>> &voActivityShare
){
    auto db = DBSingleton::instance().getDBObjectInstance(); // TODO: fix repeated db
    std::map<std::string, std::map<std::string, long long>> result;

    for (auto i = voActivityShare.begin(); i != voActivityShare.end(); i++) {
        std::string voName = i->first;
        result[voName] = db->getSubmittedCountInActivity(src, dest, vo);
    }

    return result;
}

/**
 * Compute the number of should-be-allocated slots.
 * @param p Pair of src-dest nodes.
 * @param maxPairSlots Max number of slots given to the pair, as determined by allocator.
 * @param voActivityShare Maps each VO to a mapping between each of its activities to the activity's weight.
 * @param queueActiveCounts Maps each VO to a mapping between each of its activities to the activity's number of active slots.
 * @param queueSubmittedCounts Maps each VO to a mapping between each of its activities to the activity's number of submitted slots.
*/
std::map<std::string, std::map<std::string, int>> computeShouldBeSlots(
    Pair &p,
    int maxPairSlots,
    std::map<std::string, std::map<std::string, double>> &voActivityShare,
    std::map<std::string, std::map<std::string, int>> &queueActiveCounts,
    std::map<std::string, std::map<std::string, int>> &queueSubmittedCounts
){
    FTS3_COMMON_LOGGER_NEWLOG(INFO) << "Scheduler: computing should-be-allocated slots" << commit;

    std::map<std::string, std::map<std::string, int>> result;

    // (1) Fetch weights of all vo's in that pair
    std::vector<ShareConfig> shares = DBSingleton::instance().getDBObjectInstance()->getShareConfig(p.source, p.destination);
    std::map<std::string, double> voWeights;
    for (auto j = shares.begin(); j != shares.end(); j++) {
        voWeights[j->vo] = j->weight;
    }

    // (2) Assign slots to vo's
    // TODO: this needs to know the number of active + pending transfers for each VO
    std::map<std::string, int> voShouldBeSlots = assignShouldBeSlotsToVos(voWeights, maxPairSlots, queueActiveCounts, queueSubmittedCounts);

    // (3) Assign slots of each vo to its activities
    for (auto j = voShouldBeSlots.begin(); j != voShouldBeSlots.end(); j++) {
        std::string voName = j->first;
        int voMaxSlots = j->second;

        std::map<std::string, double> activityWeights = voActivityShare[voName];
        std::map<std::string, int> activityShouldBeSlots = assignShouldBeSlotsToActivities(activityWeights, voMaxSlots, queueActiveCounts, queueSubmittedCounts);

        // Add to result
        for (auto k = activityShouldBeSlots.begin(); k != activityShouldBeSlots.end(); k++) {
            std::string activityName = k->first;
            int activitySlots = k->second;

            result[voName][activityName] = activitySlots;
        }
    }

    return result;
}

/**
 * Compute the deficit for each queue in a pair.
 * @param queueActiveCounts Number of active slots for each activity in each VO.
 * @param queueShouldBeAllocated Number of should-be-allocated slots for each activity in each VO.
*/
std::map<std::string, std::map<std::string, int>> computeDeficits(
    std::map<std::string, std::map<std::string, int>> &queueActiveCounts,
    std::map<std::string, std::map<std::string, int>> &queueShouldBeAllocated
){
    std::map<std::string, std::map<std::string, int>> result;

    for (auto i = queueActiveCounts.begin(); i != queueActiveCounts.end(); i++) {
        std::string voName = i->first;
        std::map<std::string, int> activityActiveCount = i->second;

        for (auto j = activityActiveCount.begin(); j != activityActiveCount.end(); j++) {
            std::string activityName = j->first;
            int activeCount = j->second;
            int shouldBeAllocatedCount = queueShouldBeAllocated[voName][activityName];

            result[voName][activityName] = shouldBeAllocatedCount - activeCount;
        }
    }

    return result;
}

// Helper functions:

/**
 * Transfers in uneschedulable queues must be set to fail
 */
void failUnschedulable(const std::vector<QueueId> &unschedulable)
{
    Producer producer(config::ServerConfig::instance().get<std::string>("MessagingDirectory"));

    std::map<std::string, std::list<TransferFile> > voQueues;
    DBSingleton::instance().getDBObjectInstance()->getReadyTransfers(unschedulable, voQueues);

    for (auto iterList = voQueues.begin(); iterList != voQueues.end(); ++iterList) {
        const std::list<TransferFile> &transferList = iterList->second;
        for (auto iterTransfer = transferList.begin(); iterTransfer != transferList.end(); ++iterTransfer) {
            events::Message status;

            status.set_transfer_status("FAILED");
            status.set_timestamp(millisecondsSinceEpoch());
            status.set_process_id(0);
            status.set_job_id(iterTransfer->jobId);
            status.set_file_id(iterTransfer->fileId);
            status.set_source_se(iterTransfer->sourceSe);
            status.set_dest_se(iterTransfer->destSe);
            status.set_transfer_message("No share configured for this VO");
            status.set_retry(false);
            status.set_errcode(EPERM);

            producer.runProducerStatus(status);
        }
    }
}

} // end namespace server
} // end namespace fts3