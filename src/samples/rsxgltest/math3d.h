#ifndef rsxgltest_math_H
#define rsxgltest_math_H

#include <Eigen/Geometry>

Eigen::Projective3f frustum(float,float,float,float,float,float);
Eigen::Projective3f ortho(float,float,float,float,float,float);
Eigen::Projective3f perspective(float,float,float,float);
Eigen::Projective3f pick(float *,float *);

#endif
