#pragma once

#include <mpi.h>
#include <stdlib.h>
#include <iostream>
#include <memory>
#include "Work.h"

namespace gxy
{

class MessageManager;

class Message {
  friend class MessageManager;

public:
  // Point to point, fire-and-forget.   If i is -1, then we assume
  // its client/server and the destination is the other guy
 
  Message(Work *w, int i = -1);

  // Collective, one-to-all, will be run in MPI thread. May be blocking
	// for the caller

  Message(Work *w, bool blk = false);

	// Message to be read off MPI
	Message(MPI_Status&);

	// Message to be read off socket
	Message(int skt, int size);

  Message();

  ~Message();

	bool isBlocking() { return blocking; }
	void Signal()
	{ 
		pthread_mutex_lock(&lock);
		pthread_cond_signal(&cond);
		pthread_mutex_unlock(&lock);
	}



public:
	void Send(int i);
	void Broadcast();

  int  GetType() { return header.type; }
  void SetType(int t) { header.type = t; }

  unsigned char *GetContent() { return content->get(); }
  size_t GetSize() { return content->get_size(); }

  int GetRoot() { return header.broadcast_root; }
  int GetSender() { return header.sender; }

	bool IsBroadcast() { return header.broadcast_root != -1; }
	bool IsP2P() { return ! IsBroadcast(); }

	bool HasContent() { return header.HasContent(); }

  void SetDestination(int i) { destination = i; }
  int GetDestination() { return destination; }

  // Wait for a blocking message to be handled.  This will return
  // when the message has been sent, and if the message is a broadcast
  // message (that is, runs in the messaging thread), for the message's
  // action to happen locally.  ONLY VALID IF BLOCKING
  void Wait();

	SharedP ShareContent() { return content; }


protected:
  struct MessageHeader {

    int  broadcast_root; // will be -1 for point-to-point
		int  sender; 				 // will be -1 for broadcast
    int  type;
    int  content_tag;
    int  content_size;

		bool HasContent() { return content_size > 0; }

  } header;

	unsigned char *GetHeader() { return (unsigned char *)&header; }
	int   GetHeaderSize() { return sizeof(header); }

  int id;

  bool blocking;
  pthread_mutex_t lock;
  pthread_cond_t cond;

  SharedP content;
	int destination;

  MPI_Status status;
  MPI_Request request;
};
}
