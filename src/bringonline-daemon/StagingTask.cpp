/*
 * StagingTask.cpp
 *
 *  Created on: 25 Jun 2014
 *      Author: simonm
 */

#include "StagingTask.h"

#include "common/error.h"

bool StagingTask::retryTransfer(int errorNo, const std::string& category, const std::string& message)
{
    bool retry = true;

    // ETIMEDOUT shortcuts the following
    if (errorNo == ETIMEDOUT)
        return true;

    //search for error patterns not reported by SRM or GSIFTP
    std::size_t found = message.find("performance marker");
    if (found!=std::string::npos)
        return true;
    found = message.find("Connection timed out");
    if (found!=std::string::npos)
        return true;
    found = message.find("end-of-file was reached");
    if (found!=std::string::npos)
        return true;

    if (category == "SOURCE")
        {
            switch (errorNo)
                {
                case ENOENT:          // No such file or directory
                case EPERM:           // Operation not permitted
                case EACCES:          // Permission denied
                case EISDIR:          // Is a directory
                case ENAMETOOLONG:    // File name too long
                case E2BIG:           // Argument list too long
                case ENOTDIR:         // Part of the path is not a directory
                case EPROTONOSUPPORT: // Protocol not supported by gfal2 (plugin missing?)
                case EINVAL:          // Invalid argument. i.e: invalid request token
                    retry = false;
                    break;
                default:
                    retry = true;
                    break;
                }
        }

    found = message.find("proxy expired");
    if (found!=std::string::npos)
        retry = false;
    found = message.find("File exists and overwrite");
    if (found!=std::string::npos)
        retry = false;
    found = message.find("No such file or directory");
    if (found!=std::string::npos)
        retry = false;
    found = message.find("SRM_INVALID_PATH");
    if (found!=std::string::npos)
        retry = false;
    found = message.find("The certificate has expired");
    if (found!=std::string::npos)
        retry = false;
    found = message.find("The available CRL has expired");
    if (found!=std::string::npos)
        retry = false;
    found = message.find("SRM Authentication failed");
    if (found!=std::string::npos)
        retry = false;
    found = message.find("SRM_DUPLICATION_ERROR");
    if (found!=std::string::npos)
        retry = false;
    found = message.find("SRM_AUTHENTICATION_FAILURE");
    if (found!=std::string::npos)
        retry = false;
    found = message.find("SRM_NO_FREE_SPACE");
    if (found!=std::string::npos)
        retry = false;
    found = message.find("digest too big for rsa key");
    if (found!=std::string::npos)
        retry = false;
    found = message.find("Can not determine address of local host");
    if (found!=std::string::npos)
        retry = false;
    found = message.find("Permission denied");
    if (found!=std::string::npos)
        retry = false;
    found = message.find("System error in write into HDFS");
    if (found!=std::string::npos)
        retry = false;

    return retry;
}
