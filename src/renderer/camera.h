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

    lml::Vec3 up() const { return m_transform.rotation() * lml::Vec3(0, 1, 0); }
    lml::Vec3 right() const { return m_transform.rotation() * lml::Vec3(1, 0, 0); }
    lml::Vec3 backward() const { return m_transform.rotation() * lml::Vec3(0, 0, 1); }

    lml::Vec3 forward() const { return m_transform.rotation() * lml::Vec3(0, 0, -1); }

    float fov() const { return m_fov; }
    float aspect() const { return m_aspect; }

    float exposure() const { return m_exposure; }

    lml::Matrix4f proj() const;
    lml::Matrix4f view() const;
    lml::Matrix4f viewproj() const;

    lml::Frustum frustum() const;
    lml::Frustum frustum(float znear, float zfar) const;

  public:

    void set_projection(float fov, float aspect, float znear = 0.1f, float zfar = 24000.0f);

    void set_exposure(float exposure);
    void set_exposure(float aperture, float shutterspeed, float iso);

  public:

    // world space

    void set_position(lml::Vec3 const &position);
    void set_rotation(lml::Quaternion3f const &rotation);

    void move(lml::Vec3 const &translation);

    void yaw(float angle, lml::Vec3 const &up);

    void lookat(lml::Vec3 const &target, lml::Vec3 const &up);
    void lookat(lml::Vec3 const &position, lml::Vec3 const &target, lml::Vec3 const &up);

    // camera space

    void offset(lml::Vec3 const &translation);
    void rotate(lml::Quaternion3f const &rotation);

    void roll(float angle);
    void pitch(float angle);
    void yaw(float angle);

    void dolly(lml::Vec3 const &target, float amount);
    void orbit(lml::Vec3 const &target, lml::Quaternion3f const &rotation);

  protected:

    friend Camera normalise(Camera const &camera);

  private:

    float m_fov;
    float m_aspect;
    float m_znear, m_zfar;

    float m_exposure;

    lml::Transform m_transform;
};


//////////////////////// Camera stream << ///////////////////////////////////
inline std::ostream &operator <<(std::ostream &os, Camera const &camera)
{
  os << "[Camera:" << camera.position() << "," << camera.rotation() << "]";

  return os;
}


//////////////////////// normalise //////////////////////////////////////////
/// normalise camera
inline Camera normalise(Camera const &camera)
{
  Camera result = camera;

  result.m_transform = normalise(camera.m_transform);

  return result;
}


//////////////////////// adapt //////////////////////////////////////////////
/// adapt camera exposure
inline Camera adapt(Camera const &camera, float currentluminance, float targetluminance, float rate)
{
  Camera result = camera;

  result.set_exposure(camera.exposure() * lml::lerp(1.0f, targetluminance/(currentluminance + 1e-3f), rate));

  return result;
}
