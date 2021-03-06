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


#include "Application.h"
#include "KeyedObject.h"

using namespace rapidjson;
using namespace std;

namespace gxy
{

static int ko_count = 0;
static int get_ko_count() { return ko_count; }

WORK_CLASS_TYPE(KeyedObject::CommitMsg)
WORK_CLASS_TYPE(KeyedObjectFactory::NewMsg)
WORK_CLASS_TYPE(KeyedObjectFactory::DropMsg)

KeyedObjectFactory* GetTheKeyedObjectFactory()
{
	return GetTheApplication()->GetTheKeyedObjectFactory();
}

bool
KeyedObjectFactory::NewMsg::CollectiveAction(MPI_Comm comm, bool isRoot)
{
	if (!isRoot)
	{
		unsigned char *p = (unsigned char *)get();
		KeyedObjectClass c = *(KeyedObjectClass *)p;
		p += sizeof(KeyedObjectClass);
		Key k = *(Key *)p;
		KeyedObjectP kop = shared_ptr<KeyedObject>(GetTheKeyedObjectFactory()->New(c, k));
	}

	return false;
}

void
KeyedObjectFactory::Drop(Key k)
{
#ifdef GXY_LOGGING
	APP_LOG(<< "DROP " << k);
#endif
	DropMsg msg(k);
	msg.Broadcast(true, true);
}

bool 
KeyedObjectFactory::DropMsg::CollectiveAction(MPI_Comm comm, bool isRoot)
{
	unsigned char *p = (unsigned char *)get();
	Key k = *(Key *)p;
	GetTheKeyedObjectFactory()->erase(k);
	return false;
}

KeyedObjectP
KeyedObjectFactory::get(Key k)
{
	if (k >= kmap.size())
		return NULL;
	else
		return kmap[k];
}

void
KeyedObjectFactory::erase(Key k)
{
	kmap[k] = NULL;
}

void
KeyedObjectFactory::add(KeyedObjectP p)
{
#if 0
	for (int i = kmap.size(); i < p->getkey(); i++)
		kmap.push_back(NULL);

	kmap.push_back(p);
#else
  	for (int i = kmap.size(); i <= p->getkey(); i++)
		kmap.push_back(NULL);

	kmap[p->getkey()] = p;
#endif
}

void
KeyedObject::Drop()
{
	GetTheKeyedObjectFactory()->Drop(getkey());
}

KeyedObject::KeyedObject(KeyedObjectClass c, Key k) : keyedObjectClass(c), key(k)
{  
	ko_count++;
	// cerr << "KeyedObject ctor: " << GetTheKeyedObjectFactory()->GetClassName(c) << " key " << k << endl;
	initialize();
}

KeyedObject::~KeyedObject()
{  
  
	ko_count--;
}

bool
KeyedObject::local_commit(MPI_Comm c)
{
	return false;
}

KeyedObjectP
KeyedObject::Deserialize(unsigned char *p)
{
	Key k = *(Key *)p;
	p += sizeof(Key);

	KeyedObjectP kop = GetTheKeyedObjectFactory()->get(k);
	p = kop->deserialize(p);

	if (*(int *)p != 12345)
	{
		cerr << "ERROR: Could not deserialize KeyedObject " << endl;
		exit(1);
	}

	return kop;
}

unsigned char *
KeyedObject::Serialize(unsigned char *p)
{
	*(Key *)p = getkey();
	p += sizeof(Key);
	p = serialize(p);
	*(int*)p = 12345;
	p += sizeof(int);
	return p;
}

int
KeyedObject::serialSize()
{
	return sizeof(Key) + sizeof(int);
}

unsigned char *
KeyedObject::serialize(unsigned char *p)
{
	return p;
}

unsigned char *
KeyedObject::deserialize(unsigned char *p)
{
	return p;
}

void
KeyedObject::Commit()
{
	CommitMsg msg(this);
	msg.Broadcast(true, true);
}

KeyedObject::CommitMsg::CommitMsg(KeyedObject* kop) : KeyedObject::CommitMsg::CommitMsg(kop->SerialSize())
{
	unsigned char *p = (unsigned char *)get();
	kop->Serialize(p);
}

bool
KeyedObject::CommitMsg::CollectiveAction(MPI_Comm c, bool isRoot)
{
  unsigned char *p = (unsigned char *)get();
  Key k = *(Key *)p;
  p += sizeof(Key);

  KeyedObjectP kop = GetTheKeyedObjectFactory()->get(k);

  if (! isRoot)
    kop->deserialize(p);

  return kop->local_commit(c);
}

void
KeyedObjectFactory::Dump()
{
	for (int i = 0; i < kmap.size(); i++)
	{
		KeyedObjectP kop = kmap[i];
		if (kop != NULL)
			cerr << "key " << i << " " << GetClassName(kop->getclass()) << " count " << kop.use_count() << endl;
	}
}

KeyedObjectFactory::~KeyedObjectFactory() 
{
	while (kmap.size() > 0)
		kmap.pop_back();

#if 0
	if (ko_count > 0)
		cerr << ko_count << " shared objects remain" << endl;
#endif
}

} // namespace gxy

