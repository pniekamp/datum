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
  m_fov = 60.0f;
  m_aspect = 1.7777;
  m_exposure = 1.0f;
  m_transform = Transform::identity();
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
  m_exposure = exposure;
}


///////////////////////// Camera::set_exposure //////////////////////////////
void Camera::set_exposure(float aperture, float shutterspeed, float iso)
{
  float q = 0.65;
  float l_avg = (1.0 / q) * sqrt(aperture) / (iso * shutterspeed);

  m_exposure = 0.18 / l_avg;
}


///////////////////////// Camera::view //////////////////////////////////////
Matrix4f Camera::view() const
{
  return inverse(m_transform).matrix();
}


///////////////////////// Camera::proj //////////////////////////////////////
Matrix4f Camera::proj() const
{
  return PerspectiveProjection(m_fov, m_aspect, m_znear, m_zfar) * ScaleMatrix(Vector4(1.0f, -1.0f, 1.0f, 1.0f));
}


///////////////////////// Camera::viewproj //////////////////////////////////
Matrix4f Camera::viewproj() const
{
  return proj() * view();
}


///////////////////////// Camera::frustum ///////////////////////////////////
Frustum Camera::frustum() const
{
  return m_transform * Frustum::perspective(m_fov, m_aspect, m_znear, m_zfar);
}


///////////////////////// Camera::frustum ///////////////////////////////////
Frustum Camera::frustum(float znear, float zfar) const
{
  return m_transform * Frustum::perspective(m_fov, m_aspect, znear, zfar);
}


///////////////////////// Camera::set_position //////////////////////////////
void Camera::set_position(Vec3 const &position)
{
  m_transform = Transform::translation(position) * Transform::rotation(m_transform.rotation());
}


///////////////////////// Camera::set_rotation //////////////////////////////
void Camera::set_rotation(Quaternion3f const &rotation)
{
  m_transform = Transform::translation(m_transform.translation()) * Transform::rotation(rotation);
}


///////////////////////// Camera::move //////////////////////////////////////
void Camera::move(Vec3 const &translation)
{
  m_transform = Transform::translation(translation) * m_transform;
}


///////////////////////// Camera::yaw ///////////////////////////////////////
void Camera::yaw(float angle, Vec3 const &up)
{
  m_transform = Transform::translation(m_transform.translation()) * Transform::rotation(up, angle) * Transform::rotation(m_transform.rotation());
}


///////////////////////// Camera::lookat ////////////////////////////////////
void Camera::lookat(Vec3 const &target, Vec3 const &up)
{
  m_transform = Transform::lookat(position(), target, up);
}


///////////////////////// Camera::offset ////////////////////////////////////
void Camera::offset(Vec3 const &translation)
{
  m_transform = m_transform * Transform::translation(translation);
}


///////////////////////// Camera::rotate ////////////////////////////////////
void Camera::rotate(Quaternion3f const &rotation)
{
  m_transform = m_transform * Transform::rotation(rotation);
}


///////////////////////// Camera::roll //////////////////////////////////////
void Camera::roll(float angle)
{
  m_transform = m_transform * Transform::rotation(Vec3(0.0f, 0.0f, 1.0f), angle);
}


///////////////////////// Camera::pitch /////////////////////////////////////
void Camera::pitch(float angle)
{
  m_transform = m_transform * Transform::rotation(Vec3(1.0f, 0.0f, 0.0f), angle);
}


///////////////////////// Camera::yaw ///////////////////////////////////////
void Camera::yaw(float angle)
{
  m_transform = m_transform * Transform::rotation(Vec3(0.0f, 1.0f, 0.0f), angle);
}
