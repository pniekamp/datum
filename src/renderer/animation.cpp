//
// Datum - animation
//

//
// Copyright (c) 2017 Peter Niekamp
//

#include "animation.h"
#include "resource.h"
#include "assetpack.h"
#include "debug.h"

using namespace std;
using namespace lml;
using leap::alignto;
using leap::indexof;

namespace
{
  size_t anim_datasize(int jointcount, int transformcount)
  {
    return jointcount * sizeof(Animation::Joint) + transformcount * sizeof(Animation::Transform);
  }
}

//|---------------------- Pose ----------------------------------------------
//|--------------------------------------------------------------------------

///////////////////////// Pose::Constructor /////////////////////////////////
Pose::Pose()
{
  bonecount = 0;
  bones = nullptr;
  m_freelist = nullptr;
}


///////////////////////// Pose::Constructor /////////////////////////////////
Pose::Pose(int bonecount, StackAllocator<> const &allocator)
  : bonecount(bonecount)
{
  bones = allocate<Transform>(allocator, bonecount);

  m_freelist = nullptr;
}


///////////////////////// Pose::Constructor /////////////////////////////////
Pose::Pose(int bonecount, StackAllocatorWithFreelist<> const &allocator)
  : bonecount(bonecount)
{
  bones = allocate<Transform>(allocator, bonecount);

  m_freelist = &allocator.freelist();
}


///////////////////////// Pose::Constructor /////////////////////////////////
Pose::Pose(Pose &&that)
  : Pose()
{
  swap(*this, that);
}


///////////////////////// Pose::Assignment //////////////////////////////////
Pose &Pose::operator =(Pose &&that)
{
  swap(*this, that);

  return *this;
}


///////////////////////// Pose::swap ////////////////////////////////////////
void swap(Pose &a, Pose &b)
{
  swap(a.bonecount, b.bonecount);
  swap(a.bones, b.bones);
  swap(a.m_freelist, b.m_freelist);
}


///////////////////////// Pose::Destructor //////////////////////////////////
Pose::~Pose()
{
  if (m_freelist)
  {
    m_freelist->release(bones, bonecount * sizeof(Transform));
  }
}



//|---------------------- Animation -----------------------------------------
//|--------------------------------------------------------------------------

///////////////////////// ResourceManager::create ///////////////////////////
template<>
Animation const *ResourceManager::create<Animation>(Asset const *asset)
{
  if (!asset)
    return nullptr;

  auto slot = acquire_slot(sizeof(Animation) + anim_datasize(asset->jointcount, asset->transformcount));

  if (!slot)
    return nullptr;

  auto anim = new(slot) Animation;

  anim->duration = asset->duration;
  anim->jointcount = asset->jointcount;
  anim->joints = nullptr;
  anim->transformcount = asset->transformcount;
  anim->transforms = nullptr;
  anim->asset = asset;
  anim->state = Animation::State::Empty;

  return anim;
}


///////////////////////// ResourceManager::request //////////////////////////
template<>
void ResourceManager::request<Animation>(DatumPlatform::PlatformInterface &platform, Animation const *anim)
{
  assert(anim);

  auto slot = const_cast<Animation*>(anim);

  Animation::State empty = Animation::State::Empty;

  if (slot->state.compare_exchange_strong(empty, Animation::State::Loading))
  {
    if (auto asset = slot->asset)
    {
      assert(assets()->barriercount != 0);

      if (auto bits = m_assets->request(platform, asset))
      {
        auto payload = reinterpret_cast<PackAnimationPayload const *>(bits);

        auto jointtable = PackAnimationPayload::jointtable(payload, asset->jointcount, asset->transformcount);

        auto jointdata = reinterpret_cast<Animation::Joint*>(slot->data);

        for(int i = 0; i < slot->jointcount; ++i)
        {
          memcpy(jointdata[i].name, jointtable[i].name, sizeof(jointdata[i].name));
          jointdata[i].parent = jointtable[i].parent;
          jointdata[i].index = jointtable[i].index;
          jointdata[i].count = jointtable[i].count;
        }

        slot->joints = jointdata;

        auto transformtable = PackAnimationPayload::transformtable(payload, asset->jointcount, asset->transformcount);

        auto transformdata = reinterpret_cast<Animation::Transform*>(slot->data + slot->jointcount*sizeof(Animation::Joint));

        for(int i = 0; i < slot->transformcount; ++i)
        {
          transformdata[i].time = transformtable[i].time;
          transformdata[i].transform = Transform{ { transformtable[i].transform[0], transformtable[i].transform[1], transformtable[i].transform[2], transformtable[i].transform[3] }, { transformtable[i].transform[4], transformtable[i].transform[5], transformtable[i].transform[6], transformtable[i].transform[7] } };
        }

        slot->transforms = transformdata;
      }
    }

    slot->state = (slot->joints) ? Animation::State::Ready : Animation::State::Empty;
  }
}


///////////////////////// ResourceManager::release //////////////////////////
template<>
void ResourceManager::release<Animation>(Animation const *anim)
{
  defer_destroy(anim);
}


///////////////////////// ResourceManager::destroy //////////////////////////
template<>
void ResourceManager::destroy<Animation>(Animation const *anim)
{
  if (anim)
  {
    anim->~Animation();

    release_slot(const_cast<Animation*>(anim), sizeof(Animation) + anim_datasize(anim->jointcount, anim->transformcount));
  }
}


//|---------------------- Animator ------------------------------------------
//|--------------------------------------------------------------------------

///////////////////////// Animator::Constructor /////////////////////////////
Animator::Animator(allocator_type const &allocator)
  : m_allocator(allocator),
    m_jointmap(allocator)
{
  m_mesh = nullptr;
  m_animation = nullptr;

  m_time = 0;
}


///////////////////////// Animator::set_mesh ////////////////////////////////
void Animator::set_mesh(Mesh const *mesh)
{
  pose = Pose(mesh->bonecount, m_allocator);

  for(int i = 0; i < pose.bonecount; ++i)
    pose.bones[i] = Transform::identity();

  m_mesh = mesh;
}


///////////////////////// Animator::play_animation //////////////////////////
void Animator::play_animation(Animation const *animation, Vec3 const &scale)
{
  m_jointmap.clear();
  m_jointmap.reserve(animation->jointcount);

  m_animation = animation;

  m_time = 0;
  m_scale = scale;
}


///////////////////////// Animator::update //////////////////////////////////
void Animator::update(float dt)
{
  assert(m_mesh && m_mesh->ready());
  assert(m_animation && m_animation->ready());

  m_time += dt;

  float time = fmod(m_time, m_animation->duration);

  if (m_jointmap.empty())
  {   
    m_jointmap.resize(m_animation->jointcount);

    for(int i = 0; i < m_animation->jointcount; ++i)
    {
      auto j = find_if(m_mesh->bones, m_mesh->bones + m_mesh->bonecount, [&](auto &bone) { return strcmp(bone.name, m_animation->joints[i].name) == 0; });

      m_jointmap[i].bone = indexof(m_mesh->bones, j);
      m_jointmap[i].transform = Transform::identity();
      m_jointmap[i].parent = m_animation->joints[i].parent;
    }
  }

  for(size_t i = 1; i < m_jointmap.size(); ++i)
  {
    auto &joint = m_jointmap[i];

    size_t index = m_animation->joints[i].index;

    while (index+2 < m_animation->joints[i].index + m_animation->joints[i].count && m_animation->transforms[index+1].time < time)
      ++index;

    auto alpha = remap(time, m_animation->transforms[index].time, m_animation->transforms[index+1].time, 0.0f, 1.0f);

    auto transform = lerp(m_animation->transforms[index].transform, m_animation->transforms[index+1].transform, alpha);

    joint.transform = m_jointmap[joint.parent].transform * Transform::translation(hada(m_scale, transform.translation())) * Transform::rotation(transform.rotation());

    if (joint.bone < pose.bonecount)
    {
      pose.bones[joint.bone] = joint.transform * m_mesh->bones[joint.bone].transform;
    }
  }
}
