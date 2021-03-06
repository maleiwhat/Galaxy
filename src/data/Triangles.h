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

/*! \file Triangles.h 
 * \brief a triangle (tessellated) dataset within Galaxy
 * \ingroup data
 */

#include <string>
#include <string.h>
#include <memory.h>

#include "Application.h"
#include "Box.h"
#include "Datasets.h"
#include "dtypes.h"
#include "Geometry.h"
#include "KeyedDataObject.h"

#include "rapidjson/document.h"
#include "ospray/ospray.h"

namespace gxy
{

KEYED_OBJECT_POINTER(Triangles)

//! a triangle (tessellated) dataset within Galaxy
/* \ingroup data 
 * \sa KeyedObject, KeyedDataObject, OSPRayObject
 */
class Triangles : public Geometry
{
  KEYED_OBJECT_SUBCLASS(Triangles, Geometry)

public:
	void initialize(); //!< initialize this Triangles object
	virtual ~Triangles(); //!< default destructor 

  //! commit this object to the local registry
  /*! This action is performed in response to a CommitMsg */
  virtual bool local_commit(MPI_Comm);
  //! import the given data file into local memory
  /*! This action is performed in response to a ImportMsg */
  virtual void local_import(char *, MPI_Comm);

  //! construct a Triangles from a Galaxy JSON specification
  virtual void LoadFromJSON(rapidjson::Value&);
  //! save this Triangles to a Galaxy JSON specification 
  virtual void SaveToJSON(rapidjson::Value&, rapidjson::Document&);

private:
	int n_triangles;
	int n_vertices;
	float *vertices;
	float *normals;
	int *triangles;
};

} // namespace gxy
