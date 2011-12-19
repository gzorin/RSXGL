#ifndef rsxgltest_math_H
#define rsxgltest_math_H

#include <Eigen/Geometry>

typedef Eigen::Transform< float, 3, Eigen::Affine > Transform3f;

Eigen::Projective3f frustum(float,float,float,float,float,float);
Eigen::Projective3f ortho(float,float,float,float,float,float);
Eigen::Projective3f perspective(float,float,float,float);
Eigen::Projective3f pick(float *,float *);
Eigen::Affine3f lookat(const Eigen::Vector3f &,const Eigen::Vector3f &,const Eigen::Vector3f &);

#endif
