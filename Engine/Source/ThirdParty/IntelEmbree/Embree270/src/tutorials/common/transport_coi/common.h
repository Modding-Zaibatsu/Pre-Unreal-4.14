// ======================================================================== //
// Copyright 2009-2015 Intel Corporation                                    //
//                                                                          //
// Licensed under the Apache License, Version 2.0 (the "License");          //
// you may not use this file except in compliance with the License.         //
// You may obtain a copy of the License at                                  //
//                                                                          //
//     http://www.apache.org/licenses/LICENSE-2.0                           //
//                                                                          //
// Unless required by applicable law or agreed to in writing, software      //
// distributed under the License is distributed on an "AS IS" BASIS,        //
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. //
// See the License for the specific language governing permissions and      //
// limitations under the License.                                           //
// ======================================================================== //

#pragma once

#include "../../../common/math/vec3.h"

namespace embree
{
  struct InitData 
  {
    char cfg[1024];
  };

  struct KeyPressedData
  {
    int key;
  };

  struct CreateSceneData 
  {
    int numMaterials;
    int numMeshes;
    int numHairSets;
    int numAmbientLights;
    int numPointLights;
    int numDirectionalLights;
    int numDistantLights;
    int numSubdivMeshes;
  };

  struct CreateMeshData
  {
    int numVertices;
    int numTriangles;
    int numQuads;
    int meshMaterialID;
  };

  struct CreateSubdivMeshData
  {
    int numPositions;
    int numNormals;
    int numTextureCoords;
    int numPositionIndices;
    int numNormalIndices;
    int numTexCoordIndices;
    int numVerticesPerFace;
    int numHoles;
    int numEdgeCreases;
    int numEdgeCreaseWeights;
    int numVertexCreases;
    int numVertexCreaseWeights;
    int materialID;
  };

  struct CreateHairSetData
  {
    int numVertices;
    int numHairs;
  };

  struct ResizeData {
    int width, height;
  };

  struct PickDataSend {
    float x,y;
    Vec3fa vx,vy,vz,p;
  };

  struct PickDataReceive {
    Vec3fa pos;
    bool hit;
  };

  struct RenderData {
    float time;
    Vec3fa vx,vy,vz,p;
    int width;
    int height;
  };
}
