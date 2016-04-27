//
// Datum - camera
//

//
// Copyright (c) 2015 Peter Niekamp
//

#pragma once

#include "datum/math.h"


//|-------------------- Camera --------------------------------------------
//|------------------------------------------------------------------------

class Camera
{
  public:
    Camera();

    lml::Vec3 position() const { return m_transform.translation(); }

    lml::Quaternion3f rotation() const { return m_transform.rotation(); }

    lml::Vec3 up() const { return m_transform.rotation() * lml::Vec3(0.0f, 1.0f, 0.0f); }
    lml::Vec3 right() const { return m_transform.rotation() * lml::Vec3(1.0f, 0.0f, 0.0f); }
    lml::Vec3 backward() const { return m_transform.rotation() * lml::Vec3(0.0f, 0.0f, 1.0f); }

    lml::Vec3 forward() const { return m_transform.rotation() * lml::Vec3(0.0f, 0.0f, -1.0f); }

    float fov() const { return m_fov; }
    float aspect() const { return m_aspect; }

    lml::Matrix4f proj() const;
    lml::Matrix4f view() const;
    lml::Matrix4f viewproj() const;

    lml::Frustum frustum() const;
    lml::Frustum frustum(float znear, float zfar) const;

  public:

    void set_projection(float fov, float aspect, float znear = 0.001f, float zfar = 24000.0f);

  public:

    // world space

    void set_position(lml::Vec3 const &position);
    void set_rotation(lml::Quaternion3f const &rotation);

    void move(lml::Vec3 const &translation);

    void yaw(float angle, lml::Vec3 const &up);

    void lookat(lml::Vec3 const &target, lml::Vec3 const &up);

    // camera space

    void offset(lml::Vec3 const &translation);
    void rotate(lml::Quaternion3f const &rotation);

    void roll(float angle);
    void pitch(float angle);
    void yaw(float angle);

  protected:

    friend Camera normalise(Camera const &camera);

  private:

    float m_fov;
    float m_aspect;
    float m_znear, m_zfar;

    lml::Transform m_transform;
};


//////////////////////// Camera stream << ///////////////////////////////////
inline std::ostream &operator <<(std::ostream &os, Camera const &camera)
{
  os << "[Camera:" << camera.position() << "," << camera.rotation() << "]";

  return os;
}


//////////////////////// normalise ////////////////////////////////////////
/// normalise camera
inline Camera normalise(Camera const &camera)
{
  Camera result = camera;

  result.m_transform = normalise(camera.m_transform);

  return result;
}
