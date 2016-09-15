// ======================================================================== //
// Copyright 2009-2016 Intel Corporation                                    //
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

#include "../math/sampling.h"
#include "../lights/light.h"
#include "../lights/ambient_light.h"
#include "../lights/directional_light.h"
#include "../lights/point_light.h"
#include "../lights/quad_light.h"
#include "../lights/spot_light.h"
#include "scene.h"

namespace embree
{
  struct ISPCTriangle
  {
    unsigned v0;                /*< first triangle vertex */
    unsigned v1;                /*< second triangle vertex */
    unsigned v2;                /*< third triangle vertex */
    unsigned materialID;        /*< material of triangle */
  };

  struct ISPCQuad
  {
    unsigned v0;                /*< first triangle vertex */
    unsigned v1;                /*< second triangle vertex */
    unsigned v2;                /*< third triangle vertex */
    unsigned v3;                /*< fourth triangle vertex */
  };

  struct ISPCHair
  {
    unsigned vertex;
    unsigned id;
  };

  enum ISPCType { TRIANGLE_MESH, SUBDIV_MESH, HAIR_SET, INSTANCE, GROUP, QUAD_MESH, LINE_SEGMENTS, CURVES };

  struct ISPCMaterial
  {
    int ty;
    int align0,align1,align2;
    Vec3fa v[7];
  };

  enum TEXTURE_FORMAT {
    Texture_RGBA8        = 1,
    Texture_RGB8         = 2,
    Texture_FLOAT32      = 3,
  };

  struct ISPCScene
  {
    struct ISPCGeometry
    {
      ISPCGeometry (ISPCType type) : type(type) {}

      ISPCType type;
    };

    struct ISPCTriangleMesh
    {
      ISPCTriangleMesh (unsigned int numTriangles,
                        unsigned int numQuads,
                        unsigned int numVertices,
                        int meshMaterialID)
      : geom(TRIANGLE_MESH), positions(nullptr), positions2(nullptr), normals(nullptr), texcoords(nullptr), triangles(nullptr),
        numVertices(numVertices), numTriangles(numTriangles), numQuads(numQuads), geomID(0), meshMaterialID(meshMaterialID) {}

      ISPCTriangleMesh (Ref<TutorialScene::TriangleMesh> in) : geom(TRIANGLE_MESH)
      {
        positions = in->v.data();
        positions2 = in->v2.data();
        normals = in->vn.data();
        texcoords = in->vt.data();
        triangles = (ISPCTriangle*) in->triangles.data();
        numVertices = unsigned(in->v.size());
        numTriangles = unsigned(in->triangles.size());
        geomID = -1;
        meshMaterialID = in->meshMaterialID;
      }

    public:
      ISPCGeometry geom;
      Vec3fa* positions;    //!< vertex position array
      Vec3fa* positions2;    //!< vertex position array
      Vec3fa* normals;       //!< vertex normal array
      Vec2f* texcoords;     //!< vertex texcoord array
      ISPCTriangle* triangles;  //!< list of triangles
      unsigned int numVertices;
      unsigned int numTriangles;
      unsigned int numQuads;
      unsigned int geomID;
      unsigned int meshMaterialID;
    };
    
    struct ISPCQuadMesh
    {
      ISPCQuadMesh ()
      : geom(QUAD_MESH), positions(nullptr), positions2(nullptr), normals(nullptr), texcoords(nullptr), quads(nullptr),
        numVertices(0), numQuads(0), geomID(0), meshMaterialID(0) {}

      ISPCQuadMesh (Ref<TutorialScene::QuadMesh> in) : geom(QUAD_MESH)
      {
        positions = in->v.data();
        positions2 = in->v2.data();
        normals = in->vn.data();
        texcoords = in->vt.data();
        quads = (ISPCQuad*) in->quads.data();
        numVertices = unsigned(in->v.size());
        numQuads = unsigned(in->quads.size());
        geomID = -1;
        meshMaterialID = in->meshMaterialID;
      }

    public:
      ISPCGeometry geom;
      Vec3fa* positions;    //!< vertex position array
      Vec3fa* positions2;    //!< vertex position array
      Vec3fa* normals;       //!< vertex normal array
      Vec2f* texcoords;     //!< vertex texcoord array
      ISPCQuad* quads;      //!< list of quads
      unsigned int numVertices;
      unsigned int numQuads;
      unsigned int geomID;
      unsigned int meshMaterialID;
    };

    struct ISPCSubdivMesh
    {
      ISPCSubdivMesh (unsigned int numVertices, unsigned int numFaces, unsigned int numEdges, unsigned int materialID)
      : geom(SUBDIV_MESH), positions(nullptr), positions2(nullptr), normals(nullptr), texcoords(nullptr), position_indices(nullptr), normal_indices(nullptr), texcoord_indices(nullptr), verticesPerFace(nullptr), holes(nullptr),
        subdivlevel(nullptr), edge_creases(nullptr), edge_crease_weights(nullptr), vertex_creases(nullptr), vertex_crease_weights(nullptr), face_offsets(nullptr),
        numVertices(numVertices), numFaces(numFaces), numEdges(numEdges), numEdgeCreases(0), numVertexCreases(0), numHoles(0), materialID(materialID), geomID(0) {}

      ISPCSubdivMesh (Ref<TutorialScene::SubdivMesh> in) : geom(SUBDIV_MESH)
      {
        positions = in->positions.data();
        positions2 = in->positions2.data();
        normals = in->normals.data();
        texcoords = in->texcoords.data();
        position_indices = in->position_indices.data();
        normal_indices = in->normal_indices.data();
        texcoord_indices = in->texcoord_indices.data();
        verticesPerFace = in->verticesPerFace.data();
        holes = in->holes.data();
        edge_creases = in->edge_creases.data();
        edge_crease_weights = in->edge_crease_weights.data();
        vertex_creases = in->vertex_creases.data();
        vertex_crease_weights = in->vertex_crease_weights.data();
        numVertices = unsigned(in->positions.size());
        numFaces = unsigned(in->verticesPerFace.size());
        numEdges = unsigned(in->position_indices.size());
        numEdgeCreases = unsigned(in->edge_creases.size());
        numVertexCreases = unsigned(in->vertex_creases.size());
        numHoles = unsigned(in->holes.size());
        materialID = in->materialID;
        geomID = -1;

        size_t numEdges = in->position_indices.size();
        size_t numFaces = in->verticesPerFace.size();
        subdivlevel = new float[numEdges]; // FIXME: never deleted
        face_offsets = new unsigned[numFaces]; // FIXME: never deleted
        for (size_t i=0; i<numEdges; i++) subdivlevel[i] = 1.0f;
        int offset = 0;
        for (size_t i=0; i<numFaces; i++)
        {
          face_offsets[i] = offset;
          offset+=verticesPerFace[i];
        }
      }

    public:
      ISPCGeometry geom;
      Vec3fa* positions;       //!< vertex positions
      Vec3fa* positions2;      //!< vertex positions for second time step
      Vec3fa* normals;         //!< face vertex normals
      Vec2f* texcoords;        //!< face texture coordinates
      unsigned* position_indices;   //!< position indices for all faces
      unsigned* normal_indices;     //!< normal indices for all faces
      unsigned* texcoord_indices;   //!< texcoord indices for all faces
      unsigned* verticesPerFace;    //!< number of indices of each face
      unsigned* holes;              //!< face ID of holes
      float* subdivlevel;      //!< subdivision level
      Vec2i* edge_creases;          //!< crease index pairs
      float* edge_crease_weights;   //!< weight for each crease
      unsigned* vertex_creases;          //!< indices of vertex creases
      float* vertex_crease_weights; //!< weight for each vertex crease
      unsigned* face_offsets;
      unsigned int numVertices;
      unsigned int numFaces;
      unsigned int numEdges;
      unsigned int numEdgeCreases;
      unsigned int numVertexCreases;
      unsigned int numHoles;
      unsigned int materialID;
      unsigned int geomID;
    };

    struct ISPCLineSegments
    {
      ISPCLineSegments ()
      : geom(LINE_SEGMENTS), v(nullptr), v2(nullptr), indices(nullptr),
        numVertices(0), numSegments(0), materialID(0) {}

      ISPCLineSegments (Ref<TutorialScene::LineSegments> in) : geom(LINE_SEGMENTS)
      {
        v = in->v.data();
        v2 = in->v2.data();
        indices = in->indices.data();
        numVertices = unsigned(in->v.size());
        numSegments = unsigned(in->indices.size());
        materialID = in->materialID;
      }

      ISPCGeometry geom;
      Vec3fa* v;        //!< control points (x,y,z,r)
      Vec3fa* v2;       //!< control points (x,y,z,r)
      unsigned* indices;     //!< for each segment, index to first control point
      unsigned int numVertices;
      unsigned int numSegments;
      unsigned int materialID;
    };

    struct ISPCHairSet
    {
      ISPCHairSet (unsigned int numHairs, unsigned int numVertices, int materialID)
      : geom(HAIR_SET), v(nullptr), v2(nullptr), hairs(nullptr),
        numVertices(numVertices), numHairs(numHairs), materialID(materialID) {}

      ISPCHairSet (bool hair, Ref<TutorialScene::HairSet> in) : geom(hair ? HAIR_SET : CURVES)
      {
        v = in->v.data();
        v2 = in->v2.data();
        hairs = (ISPCHair*) in->hairs.data();
        numVertices = unsigned(in->v.size());
        numHairs = unsigned(in->hairs.size());
        materialID = in->materialID;
      }

      ISPCGeometry geom;
      Vec3fa* v;       //!< hair control points (x,y,z,r)
      Vec3fa* v2;       //!< hair control points (x,y,z,r)
      ISPCHair* hairs; //!< for each hair, index to first control point
      unsigned int numVertices;
      unsigned int numHairs;
      unsigned int materialID;
    };

    struct ISPCInstance
    {
      ALIGNED_STRUCT;

      ISPCInstance ()
      : geom(INSTANCE), space0(one), space1(one), geomID(0) {}

      ISPCInstance (Ref<TutorialScene::Instance> in)
      : geom(INSTANCE), space0(in->space0), space1(in->space1), geomID(in->geomID) {}

      ISPCGeometry geom;
      AffineSpace3fa space0;
      AffineSpace3fa space1;
      unsigned int geomID;
    };

    struct ISPCGroup
    {
      ISPCGroup (Ref<TutorialScene::Group> in)
      : geom(GROUP)
      {
        numGeometries = in->size();
        geometries = new ISPCGeometry*[numGeometries];
        for (size_t i=0; i<numGeometries; i++)
          geometries[i] = ISPCScene::convertGeometry(in->at(i));
      }

      ~ISPCGroup()
      {
        for (size_t i=0; i<numGeometries; i++)
          delete geometries[i];
        delete[] geometries;
      }

    public:
      ISPCGeometry geom;
      ISPCGeometry** geometries;
      size_t numGeometries;
    };

    ISPCScene(TutorialScene* in)
    {
      geometries = new ISPCGeometry*[in->geometries.size()];
      for (size_t i=0; i<in->geometries.size(); i++)
        geometries[i] = convertGeometry(in->geometries[i]);
      numGeometries = unsigned(in->geometries.size());

      materials = (ISPCMaterial*) (in->materials.size() ? &in->materials[0] : nullptr);
      numMaterials = unsigned(in->materials.size());

      lights = new Light*[in->lights.size()];
      numLights = 0;
      for (size_t i = 0; i < in->lights.size(); i++)
      {
        Light* light = convertLight(in->lights[i]);
        if (light) lights[numLights++] = light;
      }
    }

    static ISPCGeometry* convertGeometry (Ref<TutorialScene::Geometry> in)
    {
      if (in->type == TutorialScene::Geometry::TRIANGLE_MESH)
        return (ISPCGeometry*) new ISPCTriangleMesh(in.dynamicCast<TutorialScene::TriangleMesh>());
      else if (in->type == TutorialScene::Geometry::QUAD_MESH)
        return (ISPCGeometry*) new ISPCQuadMesh(in.dynamicCast<TutorialScene::QuadMesh>());
      else if (in->type == TutorialScene::Geometry::SUBDIV_MESH)
        return (ISPCGeometry*) new ISPCSubdivMesh(in.dynamicCast<TutorialScene::SubdivMesh>());
      else if (in->type == TutorialScene::Geometry::LINE_SEGMENTS)
        return (ISPCGeometry*) new ISPCLineSegments(in.dynamicCast<TutorialScene::LineSegments>());
      else if (in->type == TutorialScene::Geometry::HAIR_SET)
        return (ISPCGeometry*) new ISPCHairSet(true,in.dynamicCast<TutorialScene::HairSet>());
      else if (in->type == TutorialScene::Geometry::CURVES)
        return (ISPCGeometry*) new ISPCHairSet(false,in.dynamicCast<TutorialScene::HairSet>());
      else if (in->type == TutorialScene::Geometry::INSTANCE)
        return (ISPCGeometry*) new ISPCInstance(in.dynamicCast<TutorialScene::Instance>());
      else if (in->type == TutorialScene::Geometry::GROUP)
        return (ISPCGeometry*) new ISPCGroup(in.dynamicCast<TutorialScene::Group>());
      else
        THROW_RUNTIME_ERROR("unknown geometry type");
    }

    static Light* convertLight(Ref<SceneGraph::Light> in)
    {
      void* out = 0;

      switch (in->getType())
      {
      case SceneGraph::LIGHT_AMBIENT:
      {
        Ref<SceneGraph::AmbientLight> inAmbient = in.dynamicCast<SceneGraph::AmbientLight>();
        out = AmbientLight_create();
        AmbientLight_set(out, inAmbient->L);
        break;
      }
      case SceneGraph::LIGHT_DIRECTIONAL:
      {
        Ref<SceneGraph::DirectionalLight> inDirectional = in.dynamicCast<SceneGraph::DirectionalLight>();
        out = DirectionalLight_create();
        DirectionalLight_set(out, -normalize(inDirectional->D), inDirectional->E, 1.0f);
        break;
      }
      case SceneGraph::LIGHT_DISTANT:
      {
        Ref<SceneGraph::DistantLight> inDistant = in.dynamicCast<SceneGraph::DistantLight>();
        out = DirectionalLight_create();
        DirectionalLight_set(out,
                             -normalize(inDistant->D),
                             inDistant->L * rcp(uniformSampleConePDF(inDistant->cosHalfAngle)),
                             inDistant->cosHalfAngle);
        break;
      }
      case SceneGraph::LIGHT_POINT:
      {
        Ref<SceneGraph::PointLight> inPoint = in.dynamicCast<SceneGraph::PointLight>();
        out = PointLight_create();
        PointLight_set(out, inPoint->P, inPoint->I, 0.f);
        break;
      }
      case SceneGraph::LIGHT_SPOT:
      case SceneGraph::LIGHT_TRIANGLE:
      case SceneGraph::LIGHT_QUAD:
      {
        // FIXME: not implemented yet
        break;
      }

      default:
        THROW_RUNTIME_ERROR("unknown light type");
      }

      return (Light*)out;
    }

  public:
    ISPCGeometry** geometries;   //!< list of geometries
    ISPCMaterial* materials;     //!< material list
    unsigned int numGeometries;           //!< number of geometries
    unsigned int numMaterials;            //!< number of materials

    Light** lights;              //!< list of lights
    unsigned int numLights;               //!< number of lights
  };

  typedef ISPCScene::ISPCGeometry ISPCGeometry;
  typedef ISPCScene::ISPCTriangleMesh ISPCTriangleMesh;
  typedef ISPCScene::ISPCQuadMesh ISPCQuadMesh;
  typedef ISPCScene::ISPCSubdivMesh ISPCSubdivMesh;
  typedef ISPCScene::ISPCLineSegments ISPCLineSegments;
  typedef ISPCScene::ISPCHairSet ISPCHairSet;
  typedef ISPCScene::ISPCInstance ISPCInstance;
  typedef ISPCScene::ISPCGroup ISPCGroup;
}
