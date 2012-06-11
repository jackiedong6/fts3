
#include "signal_logger.h"
#include <unistd.h>
#include <execinfo.h>
#include <stdlib.h>
#include <iostream>

/**
 * log_stack
 * log program stack as warnings
 */
void SignalLogger::log_stack(int sig){
    std::cerr << sig << std::endl;
    //*logStream_  << "Received signal " <<  sig << '\n';   
    //(*logStream_).flush();
    if(sig ==15){ //SIGTERM      
    	//*logStream_  << "INFO TRANSFER ABORTED " << '\n';   
	raise(SIGINT);
    }
    else{
    	const int stack_size = 25;
    	void * array[stack_size];
    	int nSize = backtrace(array, stack_size);
    	char ** symbols = backtrace_symbols(array, nSize);
    	for (int i = 0; i < nSize; ++i){
        	//*logStream_  << "ERROR " << symbols[i] << '\n';
    	}
    	free(symbols);
    }
}

//------------------------------------------------------------------------------
// SignalLogger
//------------------------------------------------------------------------------

/**
 * registerSignal
 * register a handler to log given signal
 * @param signum [IN] signal number
 * @param signame [IN] signal name
 */
void SignalLogger::registerSignal(const int signum,const std::string& signame){
    SignalInfoMap::const_iterator it = m_map.find(signum);
    if (m_map.end() == it){
        m_map.insert(std::make_pair<int,SignalInfo *>(signum,new SignalInfo(signum,signame)));
    }
}

/**
 * destructor
 */
SignalLogger::~SignalLogger(){
    for (SignalInfoMap::iterator it = m_map.begin();it != m_map.end();++it){
        SignalInfo * info = it->second;
        delete info;
    }
    m_map.clear();
}

/**
 * constructor
 * @param signum [IN] signal number
 * @param signame [IN] signal name
 */
SignalLogger::SignalInfo::SignalInfo(int signum,const std::string& signame) :
    m_signum(signum),
    m_signame(signame),
    m_set(true){
    struct sigaction sa;
    sa.sa_handler = SignalLogger::handleSignal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(m_signum, &sa, &m_old);
}

/**
 * deregister
 * deregister handler
 */
void SignalLogger::SignalInfo::deregister(){
    if (m_set){
        m_set = false;
        sigaction(m_signum, &m_old, 0);
    }
}

/**
 * destructor
 */
SignalLogger::SignalInfo::~SignalInfo(){
    deregister();
}

/**
 * handleSignal
 * actual signal handler function (registered by SignalInfo objects)
 */
void SignalLogger::handleSignal(int signum){
    instance().logSignal(signum);
}

/**
 * logSignal
 * actual signal logging
 */
void SignalLogger::logSignal(int signum){
     //*logStream_  << "Received signal 111 " <<  signum << '\n'; 
     //(*logStream_).flush();
    SignalInfoMap::iterator it = m_map.find(signum);
    if (m_map.end() != it){
        SignalInfo * info = it->second;
        info->deregister();        
        log_stack(signum);
    }
}
