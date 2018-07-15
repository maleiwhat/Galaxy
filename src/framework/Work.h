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

#include <iostream>
#include <memory>
#include <mpi.h>
#include <smem.h>
#include <vector>

namespace gxy
{

class Work
{
public:
	Work();
	Work(int n);
	Work(SharedP s);
	Work(const Work* o);
	~Work();

	void *get() { return contents->get(); }
	int   get_size() { return contents->get_size(); }

  virtual void initialize() {};
  virtual void destructor() { std::cerr << "generic Work destructor" << std::endl; }
  virtual bool Action(int sender) 
  { std::cerr << "killed by generic work Action()" << std::endl; return true; };
  virtual bool CollectiveAction(MPI_Comm comm, bool isRoot) 
  { std::cerr << "killed by generic collective work Action()" << std::endl; return true; };

	void Send(int i);
	void Broadcast(bool c, bool b = false);

	std::string getClassName() { return className; };
  int GetType() { return type; }

	SharedP get_pointer() { return contents; }

	void (*dtor)();

	int xyzzy() { return contents.use_count(); }

protected:
	static int RegisterSubclass(std::string, Work *(*d)(SharedP));

  int 		      type;
  std::string 	className;
	SharedP       contents;
};


#define WORK_CLASS_TYPE(ClassName) 																											\
		int ClassName::class_type = 0;        																							\
		std::string ClassName::class_name = #ClassName; 							  										\


#define WORK_CLASS(ClassName, bcast)				 																						\
 																																												\
public: 																																								\
 																																												\
	ClassName(size_t n) : Work(n)																													\
	{																							 																				\
    className = std::string(#ClassName); 																								\
    type = ClassName::class_type; 																											\
		initialize(); 																																			\
	} 																																										\
 																																												\
	ClassName(SharedP p) : Work(p) 							  																				\
	{																							 																				\
    className = std::string(#ClassName); 																								\
    type = ClassName::class_type; 																											\
		initialize(); 																																			\
  } 																																										\
 																																												\
public: 																																								\
	std::string getClassName() { return std::string(#ClassName); } 												\
	unsigned char *get_contents() { return contents->get(); } 														\
 																																												\
	static void Register()   																															\
	{																							 																				\
    ClassName::class_type = Work::RegisterSubclass(ClassName::class_name, Deserialize);	\
  } 																																										\
																																												\
  static Work *Deserialize(SharedP ptr)   																							\
	{																							 																				\
		return (Work *) new ClassName(ptr); 																								\
  } 																																										\
																																												\
private: 																																								\
	static int class_type;																																\
	static std::string class_name;

} // namespace gxy
