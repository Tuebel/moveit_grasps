/*********************************************************************
 * Software License Agreement (BSD License)
 *
 *  Copyright (c) 2015, University of Colorado, Boulder
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
 *   * Neither the name of the Univ of CO, Boulder nor the names of its
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
 *********************************************************************/

/* Author: Dave Coleman <dave@dav.ee>, Andy McEvoy
   Desc:   Generates geometric grasps for cuboids and blocks, not using physics or contact wrenches
*/

#include <moveit_grasps/grasp_generator.h>

// Parameter loading
#include <rviz_visual_tools/ros_param_utilities.h>

namespace moveit_grasps
{

// Constructor
GraspGenerator::GraspGenerator(moveit_visual_tools::MoveItVisualToolsPtr visual_tools, bool verbose)
  : visual_tools_(visual_tools)
  , verbose_(verbose)
  , nh_("~/generator")
{
  // Load visulization settings
  const std::string parent_name = "grasps"; // for namespacing logging messages
  rviz_visual_tools::getBoolParameter(parent_name, nh_, "verbose", verbose_);

  rviz_visual_tools::getBoolParameter(parent_name, nh_, "show_grasp_arrows", show_grasp_arrows_);
  rviz_visual_tools::getDoubleParameter(parent_name, nh_, "show_grasp_arrows_speed", show_grasp_arrows_speed_);

  rviz_visual_tools::getBoolParameter(parent_name, nh_, "show_prefiltered_grasps", show_prefiltered_grasps_);
  rviz_visual_tools::getDoubleParameter(parent_name, nh_, "show_prefiltered_grasps_speed", show_prefiltered_grasps_speed_);

  ROS_INFO_STREAM_NAMED("grasps","GraspGenerator Ready.");
}

bool GraspGenerator::generateCuboidAxisGrasps(const Eigen::Affine3d& cuboid_pose, double depth, double width,double height, 
                                              grasp_axis_t axis, const moveit_grasps::GraspDataPtr grasp_data,
                                              std::vector<GraspCandidatePtr>& grasp_candidates)
{
  double finger_depth = grasp_data->finger_to_palm_depth_ - grasp_data->grasp_min_depth_;
  double length_along_a, length_along_b;
  double delta_a, delta_b, delta_f;
  double alpha_x, alpha_y, alpha_z;
  double object_width;
  std::vector<Eigen::Affine3d> grasp_poses;

  Eigen::Affine3d grasp_pose = cuboid_pose;
  Eigen::Vector3d a_dir, b_dir; 

  switch(axis)
  {
    case X_AXIS:
      length_along_a = width;
      length_along_b = height;
      a_dir = grasp_pose.rotation() * Eigen::Vector3d::UnitY();
      b_dir = grasp_pose.rotation() * Eigen::Vector3d::UnitZ();
      alpha_x = -M_PI / 2.0;
      alpha_y = 0;
      alpha_z = -M_PI / 2.0;
      object_width = depth;
      break;
    case Y_AXIS:
      length_along_a = depth;
      length_along_b = height;      
      a_dir = grasp_pose.rotation() * Eigen::Vector3d::UnitX();
      b_dir = grasp_pose.rotation() * Eigen::Vector3d::UnitZ();
      alpha_x = 0;
      alpha_y = M_PI / 2.0;
      alpha_z = M_PI;
      object_width = width;
      break;
    case Z_AXIS:
      length_along_a = depth;
      length_along_b = width;
      a_dir = grasp_pose.rotation() * Eigen::Vector3d::UnitX();
      b_dir = grasp_pose.rotation() * Eigen::Vector3d::UnitY();
      alpha_x = M_PI / 2.0;
      alpha_y = M_PI / 2.0;
      alpha_z = 0;
      object_width = height;
      break;
    default:
      ROS_WARN_STREAM_NAMED("cuboid_axis_grasps","axis not defined properly");
      break;
  }

  double rotation_angles[3];
  rotation_angles[0] = alpha_x;
  rotation_angles[1] = alpha_y;
  rotation_angles[2] = alpha_z;

  a_dir = a_dir.normalized();
  b_dir = b_dir.normalized();

  /***** Add grasps at corners, grasps are centroid aligned *****/
  ROS_DEBUG_STREAM_NAMED("cuboid_axis_grasps","adding corner grasps...");

  double offset = 0.001; // back the palm off of the object slightly
  Eigen::Vector3d corner_translation_a = 0.5 * (length_along_a + offset) * a_dir;
  Eigen::Vector3d corner_translation_b = 0.5 * (length_along_b + offset) * b_dir;
  double angle_res = grasp_data->angle_resolution_ * M_PI / 180.0;
  std::size_t num_radial_grasps = ceil( ( M_PI / 2.0 ) / angle_res  );

  if (num_radial_grasps <=0)
    num_radial_grasps = 1;

  // move to corner 0.5 * ( -a, -b)
  Eigen::Vector3d translation = -corner_translation_a - corner_translation_b;
  addCornerGraspsHelper(cuboid_pose, rotation_angles, translation, 0.0, num_radial_grasps, grasp_poses);

  // move to corner 0.5 * ( -a, +b)
  translation = -corner_translation_a + corner_translation_b;
  addCornerGraspsHelper(cuboid_pose, rotation_angles, translation, -M_PI / 2.0, num_radial_grasps, grasp_poses);

  // move to corner 0.5 * ( +a, +b)
  translation = corner_translation_a + corner_translation_b;
  addCornerGraspsHelper(cuboid_pose, rotation_angles, translation, M_PI, num_radial_grasps, grasp_poses);

  // move to corner 0.5 * ( +a, -b)
  translation = corner_translation_a - corner_translation_b;
  addCornerGraspsHelper(cuboid_pose, rotation_angles, translation, M_PI / 2.0, num_radial_grasps, grasp_poses);

  std::size_t num_corner_grasps = grasp_poses.size();

  /***** Create grasps along faces of cuboid, grasps are axis aligned *****/
  ROS_DEBUG_STREAM_NAMED("cuboid_axis_grasps","adding face grasps...");
  // get exact deltas for sides from desired delta
  std::size_t num_grasps_along_a = floor( (length_along_a - grasp_data->gripper_finger_width_) / grasp_data->grasp_resolution_ ) + 1;
  std::size_t num_grasps_along_b = floor( (length_along_b - grasp_data->gripper_finger_width_) / grasp_data->grasp_resolution_ ) + 1; 

  // if the gripper fingers are wider than the object we're trying to grasp, try with gripper aligned with top/center/bottom of object
  // note that current implementation limits objects that are the same size as the gripper_finger_width to 1 grasp
  if (num_grasps_along_a <= 0)
  {
    delta_a = length_along_a - grasp_data->gripper_finger_width_ / 2.0;
    num_grasps_along_a = 3;
  }
  if (num_grasps_along_b <= 0)
  {
    delta_b = length_along_b - grasp_data->gripper_finger_width_ / 2.0;
    num_grasps_along_b = 3;
  }

  if (num_grasps_along_a == 1)
    delta_a = 0;
  else
    delta_a = (length_along_a - grasp_data->gripper_finger_width_) / (double)(num_grasps_along_a - 1);

  if (num_grasps_along_b == 1)
    delta_b = 0;
  else
    delta_b = (length_along_b - grasp_data->gripper_finger_width_) / (double)(num_grasps_along_b - 1);

  // ROS_DEBUG_STREAM_NAMED("cuboid_axis_grasps","delta_a : delta_b = " << delta_a << " : " << delta_b);
  // ROS_DEBUG_STREAM_NAMED("cuboid_axis_grasps","num_grasps_along_a : num_grasps_along_b  = " << num_grasps_along_a << " : " << 
  //                        num_grasps_along_b);

  Eigen::Vector3d a_translation = -(0.5 * (length_along_a + offset) * a_dir) -
    0.5 * (length_along_b - grasp_data->gripper_finger_width_) * b_dir - delta_b * b_dir;
  Eigen::Vector3d b_translation = -0.5 * (length_along_a - grasp_data->gripper_finger_width_) * a_dir - 
    delta_a * a_dir - (0.5 * (length_along_b + offset) * b_dir);

  // grasps along -a_dir face
  Eigen::Vector3d delta = delta_b * b_dir;
  double rotation = 0.0;
  addFaceGraspsHelper(cuboid_pose, rotation_angles, a_translation, delta, rotation, num_grasps_along_b, grasp_poses);

  // grasps along +b_dir face
  rotation = -M_PI / 2.0;
  delta = -delta_a * a_dir;
  addFaceGraspsHelper(cuboid_pose, rotation_angles, -b_translation, delta, rotation, num_grasps_along_b, grasp_poses);  

  // grasps along +a_dir face
  rotation = M_PI;
  delta = -delta_b * b_dir;
  addFaceGraspsHelper(cuboid_pose, rotation_angles, -a_translation, delta, rotation, num_grasps_along_b, grasp_poses);  

  // grasps along -b_dir face
  rotation = M_PI / 2.0;
  delta = delta_a * a_dir;
  addFaceGraspsHelper(cuboid_pose, rotation_angles, b_translation, delta, rotation, num_grasps_along_b, grasp_poses);  

  /***** Add grasps at variable depths *****/
  ROS_DEBUG_STREAM_NAMED("cuboid_axis_grasps","adding depth grasps...");
  std::size_t num_depth_grasps = ceil( finger_depth / grasp_data->grasp_depth_resolution_ );
  if (num_depth_grasps <= 0)
    num_depth_grasps = 1;
  delta_f = finger_depth / (double)(num_depth_grasps);
  // ROS_DEBUG_STREAM_NAMED("cuboid_axis_grasps","delta_f = " << delta_f );
  // ROS_DEBUG_STREAM_NAMED("cuboid_axis_grasps","num_depth_grasps = " << num_depth_grasps);

  std::size_t num_grasps = grasp_poses.size();
  Eigen::Vector3d grasp_dir;
  Eigen::Affine3d depth_pose;

  for (std::size_t i = 0; i < num_grasps; i++)
  {
    grasp_dir = grasp_poses[i].rotation() * Eigen::Vector3d::UnitZ();
    depth_pose = grasp_poses[i];
    for (std::size_t j = 0; j < num_depth_grasps; j++)
    {
      depth_pose.translation() -= delta_f * grasp_dir;
      grasp_poses.push_back(depth_pose);
    }
  }

  /***** add grasps at variable angles *****/
  ROS_DEBUG_STREAM_NAMED("cuboid_axis_grasps","adding variable angle grasps...");
  Eigen::Affine3d base_pose;
  num_grasps = grasp_poses.size();
  for (std::size_t i = num_corner_grasps; i < num_grasps; i++) // corner grasps at zero depth don't need variable angles
  {
    base_pose = grasp_poses[i];

    grasp_pose = base_pose * Eigen::AngleAxisd(angle_res, Eigen::Vector3d::UnitY());
    std::size_t max_iterations = M_PI / angle_res + 1;
    std::size_t iterations = 0;
    while (graspIntersectionHelper(cuboid_pose, depth, width, height, grasp_pose, grasp_data) )
    {
      grasp_poses.push_back(grasp_pose);
      //visual_tools_->publishZArrow(grasp_pose, rviz_visual_tools::BLUE, rviz_visual_tools::XSMALL, 0.02);
      grasp_pose *= Eigen::AngleAxisd(angle_res, Eigen::Vector3d::UnitY());    
      //ros::Duration(0.2).sleep();
      iterations++;
      if (iterations > max_iterations)
      {
        ROS_WARN_STREAM_NAMED("cuboid_axis_grasps","exceeded max iterations while creating variable angle grasps");
        break;
      }
    }
    
    iterations = 0;
    grasp_pose = base_pose * Eigen::AngleAxisd(-angle_res, Eigen::Vector3d::UnitY());  
    while (graspIntersectionHelper(cuboid_pose, depth, width, height, grasp_pose, grasp_data) )
    {
      grasp_poses.push_back(grasp_pose);
      //visual_tools_->publishZArrow(grasp_pose, rviz_visual_tools::CYAN, rviz_visual_tools::XSMALL, 0.02);
      grasp_pose *= Eigen::AngleAxisd(-angle_res, Eigen::Vector3d::UnitY());    
      //ros::Duration(0.2).sleep();
      iterations++;
      if (iterations > max_iterations)
      {
        ROS_WARN_STREAM_NAMED("cuboid_axis_grasps","exceeded max iterations while creating variable angle grasps");
        break;
      }
    }
  }

  /***** add grasps in both directions *****/
  ROS_DEBUG_STREAM_NAMED("cuboid_axis_grasps","adding bi-directional grasps...");
  num_grasps = grasp_poses.size();
  for (std::size_t i = 0; i < num_grasps; i++)
  {
    grasp_pose = grasp_poses[i];
    grasp_pose *= Eigen::AngleAxisd(M_PI, Eigen::Vector3d::UnitZ());
    grasp_poses.push_back(grasp_pose);
  }

  /***** add all poses as possible grasps *****/
  for (std::size_t i = 0; i < grasp_poses.size(); i++)
  {
    addGrasp(grasp_poses[i], grasp_data, grasp_candidates, cuboid_pose, object_width);
  }
  ROS_DEBUG_STREAM_NAMED("cuboid_axis_grasps","created " << grasp_poses.size() << " grasp poses");

  return true;
}

std::size_t GraspGenerator::addFaceGraspsHelper(Eigen::Affine3d pose, double rotation_angles[3], Eigen::Vector3d translation,
                                                Eigen::Vector3d delta, double alignment_rotation, std::size_t num_grasps, 
                                                std::vector<Eigen::Affine3d>& grasp_poses)
{
  std::size_t num_grasps_added = 0;
  ROS_DEBUG_STREAM_NAMED("cuboid_axis_grasps.helper","delta = \n" << delta);
  ROS_DEBUG_STREAM_NAMED("cuboid_axis_grasps.helper","num_grasps = " << num_grasps);

  Eigen::Affine3d grasp_pose = pose; 
  grasp_pose *= Eigen::AngleAxisd(rotation_angles[0], Eigen::Vector3d::UnitX()) * 
    Eigen::AngleAxisd(rotation_angles[1], Eigen::Vector3d::UnitY()) * 
    Eigen::AngleAxisd(rotation_angles[2], Eigen::Vector3d::UnitZ());
  grasp_pose *= Eigen::AngleAxisd(alignment_rotation, Eigen::Vector3d::UnitY()); 
  grasp_pose.translation() += translation;

  for (std::size_t i = 0; i < num_grasps; i++)
  {
    grasp_pose.translation() += delta;
    grasp_poses.push_back(grasp_pose);
    num_grasps_added++;
  }
  ROS_DEBUG_STREAM_NAMED("cuboid_axis_grasps.helper","num_grasps_added : grasp_poses.size() = " 
                         << num_grasps_added << " : " << grasp_poses.size());
  return true;
}

std::size_t GraspGenerator::addCornerGraspsHelper(Eigen::Affine3d pose, double rotation_angles[3], Eigen::Vector3d translation, 
                                                  double corner_rotation, std::size_t num_radial_grasps, 
                                                  std::vector<Eigen::Affine3d>& grasp_poses)
{
  std::size_t num_grasps_added = 0;
  double delta_angle = ( M_PI / 2.0 ) / (double)(num_radial_grasps + 1);
  ROS_DEBUG_STREAM_NAMED("cuboid_axis_grasps.helper","delta_angle = " << delta_angle);
  ROS_DEBUG_STREAM_NAMED("cuboid_axis_grasps.helper","num_radial_grasps = " << num_radial_grasps);

  // rotate & translate pose to be aligned with edge of cuboid
  Eigen::Affine3d grasp_pose = pose;
  grasp_pose *= Eigen::AngleAxisd(rotation_angles[0], Eigen::Vector3d::UnitX()) * 
    Eigen::AngleAxisd(rotation_angles[1], Eigen::Vector3d::UnitY()) * 
    Eigen::AngleAxisd(rotation_angles[2], Eigen::Vector3d::UnitZ());
  grasp_pose *= Eigen::AngleAxisd(corner_rotation, Eigen::Vector3d::UnitY());
  grasp_pose.translation() += translation;

  for (std::size_t i = 0; i < num_radial_grasps; i++)
  {
    //Eigen::Vector3d grasp_dir = grasp_pose.rotation() * Eigen::Vector3d::UnitZ();
    //Eigen::Affine3d radial_pose = grasp_pose;
    grasp_pose *= Eigen::AngleAxisd(delta_angle, Eigen::Vector3d::UnitY());
    grasp_poses.push_back(grasp_pose);
    num_grasps_added++;
  }
  ROS_DEBUG_STREAM_NAMED("cuboid_axis_grasps.helper","num_grasps_added : grasp_poses.size() = " 
                         << num_grasps_added << " : " << grasp_poses.size());
  return num_grasps_added;
}

bool GraspGenerator::graspIntersectionHelper(Eigen::Affine3d cuboid_pose, double depth, double width, double height,
                                             Eigen::Affine3d grasp_pose, const GraspDataPtr grasp_data)
{
  // TODO: remove vizualization commented lines after further testing
  
  // get line segment from grasp point to fingertip
  Eigen::Vector3d point_a = grasp_pose.translation();
  Eigen::Vector3d point_b = point_a + grasp_pose.rotation() * Eigen::Vector3d::UnitZ() * grasp_data->finger_to_palm_depth_;

  // translate points into cuboid coordinate system 
  point_a = cuboid_pose.inverse() * point_a; // T_cuboid-world * p_world = p_cuboid
  point_b = cuboid_pose.inverse() * point_b;

  // if (verbose_)
  // {
  //   visual_tools_->publishCuboid(visual_tools_->convertPose(Eigen::Affine3d::Identity()), depth, width, height, rviz_visual_tools::TRANSLUCENT);
  //   visual_tools_->publishAxis(Eigen::Affine3d::Identity());
  //   visual_tools_->publishSphere(point_a, rviz_visual_tools::WHITE, 0.005);
  //   visual_tools_->publishSphere(point_b, rviz_visual_tools::GREY, 0.005);
  //   visual_tools_->publishLine(point_a, point_b, rviz_visual_tools::BLUE, rviz_visual_tools::XSMALL);
  // }


  double t, u, v;
  Eigen::Vector3d intersection;
  // check if line segment intersects XY faces of cuboid (z = +/- height/2)
  t = ( height / 2.0 - point_a[2] ) / ( point_b[2] - point_a[2] ); // parameterization of line segment in 3d
  if ( intersectionHelper(t, point_a[0], point_a[1], point_b[0], point_b[1], depth, width, u, v) )
  {
    // if (verbose_)
    // {
    //   intersection[0]= u;
    //   intersection[1]= v;
    //   intersection[2]= height / 2.0;
    //   visual_tools_->publishSphere(intersection, rviz_visual_tools::BLUE, 0.005);
    // }
    return true;
  }

  t = ( -height / 2.0 - point_a[2] ) / ( point_b[2] - point_a[2] );
  if ( intersectionHelper(t, point_a[0], point_a[1], point_b[0], point_b[1], depth, width, u, v) )
  {
    // if (verbose_)
    // {
    //   intersection[0]= u;
    //   intersection[1]= v;
    //   intersection[2]= -height / 2.0;
    //   visual_tools_->publishSphere(intersection, rviz_visual_tools::CYAN, 0.005);
    // }
    return true;
  }

  // check if line segment intersects XZ faces of cuboid (y = +/- width/2)
  t = ( width / 2.0 - point_a[1] ) / ( point_b[1] - point_a[1] ); 
  if ( intersectionHelper(t, point_a[0], point_a[2], point_b[0], point_b[2], depth, height, u, v) )
  {
    // if (verbose_)
    // {
    //   intersection[0]= u;
    //   intersection[1]= width / 2.0;
    //   intersection[2]= v;
    //   visual_tools_->publishSphere(intersection, rviz_visual_tools::GREEN, 0.005);
    // }
    return true;
  }

  t = ( -width / 2.0 - point_a[1] ) / ( point_b[1] - point_a[1] );
  if ( intersectionHelper(t, point_a[0], point_a[2], point_b[0], point_b[2], depth, height, u, v) )
  {
    // if (verbose_)
    // {
    //   intersection[0]= u;
    //   intersection[1]= -width / 2.0;
    //   intersection[2]= v;
    //   visual_tools_->publishSphere(intersection, rviz_visual_tools::LIME_GREEN, 0.005);
    // }
    return true;
  }

  // check if line segment intersects YZ faces of cuboid (x = +/- depth/2)
  t = ( depth / 2.0 - point_a[0] ) / ( point_b[0] - point_a[0] ); 
  if ( intersectionHelper(t, point_a[1], point_a[2], point_b[1], point_b[2], width, height, u, v) )
  {
    // if (verbose_)
    // {
    //   intersection[0]= depth / 2.0;
    //   intersection[1]= u;
    //   intersection[2]= v;
    //   visual_tools_->publishSphere(intersection, rviz_visual_tools::RED, 0.005);
    // }
    return true;
  }

  t = ( -depth / 2.0 - point_a[0] ) / ( point_b[0] - point_a[0] ); 
  if ( intersectionHelper(t, point_a[1], point_a[2], point_b[1], point_b[2], width, height, u, v) )
  {
    // if (verbose_)
    // {
    //   intersection[0]= -depth / 2.0;
    //   intersection[1]= u;
    //   intersection[2]= v;
    //   visual_tools_->publishSphere(intersection, rviz_visual_tools::PINK, 0.005);
    // }
    return true;
  }

  // no intersection found
  return false;
}

bool GraspGenerator::intersectionHelper(double t, double u1, double v1, double u2, double v2, double a, double b, double& u, double& v)
{
  //double u, v;
  // plane must cross through our line segment
  if (t >= 0 && t <= 1)
  {
    u = u1 + t * (u2 - u1);
    v = v1 + t * (v2 - v1);
    
    if (u >= -a/2 && u <= a/2 && v >= -b/2 && v <= b/2)
      return true;
  }

  return false;
}

void GraspGenerator::addGrasp(const Eigen::Affine3d& grasp_pose, const GraspDataPtr grasp_data,
                              std::vector<GraspCandidatePtr>& grasp_candidates, const Eigen::Affine3d& object_pose, 
                              double object_width)
{
  if (verbose_)
  {
    visual_tools_->publishAxis(grasp_pose, 0.02, 0.002);
    //visual_tools_->publishZArrow(grasp_pose, rviz_visual_tools::BLUE, rviz_visual_tools::XSMALL, 0.01);
    ros::Duration(0.01).sleep();
  }

  // The new grasp
  moveit_msgs::Grasp new_grasp;

  // Approach and retreat
  // aligned with pose (aligned with grasp pose z-axis
  // TODO:: Currently the pre/post approach/retreat translations are not robot agnostic.
  // It currently being loaded with the assumption that z-axis is pointing away from object.

  // set pregrasp
  moveit_msgs::GripperTranslation pre_grasp_approach;
  new_grasp.pre_grasp_approach.direction.header.stamp = ros::Time::now();
  new_grasp.pre_grasp_approach.desired_distance = grasp_data->finger_to_palm_depth_ + grasp_data->approach_distance_desired_;
  new_grasp.pre_grasp_approach.min_distance = 0; // NOT IMPLEMENTED
  new_grasp.pre_grasp_approach.direction.header.frame_id = grasp_data->parent_link_->getName();
  new_grasp.pre_grasp_approach.direction.vector.x = 0;
  new_grasp.pre_grasp_approach.direction.vector.y = 0;
  new_grasp.pre_grasp_approach.direction.vector.z = -1;
  // new_grasp.pre_grasp_approach.direction.header.frame_id = "world";
  // new_grasp.pre_grasp_approach.direction.vector.x = 1;
  // new_grasp.pre_grasp_approach.direction.vector.y = 0;
  // new_grasp.pre_grasp_approach.direction.vector.z = 0;

  // set postgrasp
  moveit_msgs::GripperTranslation post_grasp_retreat;
  new_grasp.post_grasp_retreat.direction.header.stamp = ros::Time::now();
  new_grasp.post_grasp_retreat.desired_distance = grasp_data->finger_to_palm_depth_ + grasp_data->retreat_distance_desired_;
  new_grasp.post_grasp_retreat.min_distance = 0; // NOT IMPLEMENTED
  new_grasp.post_grasp_retreat.direction.header.frame_id = grasp_data->parent_link_->getName();
  new_grasp.post_grasp_retreat.direction.vector.x = 0;
  new_grasp.post_grasp_retreat.direction.vector.y = 0;
  new_grasp.post_grasp_retreat.direction.vector.z = 1;
  // new_grasp.post_grasp_retreat.direction.header.frame_id = "world";
  // new_grasp.post_grasp_retreat.direction.vector.x = 1;
  // new_grasp.post_grasp_retreat.direction.vector.y = 0;
  // new_grasp.post_grasp_retreat.direction.vector.z = 0;

  // pre-grasp and grasp postures e.g. hand open close values
  new_grasp.pre_grasp_posture = grasp_data->pre_grasp_posture_;
  new_grasp.grasp_posture = grasp_data->grasp_posture_;

  // set minimum opening of fingers for pre grasp approach
  new_grasp.min_finger_open_on_approach = object_width;

  // set grasp pose
  geometry_msgs::PoseStamped grasp_pose_msg;
  grasp_pose_msg.header.stamp = ros::Time::now();
  grasp_pose_msg.header.frame_id = grasp_data->base_link_;

  // name the grasp
  static std::size_t grasp_id = 0;
  new_grasp.id = "Grasp" + boost::lexical_cast<std::string>(grasp_id);
  grasp_id++;

  // compute grasp score
  new_grasp.grasp_quality = scoreGrasp(grasp_pose, grasp_data, object_pose);

  if (verbose_)
  {
    visual_tools_->publishAxis(ideal_grasp_pose_);
    visual_tools_->publishSphere(grasp_pose.translation(), rviz_visual_tools::PINK, 0.01 * new_grasp.grasp_quality);
  }

  // translate and rotate gripper to match standard orientation
  // origin on palm, z pointing outward, x perp to gripper close, y parallel to gripper close direction
  // Transform the grasp pose

  tf::poseEigenToMsg(grasp_pose * grasp_data->grasp_pose_to_eef_pose_, grasp_pose_msg.pose);
  new_grasp.grasp_pose = grasp_pose_msg;
  grasp_candidates.push_back(GraspCandidatePtr(new GraspCandidate(new_grasp, grasp_data)));
}

double GraspGenerator::scoreGrasp(const Eigen::Affine3d& pose, const GraspDataPtr grasp_data, const Eigen::Affine3d object_pose)
{
  // set ideal grasp pose (TODO: remove this and set programatically)
  ideal_grasp_pose_ = Eigen::Affine3d::Identity();
  ideal_grasp_pose_ *= Eigen::AngleAxisd(M_PI / 2.0, Eigen::Vector3d::UnitY()) * 
    Eigen::AngleAxisd(M_PI / 2.0, Eigen::Vector3d::UnitZ());
  std::size_t num_tests = 3;

  double individual_scores[num_tests];
  double score_weights[num_tests];
  
  // initialize score_weights as 1
  for (std::size_t i = 0; i < num_tests; i++)
    score_weights[i] = 1;
  
  // how close is z-axis of grasp to desired orientation? (0 = 180 degrees of, 100 = 0 degrees off)
  Eigen::Vector3d axis_grasp = pose.rotation() * Eigen::Vector3d::UnitZ();
  Eigen::Vector3d axis_desired = ideal_grasp_pose_.rotation() * Eigen::Vector3d::UnitZ();
  double angle = acos( axis_grasp.dot(axis_desired) );
  individual_scores[0] = ( M_PI - angle ) / M_PI;

  // is camera pointed up? (angle betweeen y-axes) (0 = 180 degrees of, 100 = 0 degrees off)
  axis_grasp = pose.rotation() * Eigen::Vector3d::UnitY();
  axis_desired = ideal_grasp_pose_.rotation() * Eigen::Vector3d::UnitY();
  angle = acos( axis_grasp.dot(axis_desired) );
  individual_scores[1] = ( M_PI - angle ) / M_PI;

  // how close is the palm to the object? (0 = at finger length, 100 = in palm)
  // TODO: not entierly correct since measuring from centroid of object.
  double finger_length = grasp_data->finger_to_palm_depth_ - grasp_data->grasp_min_depth_;
  Eigen::Vector3d delta = pose.translation() - object_pose.translation();
  double distance = delta.norm();
  if (distance > finger_length)
    individual_scores[2] = 0;
  else
    individual_scores[2] = ( finger_length - distance ) / finger_length;
  
  // compute combined score
  double score_sum = 0;
  for (std::size_t i = 0; i < num_tests; i++)
  {
    score_sum += individual_scores[i] * score_weights[i];
  }
  
  return ( score_sum / (double)num_tests );
}

bool GraspGenerator::generateGrasps(const shape_msgs::Mesh& mesh_msg, const Eigen::Affine3d& cuboid_pose,
                                    const moveit_grasps::GraspDataPtr grasp_data,
                                    std::vector<GraspCandidatePtr>& grasp_candidates)
{
  double depth;
  double width;
  double height;
  Eigen::Affine3d mesh_pose;  
  if (!bounding_box_.getBodyAlignedBoundingBox(mesh_msg, mesh_pose, depth, width, height))
  {
    ROS_ERROR_STREAM_NAMED("grasp_generator","Unable to get bounding box from mesh");
    return false;
  }

  // TODO - reconcile the new mesh_pose with the input cuboid_pose

  return generateGrasps(cuboid_pose, depth, width, height, grasp_data, grasp_candidates);
}

bool GraspGenerator::generateGrasps(const Eigen::Affine3d& cuboid_pose, double depth, double width, double height,
                                    const moveit_grasps::GraspDataPtr grasp_data,
                                    std::vector<GraspCandidatePtr>& grasp_candidates)
{
  // Generate grasps over axes that aren't too wide to grip

  // Most default type of grasp is X axis
  if (depth <= grasp_data->max_grasp_width_ ) // depth = size along x-axis
  {
    ROS_DEBUG_STREAM_NAMED("grasp_generator","Generating grasps around x-axis of cuboid");
    generateCuboidAxisGrasps(cuboid_pose, depth, width, height, X_AXIS, grasp_data, grasp_candidates);
  }

  if (width <= grasp_data->max_grasp_width_ ) // width = size along y-axis
  {
    ROS_DEBUG_STREAM_NAMED("grasp_generator","Generating grasps around y-axis of cuboid");
    generateCuboidAxisGrasps(cuboid_pose, depth, width, height, Y_AXIS, grasp_data, grasp_candidates);
  }

  if (height <= grasp_data->max_grasp_width_ ) // height = size along z-axis
  {
    ROS_DEBUG_STREAM_NAMED("grasp_generator","Generating grasps around z-axis of cuboid");
    generateCuboidAxisGrasps(cuboid_pose, depth, width, height, Z_AXIS, grasp_data, grasp_candidates);
  }

  if (!grasp_candidates.size())
    ROS_WARN_STREAM_NAMED("grasp_generator","Generated 0 grasps");
  else
    ROS_INFO_STREAM_NAMED("grasp_generator","Generated " << grasp_candidates.size() << " grasps");

  // Visualize animated grasps that have been generated
  if (show_prefiltered_grasps_)
  {
    ROS_DEBUG_STREAM_NAMED("grasp_generator","Animating all generated (candidate) grasps before filtering");
    visualizeAnimatedGrasps(grasp_candidates, grasp_data->ee_jmg_, show_prefiltered_grasps_speed_);
  }

  return true;
}

Eigen::Vector3d GraspGenerator::getPreGraspDirection(const moveit_msgs::Grasp &grasp, const std::string &ee_parent_link)
{
  // Grasp Pose Variables
  Eigen::Affine3d grasp_pose_eigen;
  tf::poseMsgToEigen(grasp.grasp_pose.pose, grasp_pose_eigen);

  // The direction of the pre-grasp
  Eigen::Vector3d pre_grasp_approach_direction = -1 * Eigen::Vector3d(grasp.pre_grasp_approach.direction.vector.x,    
                                                                      grasp.pre_grasp_approach.direction.vector.y,
                                                                      grasp.pre_grasp_approach.direction.vector.z);

  // Approach direction
  Eigen::Vector3d pre_grasp_approach_direction_local;

  // Decide if we need to change the approach_direction to the local frame of the end effector orientation
  if( grasp.pre_grasp_approach.direction.header.frame_id == ee_parent_link )
  {
    //ROS_WARN_STREAM_NAMED("grasp_generator","Pre grasp approach direction frame_id is " << ee_parent_link);
    // Apply/compute the approach_direction vector in the local frame of the grasp_pose orientation
    pre_grasp_approach_direction_local = grasp_pose_eigen.rotation() * pre_grasp_approach_direction;
  }
  else
  {
    pre_grasp_approach_direction_local = pre_grasp_approach_direction; //grasp_pose_eigen.rotation() * pre_grasp_approach_direction;
  }
  
  return pre_grasp_approach_direction_local;
}

geometry_msgs::PoseStamped GraspGenerator::getPreGraspPose(const moveit_msgs::Grasp &grasp, const std::string &ee_parent_link)
{
  // Grasp Pose Variables
  Eigen::Affine3d grasp_pose_eigen;
  tf::poseMsgToEigen(grasp.grasp_pose.pose, grasp_pose_eigen);

  // Get pre-grasp pose first
  geometry_msgs::PoseStamped pre_grasp_pose;
  Eigen::Affine3d pre_grasp_pose_eigen = grasp_pose_eigen; // Copy original grasp pose to pre-grasp pose

  // Approach direction
  Eigen::Vector3d pre_grasp_approach_direction_local = getPreGraspDirection(grasp, ee_parent_link);

  // Update the grasp matrix usign the new locally-framed approach_direction
  pre_grasp_pose_eigen.translation() += pre_grasp_approach_direction_local * grasp.pre_grasp_approach.desired_distance;

  // Convert eigen pre-grasp position back to regular message
  tf::poseEigenToMsg(pre_grasp_pose_eigen, pre_grasp_pose.pose);

  // Copy original header to new grasp
  pre_grasp_pose.header = grasp.grasp_pose.header;

  return pre_grasp_pose;
}

// Eigen::Vector3d GraspGenerator::getPostGraspDirection(const moveit_msgs::Grasp &grasp, const std::string &ee_parent_link)
// {
//   // Grasp Pose Variables
//   Eigen::Affine3d grasp_pose_eigen;
//   tf::poseMsgToEigen(grasp.grasp_pose.pose, grasp_pose_eigen);

//   // The direction of the pre-grasp
//   Eigen::Vector3d post_grasp_approach_direction = Eigen::Vector3d(grasp.post_grasp_approach.direction.vector.x,    
//                                                                  grasp.post_grasp_approach.direction.vector.y,
//                                                                  grasp.post_grasp_approach.direction.vector.z);

//   // Approach direction
//   Eigen::Vector3d post_grasp_approach_direction_local;

//   // Decide if we need to change the approach_direction to the local frame of the end effector orientation
//   if( grasp.post_grasp_approach.direction.header.frame_id == ee_parent_link )
//   {
//     ROS_WARN_STREAM_NAMED("grasp_generator","Post grasp approach direction frame_id is " << ee_parent_link);
//     // Apply/compute the approach_direction vector in the local frame of the grasp_pose orientation
//     post_grasp_approach_direction_local = grasp_pose_eigen.rotation() * post_grasp_approach_direction;
//   }
//   else
//   {
//     post_grasp_approach_direction_local = post_grasp_approach_direction; //grasp_pose_eigen.rotation() * post_grasp_approach_direction;
//   }
  
//   return post_grasp_approach_direction_local;
// }

// geometry_msgs::PoseStamped GraspGenerator::getPostGraspPose(const moveit_msgs::Grasp &grasp, const std::string &ee_parent_link)
// {
//   // Grasp Pose Variables
//   Eigen::Affine3d grasp_pose_eigen;
//   tf::poseMsgToEigen(grasp.grasp_pose.pose, grasp_pose_eigen);

//   // Get post-grasp pose first
//   geometry_msgs::PoseStamped post_grasp_pose;
//   Eigen::Affine3d post_grasp_pose_eigen = grasp_pose_eigen; // Copy original grasp pose to post-grasp pose

//   // Update the grasp matrix usign the new locally-framed approach_direction
//   post_grasp_pose_eigen.translation() += getPostGraspDirection(grasp, ee_parent_link) * grasp.post_grasp_approach.desired_distance;

//   // Convert eigen post-grasp position back to regular message
//   tf::poseEigenToMsg(post_grasp_pose_eigen, post_grasp_pose.pose);

//   // Copy original header to new grasp
//   post_grasp_pose.header = grasp.grasp_pose.header;

//   return post_grasp_pose;
// }

void GraspGenerator::publishGraspArrow(geometry_msgs::Pose grasp, const GraspDataPtr grasp_data,
                                       const rviz_visual_tools::colors &color, double approach_length)
{
  //Eigen::Affine3d eigen_grasp_pose;
  // Convert each grasp back to forward-facing error (undo end effector custom rotation)
  //tf::poseMsgToEigen(grasp, eigen_grasp_pose);
  //eigen_grasp_pose = eigen_grasp_pose * grasp_data->grasp_pose_to_eef_pose_.inverse();

  //visual_tools_->publishArrow(eigen_grasp_pose, color, rviz_visual_tools::REGULAR);
  visual_tools_->publishArrow(grasp, color, rviz_visual_tools::REGULAR);
}

bool GraspGenerator::visualizeAnimatedGrasps(const std::vector<GraspCandidatePtr>& grasp_candidates,
                                             const moveit::core::JointModelGroup* ee_jmg, double animation_speed)
{
  // Convert the grasp_candidates into a format moveit_visual_tools can use
  std::vector<moveit_msgs::Grasp> grasps;
  for (std::size_t i = 0; i < grasp_candidates.size(); ++i)
  {
    grasps.push_back(grasp_candidates[i]->grasp_);
  }

  return visual_tools_->publishAnimatedGrasps(grasps, ee_jmg, show_prefiltered_grasps_speed_);
}

} // namespace
