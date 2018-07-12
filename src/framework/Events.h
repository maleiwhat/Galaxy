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
#include <fstream>
#include <memory>
#include <pthread.h>
#include <vector>

#include "KeyedObject.h"

extern pthread_mutex_t EventsLock;


namespace gxy
{
class Event
{
public:
	Event();
	~Event(){}

	void Print(std::ostream& o) { print(o); }

protected:
	virtual void print(std::ostream&);

private:
	double time;
};

class EventTracker
{
public:
	EventTracker();

	void Add(Event *e);

	void DumpEvents(std::ostream& o);

	static double gettime();

	bool is_empty() { return events.size() == 0; }
	
private:
	std::vector<std::shared_ptr<Event>> events;
};

extern EventTracker *GetTheEventTracker();
}