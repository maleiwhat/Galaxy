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

/*! \file Visualization.h 
 * \brief a visualization of one or more visualization elements (Vis objects) in Galaxy
 * \ingroup data
 */

#include "Box.h"
#include "Datasets.h"
#include "KeyedDataObject.h"
#include "KeyedObject.h"
#include "Lighting.h"
#include "MappedVis.h"
#include "ParticlesVis.h"
#include "TrianglesVis.h"
#include "VolumeVis.h"

#include <ospray/ospray.h>

#include <map>

namespace gxy
{

KEYED_OBJECT_POINTER(Visualization)

//! a visualization of one or more visualization elements (Vis objects) in Galaxy
/* This object represents a combination of one or more visualization elements (Vis objects)
 * combined with lighting information for use in rendering
 * \ingroup data 
 * \sa Vis, KeyedObject, ISPCObject, OSPRayObject
 */
class Visualization : public KeyedObject, public ISPCObject
{
  KEYED_OBJECT(Visualization);

  using vis_t = std::vector<VisP>;

public:
  virtual ~Visualization(); //!< default destructor
  virtual void initialize(); //!< initialize this Visualization object

  //! load a set of Visualization objects from a Galaxy JSON specification
  static std::vector<VisualizationP> LoadVisualizationsFromJSON(rapidjson::Value&);

  //! commit this object to the global registry across all processes
  virtual void Commit(DatasetsP);
  //! commit this object to the local registry
  virtual bool local_commit(MPI_Comm);
  //! remove this object from the global registry
  virtual void Drop();

  //! Add a TrianglesVis object to this Visualization
  void AddOsprayGeometryVis(VisP g);
  //! Add a MappedVis object to this Visualization
  void AddMappedGeometryVis(VisP m);
  //! Add a VolumeVis object to this Visualization
  void AddVolumeVis(VisP v);

  //! save this Vis to a Galaxy JSON specification 
  virtual void SaveToJSON(rapidjson::Value&, rapidjson::Document&);
  //! construct a Visualization from a Galaxy JSON specification
  void LoadFromJSON(rapidjson::Value&);

  //! set the annotation string for this Visualization
  void  SetAnnotation(std::string a) { annotation = a; }
  //! get the current annotation string for this Visualization
  const char *GetAnnotation() { return annotation.c_str(); }

  //! get the OSPRay OSPModel for this Visualization
  OSPModel GetTheModel() { return ospModel; }

  //! get the Box that represents the global data extent for this Visualization
  Box *get_global_box() { return &global_box; }
  //! get the Box that represents the local data extent at this process for this Visualization
  Box *get_local_box() { return &local_box; }

  //! return the data key that is the `i^th` neighbor of this proces
  /*! This method uses the Box face orientation indices for neighbor indexing
   *          - yz-face neighbors - `0` for the lower (left) `x`, `1` for the higher (right) `x`
   *          - xz-face neighbors - `2` for the lower (left) `y`, `3` for the higher (right) `y`
   *          - xy-face neighbors - `4` for the lower (left) `z`, `5` for the higher (right) `z`
   */
  int get_neighbor(int i) { return neighbors[i]; }
  //! return true if the requested neighbor exists
  /*! This method uses the Box face orientation indices for neighbor indexing
   *          - yz-face neighbors - `0` for the lower (left) `x`, `1` for the higher (right) `x`
   *          - xz-face neighbors - `2` for the lower (left) `y`, `3` for the higher (right) `y`
   *          - xy-face neighbors - `4` for the lower (left) `z`, `5` for the higher (right) `z`
   */
  bool has_neighbor(unsigned int face) { return neighbors[face] >= 0; }

  //! get a pointer to the Lighting object for this Visualization
	Lighting *get_the_lights() { return &lighting; }

protected:
	Lighting lighting;

  virtual void allocate_ispc();
  virtual void initialize_ispc();
  virtual void destroy_ispc();

  OSPModel ospModel;

  virtual int serialSize();
  virtual unsigned char *serialize(unsigned char *);
  virtual unsigned char *deserialize(unsigned char *);

  std::string annotation;

  vis_t osprayGeometries;
  vis_t mappedGeometries;
  vis_t volumes;

  Box global_box;
  Box local_box;
  int neighbors[6];
};

} // namespace gxy
