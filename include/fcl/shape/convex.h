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


#ifndef FCL_SHAPE_CONVEX_H
#define FCL_SHAPE_CONVEX_H

#include "fcl/shape/shape_base.h"

namespace fcl
{

/// @brief Convex polytope 
class Convex : public ShapeBased
{
public:
  /// @brief Constructing a convex, providing normal and offset of each polytype surface, and the points and shape topology information 
  Convex(Vector3d* plane_normals_,
         FCL_REAL* plane_dis_,
         int num_planes_,
         Vector3d* points_,
         int num_points_,
         int* polygons_) : ShapeBased()
  {
    plane_normals = plane_normals_;
    plane_dis = plane_dis_;
    num_planes = num_planes_;
    points = points_;
    num_points = num_points_;
    polygons = polygons_;
    edges = NULL;

    Vector3d sum = Vector3d::Zero();
    for(int i = 0; i < num_points; ++i)
    {
      sum += points[i];
    }

    center = sum * (FCL_REAL)(1.0 / num_points);

    fillEdges();
  }

  /// @brief Copy constructor 
  Convex(const Convex& other) : ShapeBase(other)
  {
    plane_normals = other.plane_normals;
    plane_dis = other.plane_dis;
    num_planes = other.num_planes;
    points = other.points;
    polygons = other.polygons;
    edges = new Edge[other.num_edges];
    memcpy(edges, other.edges, sizeof(Edge) * num_edges);
  }

  ~Convex()
  {
    delete [] edges;
  }

  /// @brief Compute AABB 
  void computeLocalAABB();

  /// @brief Get node type: a conex polytope 
  NODE_TYPE getNodeType() const { return GEOM_CONVEX; }

  
  Vector3d* plane_normals;
  FCL_REAL* plane_dis;

  /// @brief An array of indices to the points of each polygon, it should be the number of vertices
  /// followed by that amount of indices to "points" in counter clockwise order
  int* polygons;

  Vector3d* points;
  int num_points;
  int num_edges;
  int num_planes;

  struct Edge
  {
    int first, second;
  };

  Edge* edges;

  /// @brief center of the convex polytope, this is used for collision: center is guaranteed in the internal of the polytope (as it is convex) 
  Vector3d center;

  /// based on http://number-none.com/blow/inertia/bb_inertia.doc
  Matrix3d computeMomentofInertia() const
  {
    
    Matrix3d C = Matrix3d::Zero();

    Matrix3d C_canonical;
    C_canonical << 1/ 60.0, 1/120.0, 1/120.0,
                   1/120.0, 1/ 60.0, 1/120.0,
                   1/120.0, 1/120.0, 1/ 60.0;

    int* points_in_poly = polygons;
    int* index = polygons + 1;
    for(int i = 0; i < num_planes; ++i)
    {
      Vector3d plane_center = Vector3d::Zero();

      // compute the center of the polygon
      for(int j = 0; j < *points_in_poly; ++j)
        plane_center += points[index[j]];
      plane_center = plane_center * (1.0 / *points_in_poly);

      // compute the volume of tetrahedron making by neighboring two points, the plane center and the reference point (zero) of the convex shape
      const Vector3d& v3 = plane_center;
      for(int j = 0; j < *points_in_poly; ++j)
      {
        int e_first = index[j];
        int e_second = index[(j+1)%*points_in_poly];
        const Vector3d& v1 = points[e_first];
        const Vector3d& v2 = points[e_second];
        FCL_REAL d_six_vol = (v1.cross(v2)).dot(v3);
        Matrix3d A; // this is A' in the original document
        A.row(0) = v1;
        A.row(1) = v2;
        A.row(2) = v3;
        C += A.transpose() * C_canonical * A * d_six_vol; // change accordingly
      }
      
      points_in_poly += (*points_in_poly + 1);
      index = points_in_poly + 1;
    }

    FCL_REAL trace_C = C(0, 0) + C(1, 1) + C(2, 2);

    Matrix3d m;
    m << trace_C - C(0, 0), -C(0, 1), -C(0, 2),
         -C(1, 0), trace_C - C(1, 1), -C(1, 2),
         -C(2, 0), -C(2, 1), trace_C - C(2, 2);

    return m;
  }

  Vector3d computeCOM() const
  {
    Vector3d com = Vector3d::Zero();
    FCL_REAL vol = 0;
    int* points_in_poly = polygons;
    int* index = polygons + 1;
    for(int i = 0; i < num_planes; ++i)
    {
      Vector3d plane_center = Vector3d::Zero();

      // compute the center of the polygon
      for(int j = 0; j < *points_in_poly; ++j)
        plane_center += points[index[j]];
      plane_center = plane_center * (1.0 / *points_in_poly);

      // compute the volume of tetrahedron making by neighboring two points, the plane center and the reference point (zero) of the convex shape
      const Vector3d& v3 = plane_center;
      for(int j = 0; j < *points_in_poly; ++j)
      {
        int e_first = index[j];
        int e_second = index[(j+1)%*points_in_poly];
        const Vector3d& v1 = points[e_first];
        const Vector3d& v2 = points[e_second];
        FCL_REAL d_six_vol = (v1.cross(v2)).dot(v3);
        vol += d_six_vol;
        com += (points[e_first] + points[e_second] + plane_center) * d_six_vol;
      }
      
      points_in_poly += (*points_in_poly + 1);
      index = points_in_poly + 1;
    }

    return com / (vol * 4); // here we choose zero as the reference
  }

  FCL_REAL computeVolume() const
  {
    FCL_REAL vol = 0;
    int* points_in_poly = polygons;
    int* index = polygons + 1;
    for(int i = 0; i < num_planes; ++i)
    {
      Vector3d plane_center = Vector3d::Zero();

      // compute the center of the polygon
      for(int j = 0; j < *points_in_poly; ++j)
        plane_center += points[index[j]];
      plane_center = plane_center * (1.0 / *points_in_poly);

      // compute the volume of tetrahedron making by neighboring two points, the plane center and the reference point (zero point) of the convex shape
      const Vector3d& v3 = plane_center;
      for(int j = 0; j < *points_in_poly; ++j)
      {
        int e_first = index[j];
        int e_second = index[(j+1)%*points_in_poly];
        const Vector3d& v1 = points[e_first];
        const Vector3d& v2 = points[e_second];
        FCL_REAL d_six_vol = (v1.cross(v2)).dot(v3);
        vol += d_six_vol;
      }
      
      points_in_poly += (*points_in_poly + 1);
      index = points_in_poly + 1;
    }

    return vol / 6;
  }

  

protected:
  /// @brief Get edge information 
  void fillEdges();
};

}

#endif
