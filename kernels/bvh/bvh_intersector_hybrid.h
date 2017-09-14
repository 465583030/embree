// ======================================================================== //
// Copyright 2009-2017 Intel Corporation                                    //
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

#include "bvh.h"
#include "../common/ray.h"
#include "../common/stack_item.h"

namespace embree
{
  namespace isa 
  {
    /*! BVH hybrid packet intersector. Switches between packet and single ray traversal (optional). */
    template<int N, int K, int types, bool robust, typename PrimitiveIntersectorK, bool single = true>
    class BVHNIntersectorKHybrid
    {
      /* right now AVX512KNL SIMD extension only for standard node types */
      static const size_t Nx = types == BVH_AN1 ? vextend<N>::size : N;

      /* shortcuts for frequently used types */
      typedef typename PrimitiveIntersectorK::Precalculations Precalculations;
      typedef typename PrimitiveIntersectorK::Primitive Primitive;
      typedef BVHN<N> BVH;
      typedef typename BVH::NodeRef NodeRef;
      typedef typename BVH::BaseNode BaseNode;
      typedef typename BVH::AlignedNode AlignedNode;
      typedef typename BVH::AlignedNodeMB AlignedNodeMB;
      
      static const size_t stackSizeSingle = 1+(N-1)*BVH::maxDepth+3; // +3 due to 16-wide store
      static const size_t stackSizeChunk = 1+(N-1)*BVH::maxDepth;

      static const size_t switchThresholdIncoherent = \
      (K==4)  ? 3 :
      (K==8)  ? ((N==4) ? 5 : 7) :
      (K==16) ? 14 : // 14 seems to work best for KNL due to better ordered chunk traversal
      0;


      /* Optimized frustum test. We calculate t=(p-org)/dir in ray/box
       * intersection. We assume the rays are split by octant, thus
       * dir intervals are either positive or negative in each
       * dimension.

         Case 1: dir.min >= 0 && dir.max >= 0:
           t_min = (p_min - org_max) / dir_max = (p_min - org_max)*rdir_min = p_min*rdir_min - org_max*rdir_min
           t_max = (p_max - org_min) / dir_min = (p_max - org_min)*rdir_max = p_max*rdir_max - org_min*rdir_max

         Case 2: dir.min < 0 && dir.max < 0:
           t_min = (p_max - org_min) / dir_min = (p_max - org_min)*rdir_max = p_max*rdir_max - org_min*rdir_max
           t_max = (p_min - org_max) / dir_max = (p_min - org_max)*rdir_min = p_min*rdir_min - org_max*rdir_min
      */
      
      struct Frustum
      {
        __forceinline Frustum(const vbool<K>  &valid,
                              const Vec3vf<K> &org,
                              const Vec3vf<K> &rdir,
                              const vfloat<K> &ray_tnear,
                              const vfloat<K> &ray_tfar)
        {
          const Vec3fa reduced_min_org( reduce_min(select(valid,org.x,pos_inf)),
                                        reduce_min(select(valid,org.y,pos_inf)),
                                        reduce_min(select(valid,org.z,pos_inf)) );
          const Vec3fa reduced_max_org( reduce_max(select(valid,org.x,neg_inf)),
                                        reduce_max(select(valid,org.y,neg_inf)),
                                        reduce_max(select(valid,org.z,neg_inf)) );
          
          const Vec3fa reduced_min_rdir( reduce_min(select(valid,rdir.x,pos_inf)),
                                         reduce_min(select(valid,rdir.y,pos_inf)),
                                         reduce_min(select(valid,rdir.z,pos_inf)) );
          const Vec3fa reduced_max_rdir( reduce_max(select(valid,rdir.x,neg_inf)),
                                         reduce_max(select(valid,rdir.y,neg_inf)),
                                         reduce_max(select(valid,rdir.z,neg_inf)) );

          min_rdir = select(ge_mask(reduced_min_rdir, Vec3fa(zero)), reduced_min_rdir, reduced_max_rdir);
          max_rdir = select(ge_mask(reduced_min_rdir, Vec3fa(zero)), reduced_max_rdir, reduced_min_rdir);

          if (!robust)
          {
            min_org_rdir = min_rdir * select(ge_mask(reduced_min_rdir, Vec3fa(zero)), reduced_max_org, reduced_min_org);
            max_org_rdir = max_rdir * select(ge_mask(reduced_min_rdir, Vec3fa(zero)), reduced_min_org, reduced_max_org);
          }
          else
          {
            min_org_rdir = select(ge_mask(reduced_min_rdir, Vec3fa(zero)), reduced_max_org, reduced_min_org);
            max_org_rdir = select(ge_mask(reduced_min_rdir, Vec3fa(zero)), reduced_min_org, reduced_max_org);
          }

          min_dist = reduce_min(select(valid,ray_tnear,vfloat<K>(pos_inf)));
          max_dist = reduce_max(select(valid,ray_tfar ,vfloat<K>(neg_inf)));

          nearX = (min_rdir.x < 0.0f) ? 1*sizeof(vfloat<N>) : 0*sizeof(vfloat<N>);
          nearY = (min_rdir.y < 0.0f) ? 3*sizeof(vfloat<N>) : 2*sizeof(vfloat<N>);
          nearZ = (min_rdir.z < 0.0f) ? 5*sizeof(vfloat<N>) : 4*sizeof(vfloat<N>);
          farX  = nearX ^ sizeof(vfloat<N>);
          farY  = nearY ^ sizeof(vfloat<N>);
          farZ  = nearZ ^ sizeof(vfloat<N>);
        }

        __forceinline unsigned int intersect(const NodeRef &nodeRef, float * const __restrict__ dist) const
        {
          /* only default alignedNodes are currently supported */
          const AlignedNode* __restrict__ const node = nodeRef.alignedNode();
          
          const vfloat<Nx> bminX = *(const vfloat<N>*)((const char*)&node->lower_x + nearX);
          const vfloat<Nx> bminY = *(const vfloat<N>*)((const char*)&node->lower_x + nearY);
          const vfloat<Nx> bminZ = *(const vfloat<N>*)((const char*)&node->lower_x + nearZ);
          const vfloat<Nx> bmaxX = *(const vfloat<N>*)((const char*)&node->lower_x + farX);
          const vfloat<Nx> bmaxY = *(const vfloat<N>*)((const char*)&node->lower_x + farY);
          const vfloat<Nx> bmaxZ = *(const vfloat<N>*)((const char*)&node->lower_x + farZ);
                    
          if (robust)
          {
            const vfloat<Nx> fminX = (bminX - vfloat<Nx>(min_org_rdir.x)) * vfloat<Nx>(min_rdir.x);
            const vfloat<Nx> fminY = (bminY - vfloat<Nx>(min_org_rdir.y)) * vfloat<Nx>(min_rdir.y);
            const vfloat<Nx> fminZ = (bminZ - vfloat<Nx>(min_org_rdir.z)) * vfloat<Nx>(min_rdir.z);
            const vfloat<Nx> fmaxX = (bmaxX - vfloat<Nx>(max_org_rdir.x)) * vfloat<Nx>(max_rdir.x);
            const vfloat<Nx> fmaxY = (bmaxY - vfloat<Nx>(max_org_rdir.y)) * vfloat<Nx>(max_rdir.y);
            const vfloat<Nx> fmaxZ = (bmaxZ - vfloat<Nx>(max_org_rdir.z)) * vfloat<Nx>(max_rdir.z);

            const float round_down = 1.0f-2.0f*float(ulp); // FIXME: use per instruction rounding for AVX512
            const float round_up   = 1.0f+2.0f*float(ulp);
            const vfloat<Nx> fmin  = max(fminX, fminY, fminZ, vfloat<Nx>(min_dist)); 
            vfloat<Nx>::store(dist,fmin);
            const vfloat<Nx> fmax  = min(fmaxX, fmaxY, fmaxZ, vfloat<Nx>(max_dist));
            const vbool<Nx> vmask_node_hit = (round_down*fmin <= round_up*fmax);
            size_t m_node = movemask(vmask_node_hit) & (((size_t)1 << N)-1);
            return m_node;          
          }
          else
          {
            const vfloat<Nx> fminX = msub(bminX, vfloat<Nx>(min_rdir.x), vfloat<Nx>(min_org_rdir.x));
            const vfloat<Nx> fminY = msub(bminY, vfloat<Nx>(min_rdir.y), vfloat<Nx>(min_org_rdir.y));
            const vfloat<Nx> fminZ = msub(bminZ, vfloat<Nx>(min_rdir.z), vfloat<Nx>(min_org_rdir.z));
            const vfloat<Nx> fmaxX = msub(bmaxX, vfloat<Nx>(max_rdir.x), vfloat<Nx>(max_org_rdir.x));
            const vfloat<Nx> fmaxY = msub(bmaxY, vfloat<Nx>(max_rdir.y), vfloat<Nx>(max_org_rdir.y));
            const vfloat<Nx> fmaxZ = msub(bmaxZ, vfloat<Nx>(max_rdir.z), vfloat<Nx>(max_org_rdir.z));

            const vfloat<Nx> fmin  = maxi(fminX, fminY, fminZ, vfloat<Nx>(min_dist)); 
            vfloat<Nx>::store(dist,fmin);
            const vfloat<Nx> fmax  = mini(fmaxX, fmaxY, fmaxZ, vfloat<Nx>(max_dist));
            const vbool<Nx> vmask_node_hit = fmin <= fmax;
            size_t m_node = movemask(vmask_node_hit) & (((size_t)1 << N)-1);
            return m_node;          
          }
        }

        __forceinline void updateMaxDist(const vfloat<K> &ray_tfar)
        {
          max_dist = reduce_max(ray_tfar);
        }

        size_t nearX, nearY, nearZ;
        size_t farX, farY, farZ;

        Vec3fa min_rdir; 
        Vec3fa max_rdir;
        
        Vec3fa min_org_rdir; 
        Vec3fa max_org_rdir; 

        float min_dist;
        float max_dist;
      };


    private:
      static void intersect1(const BVH* bvh, NodeRef root, const size_t k, Precalculations& pre, 
                             RayK<K>& ray, const Vec3vf<K> &ray_org, const Vec3vf<K> &ray_dir, const Vec3vf<K> &ray_rdir, const vfloat<K> &ray_tnear, const vfloat<K> &ray_tfar, const Vec3vi<K>& nearXYZ, IntersectContext* context);
      static bool occluded1(const BVH* bvh, NodeRef root, const size_t k, Precalculations& pre, 
                            RayK<K>& ray, const Vec3vf<K> &ray_org, const Vec3vf<K> &ray_dir, const Vec3vf<K> &ray_rdir, const vfloat<K> &ray_tnear, const vfloat<K> &ray_tfar, const Vec3vi<K>& nearXYZ, IntersectContext* context);

    public:
      static void intersect(vint<K>* valid, Accel::Intersectors* This, RayK<K>& ray, IntersectContext* context);
      static void occluded (vint<K>* valid, Accel::Intersectors* This, RayK<K>& ray, IntersectContext* context);

      static void intersect_coherent(vint<K>* valid, Accel::Intersectors* This, RayK<K>& ray, IntersectContext* context);
      static void occluded_coherent (vint<K>* valid, Accel::Intersectors* This, RayK<K>& ray, IntersectContext* context);

    };

    /*! BVH packet intersector. */
    template<int N, int K, int types, bool robust, typename PrimitiveIntersectorK>
      class BVHNIntersectorKChunk : public BVHNIntersectorKHybrid<N,K,types,robust,PrimitiveIntersectorK,false> {};
  }
}
