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
    Pose(Pose &&that) noexcept;
    Pose &operator =(Pose &&that) noexcept;
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
    int transformcount;

    Joint const *joints;
    Transform const *transforms;

  public:

    enum class State
    {
      Empty,
      Loading,
      Ready,
    };

    Asset const *asset;

    std::atomic<State> state;

  protected:
    Animation() = default;
};


//|---------------------- Animator ------------------------------------------
//|--------------------------------------------------------------------------

class Animator
{
  public:

    using allocator_type = StackAllocatorWithFreelist<>;

    Animator(allocator_type const &allocator);

  public:

    Pose pose;

    void set_mesh(Mesh const *mesh);

    void play(Animation const *animation, lml::Vec3 const &scale = lml::Vec3(1.0f), float rate = 1.0f, bool looping = true);

    void set_time(size_t channel, float time);
    void set_rate(size_t channel, float rate);
    void set_weight(size_t channel, float weight, float maxdelta = 1.0f);

    bool prepare();

    void update(float dt);

  private:

    allocator_type m_allocator;

  private:

    Mesh const *m_mesh;

  private:

    struct Joint
    {
      char name[32];

      lml::Transform transform;

      int parent;
      int bone;
    };

    std::vector<Joint, StackAllocatorWithFreelist<Joint>> m_joints;

    std::vector<size_t, StackAllocatorWithFreelist<size_t>> m_jointmap;

    struct Channel
    {
      Animation const *animation;

      lml::Vec3 scale;

      float time;
      float rate;

      float weight;

      bool looping;

      int jointmapbase;
      int jointmapcount;
    };

    std::vector<Channel, StackAllocatorWithFreelist<Channel>> m_channels;
};
