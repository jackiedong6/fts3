/*
 *	Copyright notice:
 *	Copyright © Members of the EMI Collaboration, 2010.
 *
 *	See www.eu-emi.eu for details on the copyright holders
 *
 *	Licensed under the Apache License, Version 2.0 (the "License");
 *	you may not use this file except in compliance with the License.
 *	You may obtain a copy of the License at
 *
 *		http://www.apache.org/licenses/LICENSE-2.0
 *
 *	Unless required by applicable law or agreed to in writing, software
 *	distributed under the License is distributed on an "AS IS" BASIS,
 *	WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *	See the License for the specific language governing permissions and
 *	limitations under the License.
 *
 * CliBase.cpp
 *
 *  Created on: Feb 2, 2012
 *      Author: Michal Simon
 */

#include "CliBase.h"

#include <iostream>
#include <fstream>

#include <boost/lexical_cast.hpp>
#include <boost/scoped_array.hpp>
#include <boost/property_tree/json_parser.hpp>
#include "version.h"

using namespace fts3::cli;

const string CliBase::error = "error";
const string CliBase::result = "result";
const string CliBase::parameter_error = "parameter_error";

CliBase::CliBase(): visible("Allowed options"), ctx(NULL)
{

    // add basic command line options
    basic.add_options()
    ("help,h", "Print this help text and exit.")
    ("quite,q", "Quiet operation.")
    ("verbose,v", "Be more verbose.")
    ("service,s", value<string>(), "Use the transfer service at the specified URL.")
    ("version,V", "Print the version number and exit.")
    ;

    version = getCliVersion();
    interface = version;
}

CliBase::~CliBase()
{
    if (ctx)
        {
            delete ctx;
            ctx = NULL;
        }
}

void CliBase::parse(int ac, char* av[])
{

    // set the output parameters (verbose and json) before the acctual parsing happens
    for (int i = 0; i < ac; i++)
        {
            string str(av[i]);
            if (str == "-v")
                {
                    msgPrinter.setVerbose(true);
                }
            else if (str == "-j")
                {
                    msgPrinter.setJson(true);
                }
        }

    toolname = av[0];

    // add specific and hidden parameters to all parameters
    all.add(basic).add(specific).add(hidden).add(command_specific);
    // add specific parameters to visible parameters (printed in help)
    visible.add(basic).add(specific);

    // turn off guessing, so --source is not mistaken with --source-token
    int style = command_line_style::default_style & ~command_line_style::allow_guessing;

    // parse the options that have been used
    store(command_line_parser(ac, av).options(all).positional(p).style(style).run(), vm);
    notify(vm);

    // check is the output is verbose
    msgPrinter.setVerbose(
        vm.count("verbose")
    );
    // check if the output is in json format
    msgPrinter.setJson(
        vm.count("json")
    );

    // check whether the -s option has been used
    if (vm.count("service"))
        {
            endpoint = vm["service"].as<string>();
            // check if the endpoint has the right prefix
            if (endpoint.find("http") != 0 && endpoint.find("https") != 0 && endpoint.find("httpd") != 0)
                {
                    msgPrinter.wrong_endpoint_format(endpoint);
                    // if not erase
                    endpoint.erase();
                }
        }
    else
        {

            if (useSrvConfig())
                {
                    fstream  cfg ("/etc/fts3/fts3config");
                    if (cfg.is_open())
                        {

                            string ip;
                            string port;
                            string line;

                            do
                                {
                                    getline(cfg, line);
                                    if (line.find("#") != 0 && line.find("Port") == 0)
                                        {
                                            // erase 'Port='
                                            port = line.erase(0, 5);

                                        }
                                    else if (line.find("#") != 0 && line.find("IP") == 0)
                                        {
                                            // erase 'IP='
                                            ip += line.erase(0, 3);
                                        }

                                }
                            while(!cfg.eof());

                            if (!ip.empty() && !port.empty())
                                endpoint = "https://" + ip + ":" + port;
                        }
                }
            else if (!vm.count("help") && !vm.count("version"))
                {
                    // if the -s option has not been used try to discover the endpoint
                    // (but only if -h and -v option were not used)
                    endpoint = discoverService();
                }
        }
}

optional<GSoapContextAdapter&> CliBase::validate(bool init)
{

    // if applicable print help or version and exit, nothing else needs to be done
    if (printHelp(toolname) || printVersion()) return optional<GSoapContextAdapter&>();

    // if endpoint could not be determined, we cannot do anything
    if (endpoint.empty())
        {
            throw string("Failed to determine the endpoint");
        }

    // create and initialize gsoap context
    ctx = new GSoapContextAdapter(endpoint);
    if (init) ctx->init();

    // if verbose print general info
    if (isVerbose())
        {
            msgPrinter.endpoint(ctx->getEndpoint());
            msgPrinter.service_version(ctx->getVersion());
            msgPrinter.service_interface(ctx->getInterface());
            msgPrinter.service_schema(ctx->getSchema());
            msgPrinter.service_metadata(ctx->getMetadata());
            msgPrinter.client_version(version);
            msgPrinter.client_interface(interface);
        }

    return *ctx;
}

string CliBase::getUsageString(string tool)
{
    return "Usage: " + tool + " [options]";
}

bool CliBase::printHelp(string tool)
{

    // check whether the -h option was used
    if (vm.count("help"))
        {

            // remove the path to the executive
            size_t pos = tool.find_last_of('/');
            if( pos != string::npos)
                {
                    tool = tool.substr(pos + 1);
                }
            // print the usage guigelines
            cout << endl << getUsageString(tool) << endl << endl;
            // print the available options
            cout << visible << endl;
            return true;
        }

    return false;
}

bool CliBase::printVersion()
{

    // check whether the -V option was used
    if (vm.count("version"))
        {
            msgPrinter.version(version);
            return true;
        }

    return false;
}

bool CliBase::isVerbose()
{
    return vm.count("verbose");
}

bool CliBase::isQuite()
{
    return vm.count("quite");
}

string CliBase::getService()
{

    return endpoint;
}

string CliBase::discoverService()
{
    string tmp;
    return tmp;
}

string CliBase::getCliVersion()
{

    return VERSION;
}
