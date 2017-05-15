//
// Datum - animation
//

//
// Copyright (c) 2017 Peter Niekamp
//

#include "animation.h"
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

  Transform blend(Transform const &t1, Transform const &t2, float weight)
  {
    auto flip = std::copysign(1.0f, dot(t1.real, t2.real));

    return { t1.real + weight * flip * t2.real, t1.dual + weight * flip * t2.dual };
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
    m_joints(allocator),
    m_jointmap(allocator),
    m_entries(allocator)
{
  m_mesh = nullptr;
}


///////////////////////// Animator::set_mesh ////////////////////////////////
void Animator::set_mesh(Mesh const *mesh)
{
  pose = Pose(mesh->bonecount, m_allocator);

  for(int i = 0; i < pose.bonecount; ++i)
    pose.bones[i] = Transform::identity();

  for(auto &entry : m_entries)
  {
    entry.jointmapbase = 0;
    entry.jointmapcount = 0;
  }

  m_joints.clear();
  m_jointmap.clear();

  m_mesh = mesh;
}


///////////////////////// Animator::play ////////////////////////////////////
void Animator::play(Animation const *animation, Vec3 const &scale, float rate, bool looping)
{
  Entry entry = {};
  entry.animation = animation;
  entry.scale = scale;
  entry.time = 0.0f;
  entry.rate = rate;
  entry.weight = 1.0f;
  entry.looping = looping;

  m_entries.push_back(entry);
}


///////////////////////// Animator::set_time ////////////////////////////////
void Animator::set_time(size_t entry, float time)
{
  assert(entry < m_entries.size());

  m_entries[entry].time = time;
}


///////////////////////// Animator::set_rate ////////////////////////////////
void Animator::set_rate(size_t entry, float rate)
{
  assert(entry < m_entries.size());

  m_entries[entry].rate = rate;
}


///////////////////////// Animator::set_weight //////////////////////////////
void Animator::set_weight(size_t entry, float weight, float maxdelta)
{
  assert(entry < m_entries.size());

  m_entries[entry].weight += clamp(-maxdelta, weight - m_entries[entry].weight, maxdelta);
}


///////////////////////// Animator::update //////////////////////////////////
void Animator::update(float dt)
{
  bool active = false;

  for(auto &entry : m_entries)
  {
    auto &animation = entry.animation;

    if (entry.jointmapcount != animation->jointcount)
    {
      assert(m_mesh && m_mesh->ready());
      assert(animation && animation->ready());

      entry.jointmapbase = m_jointmap.size();

      m_jointmap.resize(m_jointmap.size() + animation->jointcount);

      for(int i = 0; i < animation->jointcount; ++i)
      {
        auto j = find_if(m_joints.begin(), m_joints.end(), [&](auto &joint) { return strcmp(joint.name, animation->joints[i].name) == 0; });

        if (j == m_joints.end())
        {
          Joint joint = {};

          memcpy(joint.name, animation->joints[i].name, sizeof(joint.name));

          joint.parent = indexof(m_joints, find_if(m_joints.begin(), m_joints.end(), [&](auto &joint) { return strcmp(joint.name, animation->joints[animation->joints[i].parent].name) == 0; }));
          joint.bone = indexof(m_mesh->bones, find_if(m_mesh->bones, m_mesh->bones + m_mesh->bonecount, [&](auto &bone) { return strcmp(bone.name, animation->joints[i].name) == 0; }));

          joint.transform = Transform::identity();

          j = m_joints.insert(m_joints.end(), joint);
        }

        m_jointmap[entry.jointmapbase + i] = indexof(m_joints, j);
      }

      entry.jointmapcount = animation->jointcount;
    }

    if (entry.rate != 0.0f)
    {
      entry.time += entry.rate * dt;

      if (entry.looping)
      {
        entry.time = fmod2(entry.time, animation->duration);
      }
      else
      {
        if (entry.time <= 0.0f || entry.time >= animation->duration)
        {
          entry.rate = 0.0f;
          entry.time = clamp(entry.time, 0.0f, animation->duration);
        }
      }

      active = true;
    }
  }

  if (active)
  {
    for(auto &joint : m_joints)
    {
      joint.transform = {};
    }

    for(auto &entry : m_entries)
    {
      auto &animation = entry.animation;

      if (entry.weight != 0)
      {
        for(int i = 0; i < animation->jointcount; ++i)
        {
          auto &joint = m_joints[m_jointmap[entry.jointmapbase + i]];

          size_t index = animation->joints[i].index;

          while (index+2 < animation->joints[i].index + animation->joints[i].count && animation->transforms[index+1].time < entry.time)
            ++index;

          auto alpha = remap(entry.time, animation->transforms[index].time, animation->transforms[index+1].time, 0.0f, 1.0f);

          auto transform = lerp(animation->transforms[index].transform, animation->transforms[index+1].transform, alpha);

          joint.transform = blend(joint.transform, Transform::translation(hada(entry.scale, transform.translation())) * Transform::rotation(transform.rotation()), entry.weight);
        }
      }
    }

    for(auto &joint : m_joints)
    {
      joint.transform = m_joints[joint.parent].transform * normalise(joint.transform);

      if (joint.bone < pose.bonecount)
      {
        pose.bones[joint.bone] = joint.transform * m_mesh->bones[joint.bone].transform;
      }
    }
  }
}
