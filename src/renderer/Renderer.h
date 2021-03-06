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

/*! \file Renderer.h 
 * \brief the primary class controlling rendering within Galaxy
 * \ingroup render
 */

#include <vector>

#include "dtypes.h"
#include "KeyedObject.h"
#include "Lighting.h"
#include "pthread.h"
#include "Rays.h"
#include "Rendering.h"
#include "RenderingEvents.h"
#include "RenderingSet.h"
#include "TraceRays.h"

namespace gxy
{

class Camera;
class RayQManager;
class Pixel;
class RayList;

KEYED_OBJECT_POINTER(Renderer)

//! the primary class controlling rendering within Galaxy
/*! \sa KeyedObject
 * \ingroup render
 */
class Renderer : public KeyedObject
{
  KEYED_OBJECT(Renderer)
    
public:
  static void Initialize(); //!< initialize the Renderer subcomponents

  virtual ~Renderer(); //!< default destructor
  virtual void initialize(); //!< initializes the singleton Renderer

  void ProcessRays(RayList *); //!< add the given RayList as a Task for the ThreadPool
  void SendRays(RayList *, int); //!< send the given RayList to the specified process rank

  void SetEpsilon(float e); //!< set the epsilon distance for the Renderer to avoid exact comparison in certain tests

  RayQManager *GetTheRayQManager() { return rayQmanager; }
  //! returns a pointer to the singleton Lighting object for this process
  Lighting *GetTheLighting() { return &lighting; }

  //! load a Renderer object from a Galaxy JSON document
  virtual void LoadStateFromDocument(rapidjson::Document&);
  //! save this Renderer object to a Galaxy JSON document
  virtual void SaveStateToDocument(rapidjson::Document&);

  //! render the given RenderingSet at this process, in response to a received RenderMsg
  virtual void localRendering(RendererP, RenderingSetP, MPI_Comm c);
  //! extract and retire any terminated rays in the given RayList
  /*! \param raylist the RayList to process
   * \param classification an array of ray states corresponding to the rays in the RayList
   */
  virtual void HandleTerminatedRays(RayList *raylist, int *classification);

  virtual int SerialSize(); //!< return the size in bytes for the serialization of this Renderer
  virtual unsigned char *Serialize(unsigned char *); //!< serialize this Renderer to the given byte array
  virtual unsigned char *Deserialize(unsigned char *); //!< deserialize a Renderer from the given byte array into this object

  //! broadcasts a RenderMsg to all processes to begin rendering via each localRendering method
	virtual void Render(RenderingSetP);
  //! return the frame number for the current render
	int GetFrame() { return frame; }

	void DumpStatistics(); //!< broadcast a StatisticsMsg to all processes to write rendering stats to local file via _dumpStats()
	void _dumpStats(); //!< write local rendering statistics to file via APP_LOG()

  //! log that n rays were sent to process d
	void _sent_to(int d, int n)
	{
		pthread_mutex_lock(&lock);
		sent_to[d] += n;
		pthread_mutex_unlock(&lock);
	}

  //! log that n rays were received from process d
	void _received_from(int d, int n)
	{
		pthread_mutex_lock(&lock);
		received_from[d] += n;
		pthread_mutex_unlock(&lock);
	}

  //! log that n rays originated at this process
	void add_originated_ray_count(int n)
	{
		pthread_mutex_lock(&lock);
		originated_ray_count += n;
		pthread_mutex_unlock(&lock);
	}

  //! return a pointer to the Lighting singleton for this process
	Lighting *get_the_lights() { return &lighting; }

  //! process the given RayList using the current Rendering, RenderingSet, and Visualization
	void Trace(RayList *);

  //! return the maximum number of rays allowed in each RayList
	int GetMaxRayListSize() { return max_rays_per_packet; }

  // These defines categorize rays after a pass through the tracer
  // TODO: reimplement as enum
  static int TERMINATED;  //!< mark that this ray has been terminated
  static int DROP_ON_FLOOR; //!< mark that this ray should be discarded
  static int KEEP_HERE; //!< mark that this ray should be kept at this process
  static int UNDETERMINED; //!< mark that no action has yet been determined for this ray
  static int NO_NEIGHBOR; //!< mark that this ray has no neighbor to be sent to and has left the global volume
    // This define indicates that a ray exiting a partition face 
    // exits everything

private:

	std::vector<std::future<void>> rvec;

	int frame;

	int max_rays_per_packet;

	int sent_ray_count;
	int terminated_ray_count;
	int originated_ray_count;
	int secondary_ray_count;
	int sent_pixels_count;
	int ProcessRays_input_count;
	int ProcessRays_continued_count;
	int *sent_to;
	int *received_from;

  TraceRays tracer;
  Lighting  lighting;
  RayQManager *rayQmanager;

  pthread_mutex_t lock;
  pthread_cond_t cond; 
  
  class StatisticsMsg : public Work
  {
  public:
    StatisticsMsg(Renderer *r) : StatisticsMsg(sizeof(Key))
		{
			unsigned char *p = (unsigned char *)contents->get();
			*(Key *)p = r->getkey();
		}

    WORK_CLASS(StatisticsMsg, true);

  public:
		bool CollectiveAction(MPI_Comm coll_comm, bool isRoot);
  };

public:
  //! a Work unit to send rays between Galaxy processes
  class SendRaysMsg : public Work
  {
  public:
    SendRaysMsg(RayList *rl) : SendRaysMsg(rl->get_ptr()) {};
    WORK_CLASS(SendRaysMsg, false);

  public:
    bool Action(int sender);
  };

#ifdef GXY_WRITE_IMAGES
  //! a Work unit to acknowledge the receipt of rays from another process
  class AckRaysMsg : public Work
  {
  public:
    AckRaysMsg(RenderingSetP rs);
    
    WORK_CLASS(AckRaysMsg, false);

  public:
    bool Action(int sender);
  };

#endif // GXY_WRITE_IMAGES

  //! a Work unit to send pixel contributions to another process
  class SendPixelsMsg : public Work
  {
  private:
    struct pix 
    {
      int x, y;
      float r, g, b, o;
    };

		struct hdr
		{
			Key rkey;
			Key rskey;
			int count;
			int frame;
			int source;
		};

		RenderingSetP rset;
    
  public:
    SendPixelsMsg(RenderingP r, RenderingSetP rs, int frame, int n) : SendPixelsMsg(sizeof(hdr) + (n * sizeof(Pixel)))
    {
			rset = rs;

      hdr *h    = (hdr *)contents->get();
			h->rkey   = r->getkey();
			h->rskey  = rs->getkey();
			h->frame  = frame;
			h->count  = n;
			h->source = GetTheApplication()->GetRank();

      nxt = 0;
    }

		void Send(int i)
		{
      hdr *h    = (hdr *)contents->get();

#ifdef GXY_EVENT_TRACKING
			GetTheEventTracker()->Add(new SendPixelsEvent(h->count, h->rkey, h->frame, i));
#endif

			Work::Send(i);

#ifdef GXY_WRITE_IMAGES
      rset->SentPixels(h->count);
#endif
		}

    void StashPixel(RayList *rl, int i) 
    {
      Pixel *p = (Pixel *)((((unsigned char *)contents->get()) + sizeof(hdr))) + nxt++;

      p->x = rl->get_x(i);
      p->y = rl->get_y(i);
      p->r = rl->get_r(i);
      p->g = rl->get_g(i);
      p->b = rl->get_b(i);
      p->o = rl->get_o(i);
    }

    WORK_CLASS(SendPixelsMsg, false);

  public:
    bool Action(int s)
    {
			hdr *h = (hdr *)contents->get();
      Pixel *pixels = (Pixel *)(((unsigned char *)contents->get()) + sizeof(hdr));

      RenderingP r = Rendering::GetByKey(h->rkey);
      if (! r)
        return false;

      RenderingSetP rs = RenderingSet::GetByKey(h->rskey);
      if (! rs)
        return false;

#ifdef GXY_WRITE_IMAGES
      rs->ReceivedPixels(h->count);
#endif

#ifdef GXY_EVENT_TRACKING
			GetTheEventTracker()->Add(new RcvPixelsEvent(h->count, h->rkey, h->frame, s));
#endif

			if (rs->IsActive(h->frame))
				r->AddLocalPixels(pixels, h->count, h->frame, h->source);

      return false;
    }

  private:
    int nxt;
  };

  //! a Work unit to instruct Galaxy processes to begin rendering
  class RenderMsg : public Work
  {
  public:
    RenderMsg(Renderer *, RenderingSetP);
    ~RenderMsg();

    WORK_CLASS(RenderMsg, true);

    bool CollectiveAction(MPI_Comm, bool isRoot);

	private:
		int frame;
  };
};

} // namespace gxy
