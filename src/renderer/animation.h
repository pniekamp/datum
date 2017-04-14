//
// Datum - animation
//

//
// Copyright (c) 2017 Peter Niekamp
//

#pragma once

#include "resource.h"
#include "mesh.h"

//|---------------------- Pose ----------------------------------------------
//|--------------------------------------------------------------------------

class Pose
{
  public:

    Pose();
    Pose(int bonecount, StackAllocator<> const &allocator);
    Pose(int bonecount, StackAllocatorWithFreelist<> const &allocator);
    Pose(Pose const &) = delete;
    Pose &operator =(Pose const &) = delete;
    Pose(Pose &&that);
    Pose &operator =(Pose &&that);
    ~Pose();

    friend void swap(Pose &a, Pose &b);

  public:

    int bonecount;
    lml::Transform *bones;

  private:

    FreeList *m_freelist;
};


//|---------------------- Animation -----------------------------------------
//|--------------------------------------------------------------------------

class Animation
{
  public:

    struct Joint
    {
      char name[32];
      uint32_t parent;
      uint32_t index;
      uint32_t count;
    };

    struct Transform
    {
      float time;
      lml::Transform transform;
    };

  public:
    friend Animation const *ResourceManager::create<Animation>(Asset const *asset);

    bool ready() const { return (state == State::Ready); }

    float duration;

    int jointcount;
    Joint const *joints;

    int transformcount;
    Transform const *transforms;

  public:

    enum class State
    {
      Empty,
      Loading,
      Waiting,
      Testing,
      Ready,
    };

    Asset const *asset;

    std::atomic<State> state;

    alignas(16) uint8_t data[1];

  protected:
    Animation() = default;
};


//|---------------------- Animator ------------------------------------------
//|--------------------------------------------------------------------------

class Animator
{
  public:

    typedef StackAllocatorWithFreelist<> allocator_type;

    Animator(allocator_type const &allocator);

  public:

    Pose pose;

    void set_mesh(Mesh const *mesh);

    void play_animation(Animation const *animation, lml::Vec3 const &scale = lml::Vec3(1.0f));

    void update(float dt);

  private:

    allocator_type m_allocator;

  private:

    Mesh const *m_mesh;
    Animation const *m_animation;

  private:

    struct Joint
    {
      int bone;
      lml::Transform transform;

      int parent;
    };

    std::vector<Joint, StackAllocatorWithFreelist<Joint>> m_jointmap;

    lml::Vec3 m_scale;

    float m_time;
};
