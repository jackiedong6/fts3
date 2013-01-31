#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <vector>
#include <string>
#include <iostream>
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "producer_consumer_common.h"
#include "definitions.h"
#include <algorithm>
#include <ctime>

using namespace std;

struct sort_functor_updater
{
    bool operator()(const message_updater & a, const message_updater & b) const
    {        
        return a.timestamp < b.timestamp;
    }
};

struct sort_functor_status
{
    bool operator()(const message & a, const message & b) const
    {        
        return a.timestamp < b.timestamp;
    }
};



int getDir (string dir, vector<string> &files)
{
    DIR *dp=NULL;
    struct dirent *dirp=NULL;
    struct stat st;
    if((dp  = opendir(dir.c_str())) == NULL) {
        cout << "Error(" << errno << ") opening " << dir << endl;
        return errno;
    }

    while ((dirp = readdir(dp)) != NULL) {
        std::string fileName = string(dirp->d_name);
	size_t found = fileName.find("ready");
	stat(fileName.c_str(), &st);
	if(found!=std::string::npos && st.st_size > 0){
		std::string copyFilename = dir + "/" + fileName;		
        	files.push_back(copyFilename);
	}
    }
    closedir(dp);
    return 0;
}

void runConsumerMonitoring(std::vector<std::string>& messages)
{
    string dir = string("/var/lib/fts3/monitoring");
    vector<string> files = vector<string>();
    char msg[3000]={0};
    
    getDir(dir,files);    
    for (unsigned int i = 0;i < files.size();i++) {      
	FILE *fp = NULL;	
	if ((fp = fopen(files[i].c_str(), "r")) != NULL){
	  fread(msg, sizeof(msg), 1, fp);	  
	  messages.push_back(std::string(msg));
	  unlink(files[i].c_str());
	  fclose(fp);
	}
    }
    files.clear();
}


void runConsumerStatus(std::vector<struct message>& messages){
    string dir = string("/var/lib/fts3/status");
    vector<string> files = vector<string>();
    struct message msg;
    
    getDir(dir,files);    
    for (unsigned int i = 0;i < files.size();i++) {      
	FILE *fp = NULL;	
	if ((fp = fopen(files[i].c_str(), "r")) != NULL){
	  fread(&msg, sizeof(msg), 1, fp);	  
	  messages.push_back(msg);	  
	  unlink(files[i].c_str());
	  fclose(fp);
	}
    }
    files.clear();
    std::sort(messages.begin(), messages.end(), sort_functor_status());
}

void runConsumerStall(std::vector<struct message_updater>& messages){
    string dir = string("/var/lib/fts3/stalled");
    vector<string> files = vector<string>();
    struct message_updater msg;
    
    getDir(dir,files);    
    for (unsigned int i = 0;i < files.size();i++) {      
	FILE *fp = NULL;	
	if ((fp = fopen(files[i].c_str(), "r")) != NULL){
	  fread(&msg, sizeof(msg), 1, fp);	  
	  messages.push_back(msg);
	  unlink(files[i].c_str());
	  fclose(fp);
	}
    }
    files.clear();
    std::sort(messages.begin(), messages.end(), sort_functor_updater());
}
