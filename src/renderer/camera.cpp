//
// Datum - camera
//

//
// Copyright (c) 2015 Peter Niekamp
//

#include "camera.h"
#include <leap/lml/matrixconstants.h>
#include "debug.h"

using namespace std;
using namespace lml;

//|---------------------- Camera --------------------------------------------
//|--------------------------------------------------------------------------

///////////////////////// Camera::Constructor ///////////////////////////////
Camera::Camera()
{
  m_fov = 60.0f*pi<float>()/180.0f;
  m_aspect = 1.7777f;
  m_znear = 0.1f;
  m_zfar = 1000.0f;
  m_exposure = 1.0f;
  m_focalwidth = 100000.0f;
  m_focaldistance = 0.0f;
  m_position = Vec3(0);
  m_rotation = Quaternion3(1, 0, 0, 0);
}


///////////////////////// Camera::set_projection ////////////////////////////
void Camera::set_projection(float fov, float aspect, float znear, float zfar)
{
  m_fov = fov;
  m_aspect = aspect;
  m_znear = znear;
  m_zfar = zfar;
}


///////////////////////// Camera::set_exposure //////////////////////////////
void Camera::set_exposure(float exposure)
{
  m_exposure = max(0.0f, exposure);
}


///////////////////////// Camera::set_exposure //////////////////////////////
void Camera::set_exposure(float aperture, float shutterspeed, float iso)
{
  float q = 0.65f;
  float l_avg = (1.0f / q) * sqrt(aperture) / (iso * shutterspeed);

  set_exposure(0.18f / l_avg);
}


///////////////////////// Camera::set_depthoffield //////////////////////////
void Camera::set_depthoffield(float focalwidth, float focaldistance)
{
  m_focalwidth = focalwidth;
  m_focaldistance = focaldistance;
}


///////////////////////// Camera::view //////////////////////////////////////
Matrix4f Camera::view() const
{
  return inverse(transform()).matrix();
}


///////////////////////// Camera::proj //////////////////////////////////////
Matrix4f Camera::proj() const
{
  Matrix4f proj = {};

  // Y Flipped
  // Reverse Z

  proj(0, 0) = 1 / (m_aspect * std::tan(m_fov/2));
  proj(1, 1) = -1 / std::tan(m_fov/2);
  proj(2, 2) = m_zfar / (m_zfar - m_znear) - 1;
  proj(3, 2) = -1;
  proj(2, 3) = m_zfar * m_znear / (m_zfar - m_znear);

  return proj;
}


///////////////////////// Camera::viewproj //////////////////////////////////
Matrix4f Camera::viewproj() const
{
  return proj() * view();
}


///////////////////////// Camera::frustum ///////////////////////////////////
Frustum Camera::frustum() const
{
  return transform() * Frustum::perspective(m_fov, m_aspect, m_znear, m_zfar);
}


///////////////////////// Camera::frustum ///////////////////////////////////
Frustum Camera::frustum(float znear, float zfar) const
{
  return transform() * Frustum::perspective(m_fov, m_aspect, znear, zfar);
}


///////////////////////// Camera::set_position //////////////////////////////
void Camera::set_position(Vec3 const &position)
{
  m_position = position;
}


///////////////////////// Camera::set_rotation //////////////////////////////
void Camera::set_rotation(Quaternion3 const &rotation)
{
  m_rotation = rotation;
}


///////////////////////// Camera::move //////////////////////////////////////
void Camera::move(Vec3 const &translation)
{
  m_position = m_position + translation;
}


///////////////////////// Camera::yaw ///////////////////////////////////////
void Camera::yaw(float angle, Vec3 const &up)
{
  m_rotation = Quaternion3(up, angle) * m_rotation;
}


///////////////////////// Camera::lookat ////////////////////////////////////
void Camera::lookat(Vec3 const &target, Vec3 const &up)
{
  m_rotation = Transform::lookat(m_position, target, up).rotation();
}


///////////////////////// Camera::lookat ////////////////////////////////////
void Camera::lookat(Vec3 const &position, Vec3 const &target, Vec3 const &up)
{
  m_position = position;
  m_rotation = Transform::lookat(position, target, up).rotation();
}


///////////////////////// Camera::offset ////////////////////////////////////
void Camera::offset(Vec3 const &translation)
{
  m_position = m_position + m_rotation * translation;
}


///////////////////////// Camera::rotate ////////////////////////////////////
void Camera::rotate(Quaternion3 const &rotation)
{
  m_rotation = m_rotation * rotation;
}


///////////////////////// Camera::roll //////////////////////////////////////
void Camera::roll(float angle)
{
  m_rotation = m_rotation * Quaternion3(Vec3(0, 0, 1), angle);
}


///////////////////////// Camera::pitch /////////////////////////////////////
void Camera::pitch(float angle)
{
  m_rotation = m_rotation * Quaternion3(Vec3(1, 0, 0), angle);
}


///////////////////////// Camera::yaw ///////////////////////////////////////
void Camera::yaw(float angle)
{
  m_rotation = m_rotation * Quaternion3(Vec3(0, 1, 0), angle);
}


///////////////////////// Camera::pan ///////////////////////////////////////
void Camera::pan(Vec3 &target, float dx, float dy)
{
  auto speed = clamp(0.1f * norm(position() - target), 0.1f, 10.0f);

  auto offset = speed * (dx * right() + dy * up());

  target += offset;

  lookat(position() + offset, target, up());
}


///////////////////////// Camera::dolly /////////////////////////////////////
void Camera::dolly(Vec3 const &target, float amount)
{
  auto speed = clamp(0.1f * norm(position() - target), 0.1f, 10.0f);

  lookat(position() + speed * amount * forward(), target, up());
}


///////////////////////// Camera::orbit /////////////////////////////////////
void Camera::orbit(Vec3 const &target, Quaternion3 const &rotation)
{
  auto speed = clamp(0.1f * norm(position() - target), 0.1f, 1.0f);

  auto angle = normalise(slerp(Quaternion3(1, 0, 0, 0), rotation, speed));

  lookat(target + angle * (position() - target), target, angle * up());
}
