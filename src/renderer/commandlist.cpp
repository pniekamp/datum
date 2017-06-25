//
// Datum - command list
//

//
// Copyright (c) 2016 Peter Niekamp
//

#include "commandlist.h"
#include "renderer.h"
#include "resource.h"
#include "debug.h"

using namespace std;
using namespace Vulkan;
using leap::alignto;
using leap::extentof;

//|---------------------- CommandList ---------------------------------------
//|--------------------------------------------------------------------------

///////////////////////// CommandList::Constructor //////////////////////////
CommandList::CommandList(RenderContext *context)
  : context(context)
{
  m_resourcelump = context->resourcepool.acquire_lump();
}


///////////////////////// CommandList::Destructor ///////////////////////////
CommandList::~CommandList()
{
  if (m_resourcelump)
  {
    context->resourcepool.release_lump(m_resourcelump);
  }
}


///////////////////////// CommandList::allocate_commandbuffer ///////////////
VkCommandBuffer CommandList::allocate_commandbuffer()
{
  assert(context);

  VkCommandBuffer commandbuffer = VK_NULL_HANDLE;

  if (m_resourcelump)
  {
    commandbuffer = context->resourcepool.acquire_commandbuffer(m_resourcelump);
  }

  return commandbuffer;
}


///////////////////////// CommandList::allocate_descriptorset ///////////////
VkDescriptorSet CommandList::allocate_descriptorset(VkDescriptorSetLayout layout)
{
  assert(context);

  VkDescriptorSet descriptorset = VK_NULL_HANDLE;

  if (m_resourcelump)
  {
    descriptorset = context->resourcepool.acquire_descriptorset(m_resourcelump, layout);
  }

  return descriptorset;
}


///////////////////////// CommandList::acquire_descriptor ///////////////////
CommandList::Descriptor CommandList::acquire_descriptor(VkDeviceSize required, Descriptor &&oldset)
{
  assert(context);
  assert(required != 0);

  Descriptor descriptor = {};

  if (m_resourcelump)
  {
    swap(descriptor.m_used, oldset.m_used);
    swap(descriptor.m_storage, oldset.m_storage);

    if (descriptor.available() < required)
    {
      if (descriptor.m_storage)
      {
        context->resourcepool.release_storagebuffer(descriptor.m_storage, descriptor.m_used);
      }

      descriptor.m_used = 0;
      descriptor.m_storage = context->resourcepool.acquire_storagebuffer(m_resourcelump, required);
    }
  }

  descriptor.m_pool = &context->resourcepool;

  return descriptor;
}


///////////////////////// CommandList::acquire_descriptor ///////////////////
CommandList::Descriptor CommandList::acquire_descriptor(VkDescriptorSetLayout layout, VkDeviceSize required, Descriptor &&oldset)
{
  assert(context);

  Descriptor descriptor = acquire_descriptor(required, std::move(oldset));

  if (descriptor.m_storage)
  {
    descriptor.m_descriptor = context->resourcepool.acquire_descriptorset(m_resourcelump, layout);

    bind_buffer(context->vulkan, descriptor.m_descriptor, 0, descriptor.m_storage, descriptor.m_storage.offset, descriptor.m_storage.capacity, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC);
  }

  return descriptor;
}


//|---------------------- CommandList Resource ------------------------------
//|--------------------------------------------------------------------------

///////////////////////// ResourceManager::allocate //////////////////////////
template<>
CommandList *ResourceManager::allocate<CommandList>(RenderContext *context)
{
  auto slot = acquire_slot(sizeof(CommandList));

  if (!slot)
    return nullptr;

  return new(slot) CommandList(context);
}


///////////////////////// ResourceManager::release //////////////////////////
template<>
void ResourceManager::release<CommandList>(CommandList const *commandlist)
{
  defer_destroy(commandlist);
}


///////////////////////// ResourceManager::destroy //////////////////////////
template<>
void ResourceManager::destroy<CommandList>(CommandList const *commandlist)
{
  if (commandlist)
  {
    commandlist->~CommandList();

    release_slot(const_cast<CommandList*>(commandlist), sizeof(CommandList));
  }
}
