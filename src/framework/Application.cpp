// ========================================================================== //
// Copyright (c) 2014-2018 The University of Texas at Austin.                 //
// All rights reserved.                                                       //
//                                                                            //
// Licensed under the Apache License, Version 2.0 (the "License");            //
// you may not use this file except in compliance with the License.           //
// A copy of the License is included with this software in the file LICENSE.  //
// If your copy does not contain the License, you may obtain a copy of the    //
// License at:                                                                //
//                                                                            //
//     https://www.apache.org/licenses/LICENSE-2.0                            //
//                                                                            //
// Unless required by applicable law or agreed to in writing, software        //
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT  //
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.           //
// See the License for the specific language governing permissions and        //
// limitations under the License.                                             //
//                                                                            //
// ========================================================================== //


#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <iostream>
#include <sstream>
#include <fstream>
#include <map>

#include "Application.h"
#include "KeyedObject.h"

#include "tbb/tbb.h"
#include "tbb/task_scheduler_init.h"

#include "rapidjson/document.h"
#include "rapidjson/filereadstream.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/stringbuffer.h"

using namespace rapidjson;
using namespace std;

namespace gxy
{
WORK_CLASS_TYPE(Application::QuitMsg)
WORK_CLASS_TYPE(Application::PrintMsg)

static Application *theApplication;

int cte_indx = 0;

ClassTableEntry::ClassTableEntry(string s)
{
	my_string = new string(s.c_str());
	indx = cte_indx++;
}

ClassTableEntry::ClassTableEntry(const ClassTableEntry& o)
{
	
	indx = cte_indx++;
	// std::cerr << "CTE cpyctor " << indx << " from " << o.indx << "\n";
	my_string = new string(o.my_string->c_str());
}

ClassTableEntry::~ClassTableEntry()
{
	// std::cerr << "CTE dtor " << indx << " " << my_string << " " << my_string->c_str() << "\n";
	delete my_string;
}

const char *ClassTableEntry::c_str() { return my_string->c_str(); }

Application *
GetTheApplication()
{
	return theApplication;
}

Application::Application(int *a, char ***b) : Application()
{
  argcp = a;
  argvp = b;
}

Application::Application()
{
  theApplication = this;

  int n_threads =  getenv("GXY_NTHREADS") ?  atoi(getenv("GXY_NTHREADS")) : 1;
  std::cerr << "Using " << n_threads << " threads in rendering thread pool." << std::endl;

  threadPool = new threadpool11::Pool(n_threads);

  class_table = new vector<ClassTableEntry>;
  deserializers =  new vector<Work *(*)(SharedP)>;
	theMessageManager = new MessageManager; 
	theKeyedObjectFactory = new KeyedObjectFactory; 

	if (getenv("GXY_APP_NTHREADS"))
	{
		int nthreads = atoi(getenv("GXY_APP_NTHREADS"));
		tbb::task_scheduler_init init(nthreads);
		std::cerr << "using " << nthreads << " TBB threads for application." << std::endl;
	}
	else
	{
		int n = tbb::task_scheduler_init::default_num_threads();
		std::cerr << "using " << n << " TBB threads for application." << std::endl;
	}

	pid = getpid();

	register_thread("Application");

  application_done = false;

  argcp = 0;
  argvp = NULL;

  pthread_mutex_init(&lock, NULL);
  pthread_cond_init(&cond, NULL);

  pthread_mutex_lock(&lock);

	QuitMsg::Register();
  PrintMsg::Register();

	KeyedObject::Register();
	KeyedObjectFactory::Register();

  pthread_mutex_unlock(&lock);
}


void
Application::Log(std::string s)
{
	char buf[256];
	sprintf(buf, "log.%d", GetRank());

	fstream f;
	f.open(buf, fstream::out | fstream::app);
	f << s << "\n";
	f.close();
}

void
Application::Print(std::string s)
{
	Application::PrintMsg msg(s);
	msg.Send(0);
}

Application::~Application()
{
	if (! eventTracker.is_empty())
	{
		int rank = GetTheMessageManager()->GetRank();
		std::fstream fs;
		std::stringstream fname;
		fname << "events_" << rank;
		fs.open(fname.str().c_str(), std::fstream::out);
		eventTracker.DumpEvents(fs);
		fs.close();
	}
	
  pthread_mutex_unlock(&lock);

  delete threadPool;
	delete class_table;
	delete deserializers;
	delete theMessageManager;
	delete theKeyedObjectFactory;
}

static pthread_mutex_t log_lock = PTHREAD_MUTEX_INITIALIZER;

void
Application::log(std::stringstream &s)
{
  pthread_mutex_lock(&log_lock);
  std::fstream fs;
  std::stringstream fname;
	int rank = GetTheMessageManager()->GetRank();
  fname << "log_" << rank;
  fs.open(fname.str().c_str(), std::fstream::in | std::fstream::out | std::fstream::app);
  fs << rank << ": " << s.str();
  fs.close();
  pthread_mutex_unlock(&log_lock);
}

void Application::QuitApplication()
{
	QuitMsg *q = new QuitMsg(0);
	q->Broadcast(true);

	Application::Wait();
}

void Application::Start(bool with_mpi)
{
  GetTheMessageManager()->Start(with_mpi);
}

void Application::Kill()
{
	pthread_mutex_lock(&lock);
  application_done = true;
  pthread_cond_broadcast(&cond);
  pthread_mutex_unlock(&lock);
}

bool
Application::QuitMsg::CollectiveAction(MPI_Comm coll_comm, bool isRoot)
{
	MPI_Barrier(coll_comm);
	return true;
}

Work *
Application::Deserialize(Message *msg)
{
	return (*deserializers)[msg->GetType()](msg->ShareContent());
}

const char *
Application::Identify(Message *msg)
{
	return (*class_table)[msg->GetType()].c_str();
}

void
Application::Wait()
{
	while (! application_done)
		pthread_cond_wait(&cond, &lock);

	pthread_mutex_unlock(&lock);
}

Application::PrintMsg::PrintMsg(string &str) : Application::PrintMsg::PrintMsg(str.length() + 1)
{
	unsigned char *ptr = (unsigned char *)get();
	memcpy(ptr, str.c_str(), str.length() + 1);
}

bool 
Application::PrintMsg::Action(int sender)
{
  unsigned char *ptr = (unsigned char *)get();
  std::cerr << sender << ": " << (char *)ptr << "\n";
  return false;
}
	
Document *
Application::OpenInputState(string s)
{
  Document *doc = new Document();

  FILE *pFile = fopen (s.c_str() , "r");
  char buf[64];
  rapidjson::FileReadStream is(pFile,buf,64);
  doc->ParseStream<0>(is);
  fclose(pFile);

	return doc;
}

Document *
Application::OpenOutputState()
{
	Document *doc = new Document;
  doc->Parse("{}");
	return doc;
}

void
Application::SaveOutputState(Document *doc, string s)
{
  StringBuffer sbuf;
  PrettyWriter<StringBuffer> writer(sbuf);
  doc->Accept(writer);

  ofstream ofs;
  ofs.open(s.c_str(), ofstream::out);
  ofs << sbuf.GetString() << "\n";
  ofs.close();
}

pthread_mutex_t threadtable_lock = PTHREAD_MUTEX_INITIALIZER;
map<long, string> thread_map;

long
my_gettid()
{
  return ((long)pthread_self() & 0xffff);
}

const char *
my_threadname()
{
  if (thread_map.find(my_gettid()) == thread_map.end())
    return "this thread has no name";
  else
    return thread_map[my_gettid()].c_str();

}

const char *
threadname_by_id(long l)
{
  if (l == -1) return "nobody";
  else return thread_map[l].c_str();
}

void
register_thread(string s)
{
  pthread_mutex_lock(&threadtable_lock);
  thread_map[my_gettid()] = s;
  pthread_mutex_unlock(&threadtable_lock);
}

}
