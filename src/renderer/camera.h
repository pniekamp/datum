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

    lml::Vec3 position() const { return m_position; }

    lml::Quaternion3 rotation() const { return m_rotation; }

    lml::Vec3 up() const { return m_rotation * lml::Vec3(0, 1, 0); }
    lml::Vec3 right() const { return m_rotation * lml::Vec3(1, 0, 0); }
    lml::Vec3 backward() const { return m_rotation * lml::Vec3(0, 0, 1); }

    lml::Vec3 forward() const { return m_rotation * lml::Vec3(0, 0, -1); }

    float fov() const { return m_fov; }
    float aspect() const { return m_aspect; }

    float zfar() const { return m_zfar; }
    float znear() const { return m_znear; }

    float exposure() const { return m_exposure; }
    float focalwidth() const { return m_focalwidth; }
    float focaldistance() const { return m_focaldistance; }

    lml::Matrix4f proj() const;
    lml::Matrix4f view() const;
    lml::Matrix4f viewproj() const;

    lml::Frustum frustum() const;
    lml::Frustum frustum(float znear, float zfar) const;

    lml::Transform transform() const { return lml::Transform::lookat(m_position, m_rotation); }

  public:

    void set_projection(float fov, float aspect, float znear = 0.1f, float zfar = 24000.0f);

    void set_exposure(float exposure);
    void set_exposure(float aperture, float shutterspeed, float iso);

    void set_depthoffield(float focalwidth, float focaldistance);

  public:

    // world space

    void set_position(lml::Vec3 const &position);
    void set_rotation(lml::Quaternion3 const &rotation);

    void move(lml::Vec3 const &translation);

    void yaw(float angle, lml::Vec3 const &up);

    void lookat(lml::Vec3 const &target, lml::Vec3 const &up);
    void lookat(lml::Vec3 const &position, lml::Vec3 const &target, lml::Vec3 const &up);

    // camera space

    void offset(lml::Vec3 const &translation);
    void rotate(lml::Quaternion3 const &rotation);

    void roll(float angle);
    void pitch(float angle);
    void yaw(float angle);

    void pan(lml::Vec3 &target, float dx, float dy);
    void dolly(lml::Vec3 const &target, float amount);
    void orbit(lml::Vec3 const &target, lml::Quaternion3 const &rotation);

  private:

    float m_fov;
    float m_aspect;
    float m_znear, m_zfar;

    float m_exposure;
    float m_focalwidth;
    float m_focaldistance;

    lml::Vec3 m_position;
    lml::Quaternion3 m_rotation;
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

  result.set_rotation(normalise(camera.rotation()));

  return result;
}


//////////////////////// adapt //////////////////////////////////////////////
/// adapt camera exposure
inline Camera adapt(Camera const &camera, float currentluminance, float targetluminance, float rate)
{
  Camera result = camera;

  result.set_exposure(lml::clamp(camera.exposure() * lml::lerp(1.0f, targetluminance/(currentluminance + 1e-3f), rate), 0.0f, 8.0f));

  return result;
}
