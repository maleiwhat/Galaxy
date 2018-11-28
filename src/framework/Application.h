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

#pragma once

/*! \file Application.h 
 * \brief The core class for applications to use the Galaxy asynchronous framework
 * \ingroup framework
 */

#include <pthread.h>
#include <sstream>
#include <string>
#include <sys/types.h>
#include <unistd.h>
#include <vector>
#include <map>

#include "galaxy.h"
#include "KeyedObject.h"
#include "MessageManager.h"
#include "Work.h"

#include "rapidjson/document.h"

namespace gxy
{

class ThreadManager;
class ThreadPool;

/*!
 * Send the passed argument to a std::stringstream, 
 * which is then printed by Application::Print
 * \ingroup framework
 * \param x a set of stream operations suitable for std::stringstream, 
 * \warning argument must begin with stream operator `<<`
 * \sa gxy::Application::Print
 */
#define APP_PRINT(x)																				\
{																														\
  std::stringstream ss;																			\
	ss x;																											\
  if (GetTheApplication())                                  \
    GetTheApplication()->Print(ss.str());										\
}

/*!
 * Send the passed argument to a std::stringstream,
 * which is then logged by Application::Log. 
 * The log message also includes the calling id, threadname, and calling 
 * function.
 * \ingroup framework
 * \param x a set of stream operations suitable for std::stringstream, 
 * \warning argument must begin with stream operator `<<`
 * \sa gxy::Application::Log
 */
#define APP_LOG(x)																					                            \
{																														                            \
  if (GetTheApplication())                                                              \
  {                                                                                     \
    std::stringstream ss;																			                          \
    ss << my_gettid() << " " << my_threadname() << " " <<  __FUNCTION__ << ": " x;			\
    GetTheApplication()->Log(ss.str());												                          \
  }                                                                                     \
}


//! A table to track classes that define Work within Galaxy
/*! \ingroup framework 
 */
struct ClassTableEntry
{
  ClassTableEntry(std::string s);          //!< constuctor
	ClassTableEntry(const ClassTableEntry&); //!< copy constructor
  ~ClassTableEntry();											 //!< destructor

  //! returns table entry as a C character array
  const char *c_str();

  std::string *my_string; //!< Work class name as std::string
  int indx; 						  //!< table entry index
};

//! The core class for applications to use the Galaxy asynchronous framework
/*!
 * This class forms the core of the Galaxy asynchronous framework, including 
 * launching distributed processes, submitting Work, processing communication,
 * determining termination conditions, and outputting results.
 *
 * \ingroup framework
 * \sa Message, MessageManager, Work
 */
class Application {

public:
  Application();  //!< default constructor
  //! constructor that accepts command-line arguments to configure Galaxy
  Application(int *argc, char ***argv); 
  ~Application(); //!< default destructor

  //! returns the application's MessageManager object.
  MessageManager *GetTheMessageManager() { return theMessageManager; }
  //! returns the KeyedObjectFactory object, used to register data
  KeyedObjectFactory *GetTheKeyedObjectFactory() { return theKeyedObjectFactory; }
  //! returns the ThreadPool object, used to manage all threads created by the Application
  ThreadManager *GetTheThreadManager() { return threadManager; }
  //! returns the ThreadPool object, used to manage a pool of threads waiting for work tasks
  ThreadPool *GetTheThreadPool() { return threadPool; }

  //! print a std::string to std::cerr in a distributed parallel friendly way
	void Print(std::string);
	//! log a std::string to the log in a distributed parallel friendly way
	void Log(std::string);
	//! dump the accumulated log 
	/*! dumps the accumulated log to per-process log files `gxy_log_{i}` 
	 * where `{i}` is the process rank.
	 */
	void DumpLog();

	//! broadcast a 'Quit' message to all processes
	/*! Broadcasts a special 'Quit' Work object which instructs all processes
	 * to stop processing Work and terminate.
	 */
  void QuitApplication();
  //! broadcast a 'Sync' message to all processes
  /*! Broadcasts a special 'Sync' Work object which acts as a barrier to 
   * further Work processing by any process until all processes reach the sync.
   */
  void SyncApplication();
  //! broadcast a "LoadSO" message to all processes
  /*! Broadcast a "LoadSO" message to all processes causing them to load a
      module and run its 'init' procedure.  Master process opens the SO and
      returns the handle; the non-masters dlclose the SO.
   */
  void *LoadSO(std::string soname);

	//! returns a pointer to the argc initialization argument
	/*! \warning will return NULL if default constructor was used
	 */
  int *GetPArgC() { return argcp; }
	//! returns a pointer to the argv initialization argument
	/*! \warning will return NULL if default constructor was used
	 */
  char ***GetPArgV() { return argvp; }

  //! returns the number of processes in the messaging MPI communicator
  /*! \returns the size of the MPI communicator pool
   */
  int GetSize() { return GetTheMessageManager()->GetSize(); }

  //! returns the rank of the calling process in the messaging MPI communicator
  /*! \returns the rank of this process in the MPI communicator pool (or -1 if MPI has not been initialized)
   */
  int GetRank() { return GetTheMessageManager() ? GetTheMessageManager()->GetRank() : -1; }

  //! start Message processing by the Application
  /*! signal to the Application threads to begin processing Message objects
   * \param with_mpi if true, use MPI for distributed message passing
   * \sa Message, MessageManager, Work
   */
  void Start(bool with_mpi = true);
  //! notify the Application threads to terminate
  /*! This is used by QuitApplication to gracefully end processing and terminate
   * the Application.
   * \sa QuitApplication, Wait
   */
   void Kill();
  //! notify the Application threads to wait until a Kill notification is received
  /*! This is used by QuitApplication to gracefully end processing and terminate
   * the Application.
   * \sa Kill, QuitApplication
   */
	void Wait();
	//! returns true from initializtaion until Kill is called
	/* \sa Pause, Run */
  bool Running() { return !application_done; }

  //! temporarily stop processing Message objects
  /*! \sa Run 
   */
	void Pause() { GetTheMessageManager()->Pause(); }
	//! begin (or resume) processing Message objects
	/* \sa Pause 
	 */
	void Run() { GetTheMessageManager()->Run(); }

	//! convert a Message into a corresponding Work object
	/*! \returns a pointer to a Work object represented by the Message 
	 * \sa Message, MessageManager, Work
	 */
  Work *Deserialize(Message *msg);

  //! return the name of the Work object contained in the Message
  /*! \returns a character array with the name of the Work object class contained 
   *           in the Message
   */
  const char *Identify(Message *msg);

  //! adds the Work object class to the Application registry
  int RegisterWork(std::string name, Work *(*f)(SharedP)) {
    int n = (*deserializers).size();
    (*deserializers).push_back(f);
		ClassTableEntry c(name.c_str());
		(*class_table).push_back(c);
    return n;
  }

  //! is the application done (i.e. has a Kill notification been issued)?
  bool IsDoneSet() { return application_done; }
  //! return the process id of the Application
	pid_t get_pid() { return pid; }

	//! read the specified Galaxy input file in JSON format
	rapidjson::Document *OpenInputState(std::string s);
	//! initialize the output state file using JSON format
	/*! \returns a rapidjson::Document object pointer
	 */
	rapidjson::Document *OpenOutputState();
	//! write the output state file to the specified file
	/*! \param doc a pointer to the output state document
	 *  \param s the filename to create with the output state
	 */
	void SaveOutputState(rapidjson::Document *doc, std::string s);

  //! save a KeyedObject as a global
	/*! \param name the name under which the object is saved
	 *  \param obj the object to save
   */
  void SetGlobal(std::string name, KeyedObjectP obj) { globals[name] = obj; }

  //! retrieve a global KeyedObject.  NULL if it isn't there
	/*! \param name the name under which the object is saved
	 *  \param obj the object to save
   */
  KeyedObjectP GetGlobal(std::string name) { return globals[name]; }

  //! delete global reference to a global KeyedObject.
	/*! \param name the name under which the object is saved
   */
  void DropGlobal(std::string name) { globals.erase(name); }

private:
  std::map<std::string, KeyedObjectP> globals;      // Globally-known variables

  bool loadSO(std::string s);
  void *getSOFunction(std::string s, std::string p);
  std::map<std::string, void *> shared_objects;

	std::vector<std::string> log;
	MessageManager *theMessageManager;
	KeyedObjectFactory *theKeyedObjectFactory;

  std::vector<Work *(*)(SharedP)> *deserializers;
  std::vector<ClassTableEntry> *class_table;

	ThreadManager *threadManager;
  ThreadPool *threadPool;

  bool application_done;

  int *argcp;
  char ***argvp;

	pid_t pid;

  pthread_mutex_t lock;
  pthread_cond_t cond;

private:
	class QuitMsg : public Work
	{
	public:
		WORK_CLASS(QuitMsg, true)

	public:
		bool CollectiveAction(MPI_Comm coll_comm, bool isRoot);
	};

	class LoadSOMsg : public Work
	{
	public:
		WORK_CLASS(LoadSOMsg, true)

	public:
		bool CollectiveAction(MPI_Comm coll_comm, bool isRoot);
	};

	class SyncMsg : public Work
	{
	public:
		WORK_CLASS(SyncMsg, true)

	public:
		bool CollectiveAction(MPI_Comm coll_comm, bool isRoot);
	};

	class PrintMsg : public Work
	{
		WORK_CLASS(PrintMsg, false);
	public:
		PrintMsg(std::string &);
		bool Action(int sender);
	};
};

/*! \ingroup framework
 * @{
 */

//! global gettor for the application pointer
extern Application* GetTheApplication();  // TODO: make static class method?

//! get thread id (masked by 0xFFFF)
extern long my_gettid();
//! return thread name for given id from global thread map
/*! \param l thread id to query
 */
extern const char *threadname_by_id(long l);
//! return the registered name of this thread, if any
extern const char *my_threadname();
//! register this thread with the given name
extern void register_thread(std::string s);

/*! }@  */ // group framework

} // namespace gxy

