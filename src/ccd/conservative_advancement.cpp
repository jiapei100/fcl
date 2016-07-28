/*
 * Software License Agreement (BSD License)
 *
 *  Copyright (c) 2011-2014, Willow Garage, Inc.
 *  Copyright (c) 2014-2016, Open Source Robotics Foundation
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   * Neither the name of Open Source Robotics Foundation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

/** \author Jia Pan */

#include "fcl/ccd/conservative_advancement.h"
#include "fcl/ccd/motion.h"
#include "fcl/collision_node.h"
#include "fcl/traversal/traversal_node_bvhs.h"
#include "fcl/traversal/traversal_node_setup.h"
#include "fcl/traversal/traversal_recurse.h"
#include "fcl/collision.h"


namespace fcl
{




template<typename BV>
bool conservativeAdvancement(const BVHModel<BV>& o1,
                             const MotionBase* motion1,
                             const BVHModel<BV>& o2,
                             const MotionBase* motion2,
                             const CollisionRequest& request,
                             CollisionResult& result,
                             FCL_REAL& toc)
{
  Transform3d tf1, tf2;
  motion1->getCurrentTransform(tf1);
  motion2->getCurrentTransform(tf2);

  // whether the first start configuration is in collision
  if(collide(&o1, tf1, &o2, tf2, request, result))
  {
    toc = 0;
    return true;
  }


  BVHModel<BV>* o1_tmp = new BVHModel<BV>(o1);
  BVHModel<BV>* o2_tmp = new BVHModel<BV>(o2);
  
  
  MeshConservativeAdvancementTraversalNode<BV> node;

  node.motion1 = motion1;
  node.motion2 = motion2;

  do
  {
    // repeatedly update mesh to global coordinate, so time consuming
    initialize(node, *o1_tmp, tf1, *o2_tmp, tf2);
    
    node.delta_t = 1;
    node.min_distance = std::numeric_limits<FCL_REAL>::max();

    distanceRecurse(&node, 0, 0, NULL);

    if(node.delta_t <= node.t_err)
    {
      // std::cout << node.delta_t << " " << node.t_err << std::endl;
      break;
    }

    node.toc += node.delta_t;
    if(node.toc > 1)
    {
      node.toc = 1;
      break;
    }

    node.motion1->integrate(node.toc);
    node.motion2->integrate(node.toc);

    motion1->getCurrentTransform(tf1);
    motion2->getCurrentTransform(tf2);
  }
  while(1);

  delete o1_tmp;
  delete o2_tmp;

  toc = node.toc;

  if(node.toc < 1)
    return true;

  return false;
}

namespace details
{

template<typename BV, typename ConservativeAdvancementOrientedNode>
bool conservativeAdvancementMeshOriented(const BVHModel<BV>& o1,
                                         const MotionBase* motion1,
                                         const BVHModel<BV>& o2,
                                         const MotionBase* motion2,
                                         const CollisionRequest& request,
                                         CollisionResult& result,
                                         FCL_REAL& toc)
{
  Transform3d tf1, tf2;
  motion1->getCurrentTransform(tf1);
  motion2->getCurrentTransform(tf2);

  // whether the first start configuration is in collision
  if(collide(&o1, tf1, &o2, tf2, request, result))
  {
    toc = 0;
    return true;
  }
  
  
  ConservativeAdvancementOrientedNode node;

  initialize(node, o1, tf1, o2, tf2);

  node.motion1 = motion1;
  node.motion2 = motion2;

  do
  {
    node.motion1->getCurrentTransform(tf1);
    node.motion2->getCurrentTransform(tf2);

    // compute the transformation from 1 to 2
    Transform3d tf = tf1.inverse() * tf2;
    node.R = tf.linear();
    node.T = tf.translation();
    
    node.delta_t = 1;
    node.min_distance = std::numeric_limits<FCL_REAL>::max();

    distanceRecurse(&node, 0, 0, NULL);

    if(node.delta_t <= node.t_err)
    {
      // std::cout << node.delta_t << " " << node.t_err << std::endl;
      break;
    }

    node.toc += node.delta_t;
    if(node.toc > 1)
    {
      node.toc = 1;
      break;
    }

    node.motion1->integrate(node.toc);
    node.motion2->integrate(node.toc);
  }
  while(1);

  toc = node.toc;

  if(node.toc < 1)
    return true;

  return false;
}


}

template<>
bool conservativeAdvancement(const BVHModel<RSS>& o1,
                             const MotionBase* motion1,
                             const BVHModel<RSS>& o2,
                             const MotionBase* motion2,
                             const CollisionRequest& request,
                             CollisionResult& result,
                             FCL_REAL& toc);


template<>
bool conservativeAdvancement(const BVHModel<OBBRSS>& o1,
                             const MotionBase* motion1,
                             const BVHModel<OBBRSS>& o2,
                             const MotionBase* motion2,
                             const CollisionRequest& request,
                             CollisionResult& result,
                             FCL_REAL& toc);

template<typename S1, typename S2, typename NarrowPhaseSolver>
bool conservativeAdvancement(const S1& o1,
                             const MotionBase* motion1,
                             const S2& o2,
                             const MotionBase* motion2,
                             const NarrowPhaseSolver* solver,
                             const CollisionRequest& request,
                             CollisionResult& result,
                             FCL_REAL& toc)
{
  Transform3d tf1, tf2;
  motion1->getCurrentTransform(tf1);
  motion2->getCurrentTransform(tf2);

  // whether the first start configuration is in collision
  if(collide(&o1, tf1, &o2, tf2, request, result))
  {
    toc = 0;
    return true;
  }

  ShapeConservativeAdvancementTraversalNode<S1, S2, NarrowPhaseSolver> node;

  initialize(node, o1, tf1, o2, tf2, solver);

  node.motion1 = motion1;
  node.motion2 = motion2;

  do
  {
    motion1->getCurrentTransform(tf1);
    motion2->getCurrentTransform(tf2);
    node.tf1 = tf1;
    node.tf2 = tf2;
    
    node.delta_t = 1;
    node.min_distance = std::numeric_limits<FCL_REAL>::max();

    distanceRecurse(&node, 0, 0, NULL);

    if(node.delta_t <= node.t_err)
    {
      // std::cout << node.delta_t << " " << node.t_err << std::endl;
      break;
    }

    node.toc += node.delta_t;
    if(node.toc > 1)
    {
      node.toc = 1;
      break;
    }

    node.motion1->integrate(node.toc);
    node.motion2->integrate(node.toc);
  }
  while(1);

  toc = node.toc;

  if(node.toc < 1)
    return true;

  return false;
}

template<typename BV, typename S, typename NarrowPhaseSolver>
bool conservativeAdvancement(const BVHModel<BV>& o1,
                             const MotionBase* motion1,
                             const S& o2,
                             const MotionBase* motion2,
                             const NarrowPhaseSolver* nsolver,
                             const CollisionRequest& request,
                             CollisionResult& result,
                             FCL_REAL& toc)
{
  Transform3d tf1, tf2;
  motion1->getCurrentTransform(tf1);
  motion2->getCurrentTransform(tf2);

  if(collide(&o1, tf1, &o2, tf2, request, result))
  {
    toc = 0;
    return true;
  }

  BVHModel<BV>* o1_tmp = new BVHModel<BV>(o1);

  MeshShapeConservativeAdvancementTraversalNode<BV, S, NarrowPhaseSolver> node;

  node.motion1 = motion1;
  node.motion2 = motion2;

  do
  {
    // initialize update mesh to global coordinate, so time consuming
    initialize(node, *o1_tmp, tf1, o2, tf2, nsolver);

    node.delta_t = 1;
    node.min_distance = std::numeric_limits<FCL_REAL>::max();

    distanceRecurse(&node, 0, 0, NULL);

    if(node.delta_t <= node.t_err)
    {
      break;
    }

    node.toc += node.delta_t;
    if(node.toc > 1)
    {
      node.toc = 1;
      break;
    }

    node.motion1->integrate(node.toc);
    node.motion2->integrate(node.toc);

    motion1->getCurrentTransform(tf1);
    motion2->getCurrentTransform(tf2);
  }
  while(1);

  delete o1_tmp;

  toc = node.toc;

  if(node.toc < 1)
    return true;

  return false;
}

namespace details
{

template<typename BV, typename S, typename NarrowPhaseSolver, typename ConservativeAdvancementOrientedNode>
bool conservativeAdvancementMeshShapeOriented(const BVHModel<BV>& o1,
                                              const MotionBase* motion1,
                                              const S& o2,
                                              const MotionBase* motion2,
                                              const NarrowPhaseSolver* nsolver,
                                              const CollisionRequest& request,
                                              CollisionResult& result,
                                              FCL_REAL& toc)
{
  Transform3d tf1, tf2;
  motion1->getCurrentTransform(tf1);
  motion2->getCurrentTransform(tf2);

  if(collide(&o1, tf1, &o2, tf2, request, result))
  {
    toc = 0;
    return true;
  }

  ConservativeAdvancementOrientedNode node;

  initialize(node, o1, tf1, o2, tf2, nsolver);

  node.motion1 = motion1;
  node.motion2 = motion2;

  do
  {
    node.motion1->getCurrentTransform(tf1);
    node.motion2->getCurrentTransform(tf2);

    node.tf1 = tf1;
    node.tf2 = tf2;
    
    node.delta_t = 1;
    node.min_distance = std::numeric_limits<FCL_REAL>::max();

    distanceRecurse(&node, 0, 0, NULL);

    if(node.delta_t <= node.t_err)
    {
      break;
    }

    node.toc += node.delta_t;
    if(node.toc > 1)
    {
      node.toc = 1;
      break;
    }

    node.motion1->integrate(node.toc);
    node.motion2->integrate(node.toc);
  }
  while(1);

  toc = node.toc;

  if(node.toc < 1)
    return true;

  return false;
}

}


template<typename S, typename NarrowPhaseSolver>
bool conservativeAdvancement(const BVHModel<RSS>& o1,
                             const MotionBase* motion1,
                             const S& o2,
                             const MotionBase* motion2,
                             const NarrowPhaseSolver* nsolver,
                             const CollisionRequest& request,
                             CollisionResult& result,
                             FCL_REAL& toc)
{
  return details::conservativeAdvancementMeshShapeOriented<RSS, S, NarrowPhaseSolver, MeshShapeConservativeAdvancementTraversalNodeRSS<S, NarrowPhaseSolver> >(o1, motion1, o2, motion2, nsolver, request, result, toc);
}

template<typename S, typename NarrowPhaseSolver>
bool conservativeAdvancement(const BVHModel<OBBRSS>& o1,
                             const MotionBase* motion1,
                             const S& o2,
                             const MotionBase* motion2,
                             const NarrowPhaseSolver* nsolver,
                             const CollisionRequest& request,
                             CollisionResult& result,
                             FCL_REAL& toc)
{
  return details::conservativeAdvancementMeshShapeOriented<OBBRSS, S, NarrowPhaseSolver, MeshShapeConservativeAdvancementTraversalNodeOBBRSS<S, NarrowPhaseSolver> >(o1, motion1, o2, motion2, nsolver, request, result, toc);  
}
                            
template<typename S, typename BV, typename NarrowPhaseSolver>
bool conservativeAdvancement(const S& o1,
                             const MotionBase* motion1,
                             const BVHModel<BV>& o2,
                             const MotionBase* motion2,
                             const NarrowPhaseSolver* nsolver,
                             const CollisionRequest& request,
                             CollisionResult& result,
                             FCL_REAL& toc)
{
  Transform3d tf1, tf2;
  motion1->getCurrentTransform(tf1);
  motion2->getCurrentTransform(tf2);

  if(collide(&o1, tf1, &o2, tf2, request, result))
  {
    toc = 0;
    return true;
  }

  BVHModel<BV>* o2_tmp = new BVHModel<BV>(o2);

  ShapeMeshConservativeAdvancementTraversalNode<S, BV, NarrowPhaseSolver> node;

  node.motion1 = motion1;
  node.motion2 = motion2;

  do
  {
    // initialize update mesh to global coordinate, so time consuming
    initialize(node, o1, tf1, *o2_tmp, tf2, nsolver);

    node.delta_t = 1;
    node.min_distance = std::numeric_limits<FCL_REAL>::max();

    distanceRecurse(&node, 0, 0, NULL);

    if(node.delta_t <= node.t_err)
    {
      break;
    }

    node.toc += node.delta_t;
    if(node.toc > 1)
    {
      node.toc = 1;
      break;
    }

    node.motion1->integrate(node.toc);
    node.motion2->integrate(node.toc);

    motion1->getCurrentTransform(tf1);
    motion2->getCurrentTransform(tf2);
  }
  while(1);

  delete o2_tmp;

  toc = node.toc;

  if(node.toc < 1)
    return true;

  return false;  
}

namespace details
{

template<typename S, typename BV, typename NarrowPhaseSolver, typename ConservativeAdvancementOrientedNode>
bool conservativeAdvancementShapeMeshOriented(const S& o1,
                                              const MotionBase* motion1,
                                              const BVHModel<BV>& o2,
                                              const MotionBase* motion2,
                                              const NarrowPhaseSolver* nsolver,
                                              const CollisionRequest& request,
                                              CollisionResult& result,
                                              FCL_REAL& toc)
{
  Transform3d tf1, tf2;
  motion1->getCurrentTransform(tf1);
  motion2->getCurrentTransform(tf2);

  if(collide(&o1, tf1, &o2, tf2, request, result))
  {
    toc = 0;
    return true;
  }

  ConservativeAdvancementOrientedNode node;

  initialize(node, o1, tf1, o2, tf2, nsolver);

  node.motion1 = motion1;
  node.motion2 = motion2;

  do
  {
    node.motion1->getCurrentTransform(tf1);
    node.motion2->getCurrentTransform(tf2);

    node.tf1 = tf1;
    node.tf2 = tf2;
    
    node.delta_t = 1;
    node.min_distance = std::numeric_limits<FCL_REAL>::max();

    distanceRecurse(&node, 0, 0, NULL);

    if(node.delta_t <= node.t_err)
    {
      break;
    }

    node.toc += node.delta_t;
    if(node.toc > 1)
    {
      node.toc = 1;
      break;
    }

    node.motion1->integrate(node.toc);
    node.motion2->integrate(node.toc);
  }
  while(1);

  toc = node.toc;

  if(node.toc < 1)
    return true;

  return false;
}

}

template<typename S, typename NarrowPhaseSolver>
bool conservativeAdvancement(const S& o1,
                             const MotionBase* motion1,
                             const BVHModel<RSS>& o2,
                             const MotionBase* motion2,
                             const NarrowPhaseSolver* nsolver,
                             const CollisionRequest& request,
                             CollisionResult& result,
                             FCL_REAL& toc)
{
  return details::conservativeAdvancementShapeMeshOriented<S, RSS, NarrowPhaseSolver, ShapeMeshConservativeAdvancementTraversalNodeRSS<S, NarrowPhaseSolver> >(o1, motion1, o2, motion2, nsolver, request, result, toc);
}


template<typename S, typename NarrowPhaseSolver>
bool conservativeAdvancement(const S& o1,
                             const MotionBase* motion1,
                             const BVHModel<OBBRSS>& o2,
                             const MotionBase* motion2,
                             const NarrowPhaseSolver* nsolver,
                             const CollisionRequest& request,
                             CollisionResult& result,
                             FCL_REAL& toc)
{
  return details::conservativeAdvancementShapeMeshOriented<S, OBBRSS, NarrowPhaseSolver, ShapeMeshConservativeAdvancementTraversalNodeOBBRSS<S, NarrowPhaseSolver> >(o1, motion1, o2, motion2, nsolver, request, result, toc);
}



template<>
bool conservativeAdvancement(const BVHModel<RSS>& o1,
                             const MotionBase* motion1,
                             const BVHModel<RSS>& o2,
                             const MotionBase* motion2,
                             const CollisionRequest& request,
                             CollisionResult& result,
                             FCL_REAL& toc)
{
  return details::conservativeAdvancementMeshOriented<RSS, MeshConservativeAdvancementTraversalNodeRSS>(o1, motion1, o2, motion2, request, result, toc);
}

template<>
bool conservativeAdvancement(const BVHModel<OBBRSS>& o1,
                             const MotionBase* motion1,
                             const BVHModel<OBBRSS>& o2,
                             const MotionBase* motion2,
                             const CollisionRequest& request,
                             CollisionResult& result,
                             FCL_REAL& toc)
{
  return details::conservativeAdvancementMeshOriented<OBBRSS, MeshConservativeAdvancementTraversalNodeOBBRSS>(o1, motion1, o2, motion2, request, result, toc);
}


template<typename BV, typename NarrowPhaseSolver>
FCL_REAL BVHConservativeAdvancement(const CollisionGeometryd* o1, const MotionBase* motion1, const CollisionGeometryd* o2, const MotionBase* motion2, const NarrowPhaseSolver* nsolver, const ContinuousCollisionRequest& request, ContinuousCollisionResult& result)
{
  const BVHModel<BV>* obj1 = static_cast<const BVHModel<BV>*>(o1);
  const BVHModel<BV>* obj2 = static_cast<const BVHModel<BV>*>(o2);

  CollisionRequest c_request;
  CollisionResult c_result;
  FCL_REAL toc;
  bool is_collide = conservativeAdvancement(*obj1, motion1, *obj2, motion2, c_request, c_result, toc);

  result.is_collide = is_collide;
  result.time_of_contact = toc;
  
  return toc;
}

template<typename S1, typename S2, typename NarrowPhaseSolver>
FCL_REAL ShapeConservativeAdvancement(const CollisionGeometryd* o1, const MotionBase* motion1, const CollisionGeometryd* o2, const MotionBase* motion2, const NarrowPhaseSolver* nsolver, const ContinuousCollisionRequest& request, ContinuousCollisionResult& result)
{
  const S1* obj1 = static_cast<const S1*>(o1);
  const S2* obj2 = static_cast<const S2*>(o2);

  CollisionRequest c_request;
  CollisionResult c_result;
  FCL_REAL toc;
  bool is_collide = conservativeAdvancement(*obj1, motion1, *obj2, motion2, nsolver, c_request, c_result, toc);

  result.is_collide = is_collide;
  result.time_of_contact = toc;

  return toc;
}

template<typename S, typename BV, typename NarrowPhaseSolver>
FCL_REAL ShapeBVHConservativeAdvancement(const CollisionGeometryd* o1, const MotionBase* motion1, const CollisionGeometryd* o2, const MotionBase* motion2, const NarrowPhaseSolver* nsolver, const ContinuousCollisionRequest& request, ContinuousCollisionResult& result)
{
  const S* obj1 = static_cast<const S*>(o1);
  const BVHModel<BV>* obj2 = static_cast<const BVHModel<BV>*>(o2);

  CollisionRequest c_request;
  CollisionResult c_result;
  FCL_REAL toc;

  bool is_collide = conservativeAdvancement(*obj1, motion1, *obj2, motion2, nsolver, c_request, c_result, toc);

  result.is_collide = is_collide;
  result.time_of_contact = toc;

  return toc;
}

template<typename BV, typename S, typename NarrowPhaseSolver>
FCL_REAL BVHShapeConservativeAdvancement(const CollisionGeometryd* o1, const MotionBase* motion1, const CollisionGeometryd* o2, const MotionBase* motion2, const NarrowPhaseSolver* nsolver, const ContinuousCollisionRequest& request, ContinuousCollisionResult& result)
{
  const BVHModel<BV>* obj1 = static_cast<const BVHModel<BV>*>(o1);
  const S* obj2 = static_cast<const S*>(o2);

  CollisionRequest c_request;
  CollisionResult c_result;
  FCL_REAL toc;

  bool is_collide = conservativeAdvancement(*obj1, motion1, *obj2, motion2, nsolver, c_request, c_result, toc);

  result.is_collide = is_collide;
  result.time_of_contact = toc;

  return toc;
}

template<typename NarrowPhaseSolver>
ConservativeAdvancementFunctionMatrix<NarrowPhaseSolver>::ConservativeAdvancementFunctionMatrix()
{
  for(int i = 0; i < NODE_COUNT; ++i)
  {
    for(int j = 0; j < NODE_COUNT; ++j)
      conservative_advancement_matrix[i][j] = NULL;
  }


  conservative_advancement_matrix[GEOM_BOX][GEOM_BOX] = &ShapeConservativeAdvancement<Boxd, Boxd, NarrowPhaseSolver>;
  conservative_advancement_matrix[GEOM_BOX][GEOM_SPHERE] = &ShapeConservativeAdvancement<Boxd, Sphere, NarrowPhaseSolver>;
  conservative_advancement_matrix[GEOM_BOX][GEOM_CAPSULE] = &ShapeConservativeAdvancement<Boxd, Capsuled, NarrowPhaseSolver>;
  conservative_advancement_matrix[GEOM_BOX][GEOM_CONE] = &ShapeConservativeAdvancement<Boxd, Coned, NarrowPhaseSolver>;
  conservative_advancement_matrix[GEOM_BOX][GEOM_CYLINDER] = &ShapeConservativeAdvancement<Boxd, Cylinder, NarrowPhaseSolver>;
  conservative_advancement_matrix[GEOM_BOX][GEOM_CONVEX] = &ShapeConservativeAdvancement<Boxd, Convex, NarrowPhaseSolver>;
  conservative_advancement_matrix[GEOM_BOX][GEOM_PLANE] = &ShapeConservativeAdvancement<Boxd, Plane, NarrowPhaseSolver>;
  conservative_advancement_matrix[GEOM_BOX][GEOM_HALFSPACE] = &ShapeConservativeAdvancement<Boxd, Halfspace, NarrowPhaseSolver>;
    
  conservative_advancement_matrix[GEOM_SPHERE][GEOM_BOX] = &ShapeConservativeAdvancement<Sphere, Boxd, NarrowPhaseSolver>;
  conservative_advancement_matrix[GEOM_SPHERE][GEOM_SPHERE] = &ShapeConservativeAdvancement<Sphere, Sphere, NarrowPhaseSolver>;
  conservative_advancement_matrix[GEOM_SPHERE][GEOM_CAPSULE] = &ShapeConservativeAdvancement<Sphere, Capsuled, NarrowPhaseSolver>;
  conservative_advancement_matrix[GEOM_SPHERE][GEOM_CONE] = &ShapeConservativeAdvancement<Sphere, Coned, NarrowPhaseSolver>;
  conservative_advancement_matrix[GEOM_SPHERE][GEOM_CYLINDER] = &ShapeConservativeAdvancement<Sphere, Cylinder, NarrowPhaseSolver>;
  conservative_advancement_matrix[GEOM_SPHERE][GEOM_CONVEX] = &ShapeConservativeAdvancement<Sphere, Convex, NarrowPhaseSolver>;
  conservative_advancement_matrix[GEOM_SPHERE][GEOM_PLANE] = &ShapeConservativeAdvancement<Sphere, Plane, NarrowPhaseSolver>;
  conservative_advancement_matrix[GEOM_SPHERE][GEOM_HALFSPACE] = &ShapeConservativeAdvancement<Sphere, Halfspace, NarrowPhaseSolver>;

  conservative_advancement_matrix[GEOM_CAPSULE][GEOM_BOX] = &ShapeConservativeAdvancement<Capsuled, Boxd, NarrowPhaseSolver>;
  conservative_advancement_matrix[GEOM_CAPSULE][GEOM_SPHERE] = &ShapeConservativeAdvancement<Capsuled, Sphere, NarrowPhaseSolver>;
  conservative_advancement_matrix[GEOM_CAPSULE][GEOM_CAPSULE] = &ShapeConservativeAdvancement<Capsuled, Capsuled, NarrowPhaseSolver>;
  conservative_advancement_matrix[GEOM_CAPSULE][GEOM_CONE] = &ShapeConservativeAdvancement<Capsuled, Coned, NarrowPhaseSolver>;
  conservative_advancement_matrix[GEOM_CAPSULE][GEOM_CYLINDER] = &ShapeConservativeAdvancement<Capsuled, Cylinder, NarrowPhaseSolver>;
  conservative_advancement_matrix[GEOM_CAPSULE][GEOM_CONVEX] = &ShapeConservativeAdvancement<Capsuled, Convex, NarrowPhaseSolver>;
  conservative_advancement_matrix[GEOM_CAPSULE][GEOM_PLANE] = &ShapeConservativeAdvancement<Capsuled, Plane, NarrowPhaseSolver>;
  conservative_advancement_matrix[GEOM_CAPSULE][GEOM_HALFSPACE] = &ShapeConservativeAdvancement<Capsuled, Halfspace, NarrowPhaseSolver>;

  conservative_advancement_matrix[GEOM_CONE][GEOM_BOX] = &ShapeConservativeAdvancement<Coned, Boxd, NarrowPhaseSolver>;
  conservative_advancement_matrix[GEOM_CONE][GEOM_SPHERE] = &ShapeConservativeAdvancement<Coned, Sphere, NarrowPhaseSolver>;
  conservative_advancement_matrix[GEOM_CONE][GEOM_CAPSULE] = &ShapeConservativeAdvancement<Coned, Capsuled, NarrowPhaseSolver>;
  conservative_advancement_matrix[GEOM_CONE][GEOM_CONE] = &ShapeConservativeAdvancement<Coned, Coned, NarrowPhaseSolver>;
  conservative_advancement_matrix[GEOM_CONE][GEOM_CYLINDER] = &ShapeConservativeAdvancement<Coned, Cylinder, NarrowPhaseSolver>;
  conservative_advancement_matrix[GEOM_CONE][GEOM_CONVEX] = &ShapeConservativeAdvancement<Coned, Convex, NarrowPhaseSolver>;
  conservative_advancement_matrix[GEOM_CONE][GEOM_PLANE] = &ShapeConservativeAdvancement<Coned, Plane, NarrowPhaseSolver>;
  conservative_advancement_matrix[GEOM_CONE][GEOM_HALFSPACE] = &ShapeConservativeAdvancement<Coned, Halfspace, NarrowPhaseSolver>;

  conservative_advancement_matrix[GEOM_CYLINDER][GEOM_BOX] = &ShapeConservativeAdvancement<Cylinder, Boxd, NarrowPhaseSolver>;
  conservative_advancement_matrix[GEOM_CYLINDER][GEOM_SPHERE] = &ShapeConservativeAdvancement<Cylinder, Sphere, NarrowPhaseSolver>;
  conservative_advancement_matrix[GEOM_CYLINDER][GEOM_CAPSULE] = &ShapeConservativeAdvancement<Cylinder, Capsuled, NarrowPhaseSolver>;
  conservative_advancement_matrix[GEOM_CYLINDER][GEOM_CONE] = &ShapeConservativeAdvancement<Cylinder, Coned, NarrowPhaseSolver>;
  conservative_advancement_matrix[GEOM_CYLINDER][GEOM_CYLINDER] = &ShapeConservativeAdvancement<Cylinder, Cylinder, NarrowPhaseSolver>;
  conservative_advancement_matrix[GEOM_CYLINDER][GEOM_CONVEX] = &ShapeConservativeAdvancement<Cylinder, Convex, NarrowPhaseSolver>;
  conservative_advancement_matrix[GEOM_CYLINDER][GEOM_PLANE] = &ShapeConservativeAdvancement<Cylinder, Plane, NarrowPhaseSolver>;
  conservative_advancement_matrix[GEOM_CYLINDER][GEOM_HALFSPACE] = &ShapeConservativeAdvancement<Cylinder, Halfspace, NarrowPhaseSolver>;

  conservative_advancement_matrix[GEOM_CONVEX][GEOM_BOX] = &ShapeConservativeAdvancement<Convex, Boxd, NarrowPhaseSolver>;
  conservative_advancement_matrix[GEOM_CONVEX][GEOM_SPHERE] = &ShapeConservativeAdvancement<Convex, Sphere, NarrowPhaseSolver>;
  conservative_advancement_matrix[GEOM_CONVEX][GEOM_CAPSULE] = &ShapeConservativeAdvancement<Convex, Capsuled, NarrowPhaseSolver>;
  conservative_advancement_matrix[GEOM_CONVEX][GEOM_CONE] = &ShapeConservativeAdvancement<Convex, Coned, NarrowPhaseSolver>;
  conservative_advancement_matrix[GEOM_CONVEX][GEOM_CYLINDER] = &ShapeConservativeAdvancement<Convex, Cylinder, NarrowPhaseSolver>;
  conservative_advancement_matrix[GEOM_CONVEX][GEOM_CONVEX] = &ShapeConservativeAdvancement<Convex, Convex, NarrowPhaseSolver>;
  conservative_advancement_matrix[GEOM_CONVEX][GEOM_PLANE] = &ShapeConservativeAdvancement<Convex, Plane, NarrowPhaseSolver>;
  conservative_advancement_matrix[GEOM_CONVEX][GEOM_HALFSPACE] = &ShapeConservativeAdvancement<Convex, Halfspace, NarrowPhaseSolver>;

  conservative_advancement_matrix[GEOM_PLANE][GEOM_BOX] = &ShapeConservativeAdvancement<Plane, Boxd, NarrowPhaseSolver>;
  conservative_advancement_matrix[GEOM_PLANE][GEOM_SPHERE] = &ShapeConservativeAdvancement<Plane, Sphere, NarrowPhaseSolver>;
  conservative_advancement_matrix[GEOM_PLANE][GEOM_CAPSULE] = &ShapeConservativeAdvancement<Plane, Capsuled, NarrowPhaseSolver>;
  conservative_advancement_matrix[GEOM_PLANE][GEOM_CONE] = &ShapeConservativeAdvancement<Plane, Coned, NarrowPhaseSolver>;
  conservative_advancement_matrix[GEOM_PLANE][GEOM_CYLINDER] = &ShapeConservativeAdvancement<Plane, Cylinder, NarrowPhaseSolver>;
  conservative_advancement_matrix[GEOM_PLANE][GEOM_CONVEX] = &ShapeConservativeAdvancement<Plane, Convex, NarrowPhaseSolver>;
  conservative_advancement_matrix[GEOM_PLANE][GEOM_PLANE] = &ShapeConservativeAdvancement<Plane, Plane, NarrowPhaseSolver>;
  conservative_advancement_matrix[GEOM_PLANE][GEOM_HALFSPACE] = &ShapeConservativeAdvancement<Plane, Halfspace, NarrowPhaseSolver>;

  conservative_advancement_matrix[GEOM_HALFSPACE][GEOM_BOX] = &ShapeConservativeAdvancement<Halfspace, Boxd, NarrowPhaseSolver>;
  conservative_advancement_matrix[GEOM_HALFSPACE][GEOM_SPHERE] = &ShapeConservativeAdvancement<Halfspace, Sphere, NarrowPhaseSolver>;
  conservative_advancement_matrix[GEOM_HALFSPACE][GEOM_CAPSULE] = &ShapeConservativeAdvancement<Halfspace, Capsuled, NarrowPhaseSolver>;
  conservative_advancement_matrix[GEOM_HALFSPACE][GEOM_CONE] = &ShapeConservativeAdvancement<Halfspace, Coned, NarrowPhaseSolver>;
  conservative_advancement_matrix[GEOM_HALFSPACE][GEOM_CYLINDER] = &ShapeConservativeAdvancement<Halfspace, Cylinder, NarrowPhaseSolver>;
  conservative_advancement_matrix[GEOM_HALFSPACE][GEOM_CONVEX] = &ShapeConservativeAdvancement<Halfspace, Convex, NarrowPhaseSolver>;
  conservative_advancement_matrix[GEOM_HALFSPACE][GEOM_PLANE] = &ShapeConservativeAdvancement<Halfspace, Plane, NarrowPhaseSolver>;
  conservative_advancement_matrix[GEOM_HALFSPACE][GEOM_HALFSPACE] = &ShapeConservativeAdvancement<Halfspace, Halfspace, NarrowPhaseSolver>;

  conservative_advancement_matrix[BV_AABB][GEOM_BOX] = &BVHShapeConservativeAdvancement<AABB, Boxd, NarrowPhaseSolver>;
  conservative_advancement_matrix[BV_AABB][GEOM_SPHERE] = &BVHShapeConservativeAdvancement<AABB, Sphere, NarrowPhaseSolver>;
  conservative_advancement_matrix[BV_AABB][GEOM_CAPSULE] = &BVHShapeConservativeAdvancement<AABB, Capsuled, NarrowPhaseSolver>;
  conservative_advancement_matrix[BV_AABB][GEOM_CONE] = &BVHShapeConservativeAdvancement<AABB, Coned, NarrowPhaseSolver>;
  conservative_advancement_matrix[BV_AABB][GEOM_CYLINDER] = &BVHShapeConservativeAdvancement<AABB, Cylinder, NarrowPhaseSolver>;
  conservative_advancement_matrix[BV_AABB][GEOM_CONVEX] = &BVHShapeConservativeAdvancement<AABB, Convex, NarrowPhaseSolver>;
  conservative_advancement_matrix[BV_AABB][GEOM_PLANE] = &BVHShapeConservativeAdvancement<AABB, Plane, NarrowPhaseSolver>;
  conservative_advancement_matrix[BV_AABB][GEOM_HALFSPACE] = &BVHShapeConservativeAdvancement<AABB, Halfspace, NarrowPhaseSolver>;
  
  conservative_advancement_matrix[BV_OBB][GEOM_BOX] = &BVHShapeConservativeAdvancement<OBB, Boxd, NarrowPhaseSolver>;
  conservative_advancement_matrix[BV_OBB][GEOM_SPHERE] = &BVHShapeConservativeAdvancement<OBB, Sphere, NarrowPhaseSolver>;
  conservative_advancement_matrix[BV_OBB][GEOM_CAPSULE] = &BVHShapeConservativeAdvancement<OBB, Capsuled, NarrowPhaseSolver>;
  conservative_advancement_matrix[BV_OBB][GEOM_CONE] = &BVHShapeConservativeAdvancement<OBB, Coned, NarrowPhaseSolver>;
  conservative_advancement_matrix[BV_OBB][GEOM_CYLINDER] = &BVHShapeConservativeAdvancement<OBB, Cylinder, NarrowPhaseSolver>;
  conservative_advancement_matrix[BV_OBB][GEOM_CONVEX] = &BVHShapeConservativeAdvancement<OBB, Convex, NarrowPhaseSolver>;
  conservative_advancement_matrix[BV_OBB][GEOM_PLANE] = &BVHShapeConservativeAdvancement<OBB, Plane, NarrowPhaseSolver>;
  conservative_advancement_matrix[BV_OBB][GEOM_HALFSPACE] = &BVHShapeConservativeAdvancement<OBB, Halfspace, NarrowPhaseSolver>;

  conservative_advancement_matrix[BV_OBBRSS][GEOM_BOX] = &BVHShapeConservativeAdvancement<OBBRSS, Boxd, NarrowPhaseSolver>;
  conservative_advancement_matrix[BV_OBBRSS][GEOM_SPHERE] = &BVHShapeConservativeAdvancement<OBBRSS, Sphere, NarrowPhaseSolver>;
  conservative_advancement_matrix[BV_OBBRSS][GEOM_CAPSULE] = &BVHShapeConservativeAdvancement<OBBRSS, Capsuled, NarrowPhaseSolver>;
  conservative_advancement_matrix[BV_OBBRSS][GEOM_CONE] = &BVHShapeConservativeAdvancement<OBBRSS, Coned, NarrowPhaseSolver>;
  conservative_advancement_matrix[BV_OBBRSS][GEOM_CYLINDER] = &BVHShapeConservativeAdvancement<OBBRSS, Cylinder, NarrowPhaseSolver>;
  conservative_advancement_matrix[BV_OBBRSS][GEOM_CONVEX] = &BVHShapeConservativeAdvancement<OBBRSS, Convex, NarrowPhaseSolver>;
  conservative_advancement_matrix[BV_OBBRSS][GEOM_PLANE] = &BVHShapeConservativeAdvancement<OBBRSS, Plane, NarrowPhaseSolver>;
  conservative_advancement_matrix[BV_OBBRSS][GEOM_HALFSPACE] = &BVHShapeConservativeAdvancement<OBBRSS, Halfspace, NarrowPhaseSolver>;

  conservative_advancement_matrix[BV_RSS][GEOM_BOX] = &BVHShapeConservativeAdvancement<RSS, Boxd, NarrowPhaseSolver>;
  conservative_advancement_matrix[BV_RSS][GEOM_SPHERE] = &BVHShapeConservativeAdvancement<RSS, Sphere, NarrowPhaseSolver>;
  conservative_advancement_matrix[BV_RSS][GEOM_CAPSULE] = &BVHShapeConservativeAdvancement<RSS, Capsuled, NarrowPhaseSolver>;
  conservative_advancement_matrix[BV_RSS][GEOM_CONE] = &BVHShapeConservativeAdvancement<RSS, Coned, NarrowPhaseSolver>;
  conservative_advancement_matrix[BV_RSS][GEOM_CYLINDER] = &BVHShapeConservativeAdvancement<RSS, Cylinder, NarrowPhaseSolver>;
  conservative_advancement_matrix[BV_RSS][GEOM_CONVEX] = &BVHShapeConservativeAdvancement<RSS, Convex, NarrowPhaseSolver>;
  conservative_advancement_matrix[BV_RSS][GEOM_PLANE] = &BVHShapeConservativeAdvancement<RSS, Plane, NarrowPhaseSolver>;
  conservative_advancement_matrix[BV_RSS][GEOM_HALFSPACE] = &BVHShapeConservativeAdvancement<RSS, Halfspace, NarrowPhaseSolver>;

  conservative_advancement_matrix[BV_KDOP16][GEOM_BOX] = &BVHShapeConservativeAdvancement<KDOP<16>, Boxd, NarrowPhaseSolver>;
  conservative_advancement_matrix[BV_KDOP16][GEOM_SPHERE] = &BVHShapeConservativeAdvancement<KDOP<16>, Sphere, NarrowPhaseSolver>;
  conservative_advancement_matrix[BV_KDOP16][GEOM_CAPSULE] = &BVHShapeConservativeAdvancement<KDOP<16>, Capsuled, NarrowPhaseSolver>;
  conservative_advancement_matrix[BV_KDOP16][GEOM_CONE] = &BVHShapeConservativeAdvancement<KDOP<16>, Coned, NarrowPhaseSolver>;
  conservative_advancement_matrix[BV_KDOP16][GEOM_CYLINDER] = &BVHShapeConservativeAdvancement<KDOP<16>, Cylinder, NarrowPhaseSolver>;
  conservative_advancement_matrix[BV_KDOP16][GEOM_CONVEX] = &BVHShapeConservativeAdvancement<KDOP<16>, Convex, NarrowPhaseSolver>;
  conservative_advancement_matrix[BV_KDOP16][GEOM_PLANE] = &BVHShapeConservativeAdvancement<KDOP<16>, Plane, NarrowPhaseSolver>;
  conservative_advancement_matrix[BV_KDOP16][GEOM_HALFSPACE] = &BVHShapeConservativeAdvancement<KDOP<16>, Halfspace, NarrowPhaseSolver>;

  conservative_advancement_matrix[BV_KDOP18][GEOM_BOX] = &BVHShapeConservativeAdvancement<KDOP<18>, Boxd, NarrowPhaseSolver>;
  conservative_advancement_matrix[BV_KDOP18][GEOM_SPHERE] = &BVHShapeConservativeAdvancement<KDOP<18>, Sphere, NarrowPhaseSolver>;
  conservative_advancement_matrix[BV_KDOP18][GEOM_CAPSULE] = &BVHShapeConservativeAdvancement<KDOP<18>, Capsuled, NarrowPhaseSolver>;
  conservative_advancement_matrix[BV_KDOP18][GEOM_CONE] = &BVHShapeConservativeAdvancement<KDOP<18>, Coned, NarrowPhaseSolver>;
  conservative_advancement_matrix[BV_KDOP18][GEOM_CYLINDER] = &BVHShapeConservativeAdvancement<KDOP<18>, Cylinder, NarrowPhaseSolver>;
  conservative_advancement_matrix[BV_KDOP18][GEOM_CONVEX] = &BVHShapeConservativeAdvancement<KDOP<18>, Convex, NarrowPhaseSolver>;
  conservative_advancement_matrix[BV_KDOP18][GEOM_PLANE] = &BVHShapeConservativeAdvancement<KDOP<18>, Plane, NarrowPhaseSolver>;
  conservative_advancement_matrix[BV_KDOP18][GEOM_HALFSPACE] = &BVHShapeConservativeAdvancement<KDOP<18>, Halfspace, NarrowPhaseSolver>;

  conservative_advancement_matrix[BV_KDOP24][GEOM_BOX] = &BVHShapeConservativeAdvancement<KDOP<24>, Boxd, NarrowPhaseSolver>;
  conservative_advancement_matrix[BV_KDOP24][GEOM_SPHERE] = &BVHShapeConservativeAdvancement<KDOP<24>, Sphere, NarrowPhaseSolver>;
  conservative_advancement_matrix[BV_KDOP24][GEOM_CAPSULE] = &BVHShapeConservativeAdvancement<KDOP<24>, Capsuled, NarrowPhaseSolver>;
  conservative_advancement_matrix[BV_KDOP24][GEOM_CONE] = &BVHShapeConservativeAdvancement<KDOP<24>, Coned, NarrowPhaseSolver>;
  conservative_advancement_matrix[BV_KDOP24][GEOM_CYLINDER] = &BVHShapeConservativeAdvancement<KDOP<24>, Cylinder, NarrowPhaseSolver>;
  conservative_advancement_matrix[BV_KDOP24][GEOM_CONVEX] = &BVHShapeConservativeAdvancement<KDOP<24>, Convex, NarrowPhaseSolver>;
  conservative_advancement_matrix[BV_KDOP24][GEOM_PLANE] = &BVHShapeConservativeAdvancement<KDOP<24>, Plane, NarrowPhaseSolver>;
  conservative_advancement_matrix[BV_KDOP24][GEOM_HALFSPACE] = &BVHShapeConservativeAdvancement<KDOP<24>, Halfspace, NarrowPhaseSolver>;

  conservative_advancement_matrix[BV_kIOS][GEOM_BOX] = &BVHShapeConservativeAdvancement<kIOS, Boxd, NarrowPhaseSolver>;
  conservative_advancement_matrix[BV_kIOS][GEOM_SPHERE] = &BVHShapeConservativeAdvancement<kIOS, Sphere, NarrowPhaseSolver>;
  conservative_advancement_matrix[BV_kIOS][GEOM_CAPSULE] = &BVHShapeConservativeAdvancement<kIOS, Capsuled, NarrowPhaseSolver>;
  conservative_advancement_matrix[BV_kIOS][GEOM_CONE] = &BVHShapeConservativeAdvancement<kIOS, Coned, NarrowPhaseSolver>;
  conservative_advancement_matrix[BV_kIOS][GEOM_CYLINDER] = &BVHShapeConservativeAdvancement<kIOS, Cylinder, NarrowPhaseSolver>;
  conservative_advancement_matrix[BV_kIOS][GEOM_CONVEX] = &BVHShapeConservativeAdvancement<kIOS, Convex, NarrowPhaseSolver>;
  conservative_advancement_matrix[BV_kIOS][GEOM_PLANE] = &BVHShapeConservativeAdvancement<kIOS, Plane, NarrowPhaseSolver>;
  conservative_advancement_matrix[BV_kIOS][GEOM_HALFSPACE] = &BVHShapeConservativeAdvancement<kIOS, Halfspace, NarrowPhaseSolver>;


  conservative_advancement_matrix[GEOM_BOX][BV_AABB] = &ShapeBVHConservativeAdvancement<Boxd, AABB, NarrowPhaseSolver>;
  conservative_advancement_matrix[GEOM_SPHERE][BV_AABB] = &ShapeBVHConservativeAdvancement<Sphere, AABB, NarrowPhaseSolver>;
  conservative_advancement_matrix[GEOM_CAPSULE][BV_AABB] = &ShapeBVHConservativeAdvancement<Capsuled, AABB, NarrowPhaseSolver>;
  conservative_advancement_matrix[GEOM_CONE][BV_AABB] = &ShapeBVHConservativeAdvancement<Coned, AABB, NarrowPhaseSolver>;
  conservative_advancement_matrix[GEOM_CYLINDER][BV_AABB] = &ShapeBVHConservativeAdvancement<Cylinder, AABB, NarrowPhaseSolver>;
  conservative_advancement_matrix[GEOM_CONVEX][BV_AABB] = &ShapeBVHConservativeAdvancement<Convex, AABB, NarrowPhaseSolver>;
  conservative_advancement_matrix[GEOM_PLANE][BV_AABB] = &ShapeBVHConservativeAdvancement<Plane, AABB, NarrowPhaseSolver>;
  conservative_advancement_matrix[GEOM_HALFSPACE][BV_AABB] = &ShapeBVHConservativeAdvancement<Halfspace, AABB, NarrowPhaseSolver>;
  
  conservative_advancement_matrix[GEOM_BOX][BV_OBB] = &ShapeBVHConservativeAdvancement<Boxd, OBB, NarrowPhaseSolver>;
  conservative_advancement_matrix[GEOM_SPHERE][BV_OBB] = &ShapeBVHConservativeAdvancement<Sphere, OBB, NarrowPhaseSolver>;
  conservative_advancement_matrix[GEOM_CAPSULE][BV_OBB] = &ShapeBVHConservativeAdvancement<Capsuled, OBB, NarrowPhaseSolver>;
  conservative_advancement_matrix[GEOM_CONE][BV_OBB] = &ShapeBVHConservativeAdvancement<Coned, OBB, NarrowPhaseSolver>;
  conservative_advancement_matrix[GEOM_CYLINDER][BV_OBB] = &ShapeBVHConservativeAdvancement<Cylinder, OBB, NarrowPhaseSolver>;
  conservative_advancement_matrix[GEOM_CONVEX][BV_OBB] = &ShapeBVHConservativeAdvancement<Convex, OBB, NarrowPhaseSolver>;
  conservative_advancement_matrix[GEOM_PLANE][BV_OBB] = &ShapeBVHConservativeAdvancement<Plane, OBB, NarrowPhaseSolver>;
  conservative_advancement_matrix[GEOM_HALFSPACE][BV_OBB] = &ShapeBVHConservativeAdvancement<Halfspace, OBB, NarrowPhaseSolver>;

  conservative_advancement_matrix[GEOM_BOX][BV_RSS] = &ShapeBVHConservativeAdvancement<Boxd, RSS, NarrowPhaseSolver>;
  conservative_advancement_matrix[GEOM_SPHERE][BV_RSS] = &ShapeBVHConservativeAdvancement<Sphere, RSS, NarrowPhaseSolver>;
  conservative_advancement_matrix[GEOM_CAPSULE][BV_RSS] = &ShapeBVHConservativeAdvancement<Capsuled, RSS, NarrowPhaseSolver>;
  conservative_advancement_matrix[GEOM_CONE][BV_RSS] = &ShapeBVHConservativeAdvancement<Coned, RSS, NarrowPhaseSolver>;
  conservative_advancement_matrix[GEOM_CYLINDER][BV_RSS] = &ShapeBVHConservativeAdvancement<Cylinder, RSS, NarrowPhaseSolver>;
  conservative_advancement_matrix[GEOM_CONVEX][BV_RSS] = &ShapeBVHConservativeAdvancement<Convex, RSS, NarrowPhaseSolver>;
  conservative_advancement_matrix[GEOM_PLANE][BV_RSS] = &ShapeBVHConservativeAdvancement<Plane, RSS, NarrowPhaseSolver>;
  conservative_advancement_matrix[GEOM_HALFSPACE][BV_RSS] = &ShapeBVHConservativeAdvancement<Halfspace, RSS, NarrowPhaseSolver>;

  conservative_advancement_matrix[GEOM_BOX][BV_OBBRSS] = &ShapeBVHConservativeAdvancement<Boxd, OBBRSS, NarrowPhaseSolver>;
  conservative_advancement_matrix[GEOM_SPHERE][BV_OBBRSS] = &ShapeBVHConservativeAdvancement<Sphere, OBBRSS, NarrowPhaseSolver>;
  conservative_advancement_matrix[GEOM_CAPSULE][BV_OBBRSS] = &ShapeBVHConservativeAdvancement<Capsuled, OBBRSS, NarrowPhaseSolver>;
  conservative_advancement_matrix[GEOM_CONE][BV_OBBRSS] = &ShapeBVHConservativeAdvancement<Coned, OBBRSS, NarrowPhaseSolver>;
  conservative_advancement_matrix[GEOM_CYLINDER][BV_OBBRSS] = &ShapeBVHConservativeAdvancement<Cylinder, OBBRSS, NarrowPhaseSolver>;
  conservative_advancement_matrix[GEOM_CONVEX][BV_OBBRSS] = &ShapeBVHConservativeAdvancement<Convex, OBBRSS, NarrowPhaseSolver>;
  conservative_advancement_matrix[GEOM_PLANE][BV_OBBRSS] = &ShapeBVHConservativeAdvancement<Plane, OBBRSS, NarrowPhaseSolver>;
  conservative_advancement_matrix[GEOM_HALFSPACE][BV_OBBRSS] = &ShapeBVHConservativeAdvancement<Halfspace, OBBRSS, NarrowPhaseSolver>;

  conservative_advancement_matrix[GEOM_BOX][BV_KDOP16] = &ShapeBVHConservativeAdvancement<Boxd, KDOP<16>, NarrowPhaseSolver>;
  conservative_advancement_matrix[GEOM_SPHERE][BV_KDOP16] = &ShapeBVHConservativeAdvancement<Sphere, KDOP<16>, NarrowPhaseSolver>;
  conservative_advancement_matrix[GEOM_CAPSULE][BV_KDOP16] = &ShapeBVHConservativeAdvancement<Capsuled, KDOP<16>, NarrowPhaseSolver>;
  conservative_advancement_matrix[GEOM_CONE][BV_KDOP16] = &ShapeBVHConservativeAdvancement<Coned, KDOP<16>, NarrowPhaseSolver>;
  conservative_advancement_matrix[GEOM_CYLINDER][BV_KDOP16] = &ShapeBVHConservativeAdvancement<Cylinder, KDOP<16>, NarrowPhaseSolver>;
  conservative_advancement_matrix[GEOM_CONVEX][BV_KDOP16] = &ShapeBVHConservativeAdvancement<Convex, KDOP<16>, NarrowPhaseSolver>;
  conservative_advancement_matrix[GEOM_PLANE][BV_KDOP16] = &ShapeBVHConservativeAdvancement<Plane, KDOP<16>, NarrowPhaseSolver>;
  conservative_advancement_matrix[GEOM_HALFSPACE][BV_KDOP16] = &ShapeBVHConservativeAdvancement<Halfspace, KDOP<16>, NarrowPhaseSolver>;

  conservative_advancement_matrix[GEOM_BOX][BV_KDOP18] = &ShapeBVHConservativeAdvancement<Boxd, KDOP<18>, NarrowPhaseSolver>;
  conservative_advancement_matrix[GEOM_SPHERE][BV_KDOP18] = &ShapeBVHConservativeAdvancement<Sphere, KDOP<18>, NarrowPhaseSolver>;
  conservative_advancement_matrix[GEOM_CAPSULE][BV_KDOP18] = &ShapeBVHConservativeAdvancement<Capsuled, KDOP<18>, NarrowPhaseSolver>;
  conservative_advancement_matrix[GEOM_CONE][BV_KDOP18] = &ShapeBVHConservativeAdvancement<Coned, KDOP<18>, NarrowPhaseSolver>;
  conservative_advancement_matrix[GEOM_CYLINDER][BV_KDOP18] = &ShapeBVHConservativeAdvancement<Cylinder, KDOP<18>, NarrowPhaseSolver>;
  conservative_advancement_matrix[GEOM_CONVEX][BV_KDOP18] = &ShapeBVHConservativeAdvancement<Convex, KDOP<18>, NarrowPhaseSolver>;
  conservative_advancement_matrix[GEOM_PLANE][BV_KDOP18] = &ShapeBVHConservativeAdvancement<Plane, KDOP<18>, NarrowPhaseSolver>;
  conservative_advancement_matrix[GEOM_HALFSPACE][BV_KDOP18] = &ShapeBVHConservativeAdvancement<Halfspace, KDOP<18>, NarrowPhaseSolver>;

  conservative_advancement_matrix[GEOM_BOX][BV_KDOP24] = &ShapeBVHConservativeAdvancement<Boxd, KDOP<24>, NarrowPhaseSolver>;
  conservative_advancement_matrix[GEOM_SPHERE][BV_KDOP24] = &ShapeBVHConservativeAdvancement<Sphere, KDOP<24>, NarrowPhaseSolver>;
  conservative_advancement_matrix[GEOM_CAPSULE][BV_KDOP24] = &ShapeBVHConservativeAdvancement<Capsuled, KDOP<24>, NarrowPhaseSolver>;
  conservative_advancement_matrix[GEOM_CONE][BV_KDOP24] = &ShapeBVHConservativeAdvancement<Coned, KDOP<24>, NarrowPhaseSolver>;
  conservative_advancement_matrix[GEOM_CYLINDER][BV_KDOP24] = &ShapeBVHConservativeAdvancement<Cylinder, KDOP<24>, NarrowPhaseSolver>;
  conservative_advancement_matrix[GEOM_CONVEX][BV_KDOP24] = &ShapeBVHConservativeAdvancement<Convex, KDOP<24>, NarrowPhaseSolver>;
  conservative_advancement_matrix[GEOM_PLANE][BV_KDOP24] = &ShapeBVHConservativeAdvancement<Plane, KDOP<24>, NarrowPhaseSolver>;
  conservative_advancement_matrix[GEOM_HALFSPACE][BV_KDOP24] = &ShapeBVHConservativeAdvancement<Halfspace, KDOP<24>, NarrowPhaseSolver>;

  conservative_advancement_matrix[GEOM_BOX][BV_kIOS] = &ShapeBVHConservativeAdvancement<Boxd, kIOS, NarrowPhaseSolver>;
  conservative_advancement_matrix[GEOM_SPHERE][BV_kIOS] = &ShapeBVHConservativeAdvancement<Sphere, kIOS, NarrowPhaseSolver>;
  conservative_advancement_matrix[GEOM_CAPSULE][BV_kIOS] = &ShapeBVHConservativeAdvancement<Capsuled, kIOS, NarrowPhaseSolver>;
  conservative_advancement_matrix[GEOM_CONE][BV_kIOS] = &ShapeBVHConservativeAdvancement<Coned, kIOS, NarrowPhaseSolver>;
  conservative_advancement_matrix[GEOM_CYLINDER][BV_kIOS] = &ShapeBVHConservativeAdvancement<Cylinder, kIOS, NarrowPhaseSolver>;
  conservative_advancement_matrix[GEOM_CONVEX][BV_kIOS] = &ShapeBVHConservativeAdvancement<Convex, kIOS, NarrowPhaseSolver>;
  conservative_advancement_matrix[GEOM_PLANE][BV_kIOS] = &ShapeBVHConservativeAdvancement<Plane, kIOS, NarrowPhaseSolver>;
  conservative_advancement_matrix[GEOM_HALFSPACE][BV_kIOS] = &ShapeBVHConservativeAdvancement<Halfspace, kIOS, NarrowPhaseSolver>;

  conservative_advancement_matrix[BV_AABB][BV_AABB] = &BVHConservativeAdvancement<AABB, NarrowPhaseSolver>;
  conservative_advancement_matrix[BV_OBB][BV_OBB] = &BVHConservativeAdvancement<OBB, NarrowPhaseSolver>;
  conservative_advancement_matrix[BV_RSS][BV_RSS] = &BVHConservativeAdvancement<RSS, NarrowPhaseSolver>;
  conservative_advancement_matrix[BV_OBBRSS][BV_OBBRSS] = &BVHConservativeAdvancement<OBBRSS, NarrowPhaseSolver>;
  conservative_advancement_matrix[BV_KDOP16][BV_KDOP16] = &BVHConservativeAdvancement<KDOP<16>, NarrowPhaseSolver>;
  conservative_advancement_matrix[BV_KDOP18][BV_KDOP18] = &BVHConservativeAdvancement<KDOP<18>, NarrowPhaseSolver>;
  conservative_advancement_matrix[BV_KDOP24][BV_KDOP24] = &BVHConservativeAdvancement<KDOP<24>, NarrowPhaseSolver>;
  conservative_advancement_matrix[BV_kIOS][BV_kIOS] = &BVHConservativeAdvancement<kIOS, NarrowPhaseSolver>;
  
}


template struct ConservativeAdvancementFunctionMatrix<GJKSolver_libccd>;
template struct ConservativeAdvancementFunctionMatrix<GJKSolver_indep>;









}


