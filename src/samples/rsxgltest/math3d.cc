#include "math3d.h"

Eigen::Projective3f
frustum(float left,float right,
	float bottom,float top,
	float near,float far)
{
  Eigen::Projective3f r(Eigen::Projective3f::Identity());

  float near_times_2 = 2.0f * near;
  float right_minus_left = right - left,
    top_minus_bottom = top - bottom,
    far_minus_near = far - near;

  float x = near_times_2 / right_minus_left,
    y = near_times_2 / top_minus_bottom,
    a = (right + left) / right_minus_left,
    b = (top + bottom) / top_minus_bottom,
    c = -(far + near) / far_minus_near,
    d = -(2.0f * far * near) / far_minus_near;

  r(0,0) = x;
  r(0,2) = a;
  r(1,1) = y;
  r(1,2) = b;
  r(2,2) = c;
  r(2,3) = d;
  r(3,2) = -1.0f;
  r(3,3) = 0;

  return r;
}

Eigen::Projective3f
ortho(float left,float right,
      float bottom,float top,
      float near,float far)
{
  Eigen::Projective3f r;
  
  const float
    right_minus_left = right - left,
    top_minus_bottom = top - bottom,
    far_minus_near = far - near;

  r(0,0) = 2.0f / right_minus_left;
  r(0,1) = 0;
  r(0,2) = 0;
  r(0,3) = -(right + left) / right_minus_left;

  r(1,0) = 0;
  r(1,1) = 2.0 / top_minus_bottom;
  r(1,2) = 0.0f;
  r(1,3) = -(top + bottom) / top_minus_bottom;

  r(2,0) = 0;
  r(2,1) = 0;
  r(2,2) = -2.0f / far_minus_near;
  r(2,3) = -(far + near) / far_minus_near;

  r(3,0) = 0;
  r(3,1) = 0;
  r(3,2) = 0;
  r(3,3) = 1.0f;

  return r;
}

Eigen::Projective3f
perspective(float fovy,
	    float aspect,
	    float near,float far)
{
  float xmin, xmax, ymin, ymax;

  ymax = near * tan(fovy);
  ymin = -ymax;
  xmin = ymin * aspect;
  xmax = ymax * aspect;

  return frustum(xmin, xmax, ymin, ymax, near, far);
}

Eigen::Projective3f
pick(float * viewport,
     float * region)
{
  Eigen::Projective3f r(Eigen::Projective3f::Identity());  

  float
    & ox = viewport[0],
    & oy = viewport[1],
    & px = viewport[2],
    & py = viewport[3],

    & qx = region[0],
    & qy = region[1],
    & dx = region[2],
    & dy = region[3];

  r(0,0) = px/dx;
  r(1,1) = py/dy;
  r(3,0) = px - (2 * (qx - ox) / dx);
  r(3,1) = py - (2 * (qy - oy) / dy);

  return r;
}

Eigen::Affine3f
lookat(const Eigen::Vector3f & eye,const Eigen::Vector3f & center,const Eigen::Vector3f & up)
{
  const Eigen::Vector3f tmp = center - eye;
  const Eigen::Vector3f forward = tmp / tmp.norm();
  const Eigen::Vector3f side = forward.cross(up);

  Eigen::Affine3f m = Eigen::Affine3f::Identity();

  m(0,0) = side[0];
  m(1,0) = side[1];
  m(2,0) = side[2];

  m(0,1) = up[0];
  m(1,1) = up[1];
  m(2,1) = up[2];

  m(0,2) = -forward[0];
  m(1,2) = -forward[1];
  m(2,2) = -forward[2];

  return m * Eigen::Translation3f(-eye[0],-eye[1],-eye[2]);
}
