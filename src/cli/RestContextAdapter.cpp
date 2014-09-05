/*
 * RestContextAdapter.cpp
 *
 *  Created on: 15 Aug 2014
 *      Author: simonm
 */

#include "RestContextAdapter.h"

#include "rest/RestSubmission.h"
#include "rest/HttpRequest.h"
#include "rest/ResponseParser.h"

#include "delegation/RestDelegator.h"

#include <iostream>
#include <sstream>

#include <boost/lexical_cast.hpp>

namespace fts3
{
namespace cli
{

void RestContextAdapter::getInterfaceDeatailes()
{
    std::stringstream ss;
    HttpRequest http (endpoint, capath, proxy, ss);
    http.get();

    ResponseParser parser(ss);

    version += parser.get("api.major");
    version += "." + parser.get("api.minor");
    version += "." + parser.get("api.patch");

    interface = version;
    metadata  = "fts3-rest-" + version;

    schema += parser.get("schema.major");
    schema += "." + parser.get("schema.minor");
    schema += "." + parser.get("schema.patch");
}

std::vector<JobStatus> RestContextAdapter::listRequests (std::vector<std::string> const & statuses, std::string const & dn, std::string const & vo, std::string const & /*source*/, std::string const & /*destination*/)
{
    // prefix will be holding '?' at the first concatenation and then '&'
    char prefix = '?';
    std::string url = endpoint + "/jobs";

    if (!dn.empty())
        {
            url += prefix;
            url += "user_dn=";
            url += dn;
            prefix = '&';
        }

    if (!vo.empty())
        {
            url += prefix;
            url += "vo_name=";
            url += vo;
            prefix = '&';
        }

    if (!statuses.empty())
        {
            url += prefix;
            url += "job_state=";
            url += *statuses.begin();
            prefix = '&';
        }

    std::stringstream ss;
    ss << "{\"jobs\":";
    HttpRequest http (url, capath, proxy, ss);
    http.get();
    ss << '}';

    ResponseParser parser(ss);
    return parser.getJobs("jobs");
}

std::vector< std::pair<std::string, std::string> > RestContextAdapter::cancel(std::vector<std::string> const & jobIds)
{
    std::vector<std::string>::const_iterator itr;

    std::vector< std::pair< std::string, std::string> > ret;

    for (itr = jobIds.begin(); itr != jobIds.end(); ++itr)
        {
            std::stringstream ss;
            std::string url = endpoint + "/jobs/" + *itr;
            HttpRequest http (url, capath, proxy, ss);
            http.del();

            ResponseParser response(ss);
            ret.push_back(std::make_pair(response.get("job_id"), response.get("job_state")));
        }

    return ret;
}

std::string RestContextAdapter::transferSubmit (std::vector<File> const & files, std::map<std::string, std::string> const & parameters)
{
    std::stringstream ss;
    ss << RestSubmission(files, parameters);

    std::string url = endpoint + "/jobs";
    HttpRequest http (url, capath, proxy, ss);
    http.put();

    ResponseParser response(ss);
    return response.get("job_id");
}

JobStatus RestContextAdapter::getTransferJobStatus (std::string const & jobId, bool archive)
{
    std::string url = endpoint + "/jobs/" + jobId;

    std::stringstream ss;
    HttpRequest http (url, capath, proxy, ss);
    http.get();

    ResponseParser response(ss);

    return JobStatus(
               response.get("job_id"),
               response.get("job_state"),
               response.get("user_dn"),
               response.get("reason"),
               response.get("vo_name"),
               response.get("submit_time"),
               -1, // this is never shown so we don't care
               boost::lexical_cast<int>(response.get("priority"))
           );
}

JobStatus RestContextAdapter::getTransferJobSummary (std::string const & jobId, bool archive)
{
    // first get the files
    std::string url_files = endpoint + "/jobs/" + jobId + "/files";

    std::stringstream ss_files;
    ss_files << "{\"files\" :";
    HttpRequest http_files (url_files, capath, proxy, ss_files);
    http_files.get();
    ss_files << '}';

    ResponseParser response_files(ss_files);

    JobStatus::JobSummary summary (
        response_files.getNb("files", "ACTIVE"),
        response_files.getNb("files", "READY"),
        response_files.getNb("files", "CANCELED"),
        response_files.getNb("files", "FINISHED"),
        response_files.getNb("files", "SUBMITTED"),
        response_files.getNb("files", "FAILED")
    );

    // than get the job itself
    std::string url = endpoint + "/jobs/" + jobId;

    std::stringstream ss;
    HttpRequest http (url, capath, proxy, ss);
    http.get();

    ResponseParser response(ss);

    return JobStatus(
               response.get("job_id"),
               response.get("job_state"),
               response.get("user_dn"),
               response.get("reason"),
               response.get("vo_name"),
               response.get("submit_time"),
               response_files.getFiles("files").size(),
               boost::lexical_cast<int>(response.get("priority")),
               summary
           );
}

std::vector<FileInfo> RestContextAdapter::getFileStatus (std::string const & jobId, bool archive, int offset, int limit, bool retries)
{
    std::string url = endpoint + "/jobs/" + jobId + "/files";

    std::stringstream ss;
    ss << "{\"files\" :";
    HttpRequest http (url, capath, proxy, ss);
    http.get();
    ss << '}';

    ResponseParser response(ss);
    return response.getFiles("files");
}

std::vector<Snapshot> RestContextAdapter::getSnapShot(std::string const & vo, std::string const & src, std::string const & dst)
{
    char prefix = '?';
    std::string url = endpoint + "/snapshot";

    if (!vo.empty())
        {
            url += prefix;
            url += "vo_name=";
            url += vo;
            prefix = '&';
        }

    if (!dst.empty())
        {
            url += prefix;
            url += "dest_se=";
            url += dst;
            prefix = '&';
        }

    if (!src.empty())
        {
            url += prefix;
            url += "source_se=";
            url += src;
        }

    std::stringstream ss;
    ss << "{\"snapshot\":";
    HttpRequest http (url, capath, proxy, ss);
    http.get();
    ss << '}';

    return ResponseParser(ss).getSnapshot();
}

void RestContextAdapter::delegate(std::string const & delegationId, long expirationTime)
{
    // delegate Proxy Certificate
    RestDelegator delegator(endpoint, delegationId, expirationTime, capath, proxy);
    delegator.delegate();
}

std::vector<DetailedFileStatus> RestContextAdapter::getDetailedJobStatus(std::string const & jobId)
{
    std::string url = endpoint + "/jobs/" + jobId + "/files";

    std::stringstream ss;
    ss << "{\"files\" :";
    HttpRequest http (url, capath, proxy, ss);
    http.get();
    ss << '}';

    return ResponseParser(ss).getDetailedFiles("files");

}

} /* namespace cli */
} /* namespace fts3 */
