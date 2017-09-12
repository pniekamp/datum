//
// Datum - command lump
//

//
// Copyright (c) 2016 Peter Niekamp
//

#include "commandlump.h"
#include "renderer.h"
#include "resource.h"
#include "debug.h"

using namespace std;
using namespace Vulkan;
using leap::alignto;
using leap::extentof;

//|---------------------- CommandLump ---------------------------------------
//|--------------------------------------------------------------------------

///////////////////////// CommandLump::Constructor //////////////////////////
CommandLump::CommandLump(RenderContext *context)
  : context(context)
{
  m_resourcelump = context->resourcepool.acquire_lump();
}


///////////////////////// CommandLump::Destructor ///////////////////////////
CommandLump::~CommandLump()
{
  if (m_resourcelump)
  {
    context->resourcepool.release_lump(m_resourcelump);
  }
}


///////////////////////// CommandLump::allocate_commandbuffer ///////////////
VkCommandBuffer CommandLump::allocate_commandbuffer()
{
  assert(context);

  VkCommandBuffer commandbuffer = VK_NULL_HANDLE;

  if (m_resourcelump)
  {
    commandbuffer = context->resourcepool.acquire_commandbuffer(m_resourcelump);
  }

  return commandbuffer;
}


///////////////////////// CommandLump::allocate_descriptorset ///////////////
VkDescriptorSet CommandLump::allocate_descriptorset(VkDescriptorSetLayout layout)
{
  assert(context);

  VkDescriptorSet descriptorset = VK_NULL_HANDLE;

  if (m_resourcelump)
  {
    descriptorset = context->resourcepool.acquire_descriptorset(m_resourcelump, layout);
  }

  return descriptorset;
}


///////////////////////// CommandLump::acquire_descriptor ///////////////////
CommandLump::Descriptor CommandLump::acquire_descriptor(VkDeviceSize required, Descriptor &&oldset)
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

CommandLump::Descriptor CommandLump::acquire_descriptor(VkDescriptorSetLayout layout, Descriptor &&oldset)
{
  assert(context);

  Descriptor descriptor = std::move(oldset);

  descriptor.m_descriptor = context->resourcepool.acquire_descriptorset(m_resourcelump, layout);

  return descriptor;
}

CommandLump::Descriptor CommandLump::acquire_descriptor(VkDescriptorSetLayout layout, VkDeviceSize required, Descriptor &&oldset)
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


//|---------------------- CommandLump Resource ------------------------------
//|--------------------------------------------------------------------------

///////////////////////// ResourceManager::allocate /////////////////////////
template<>
CommandLump *ResourceManager::allocate<CommandLump>(RenderContext *context)
{
  auto slot = acquire_slot(sizeof(CommandLump));

  if (!slot)
    return nullptr;

  return new(slot) CommandLump(context);
}


///////////////////////// ResourceManager::release //////////////////////////
template<>
void ResourceManager::release<CommandLump>(CommandLump const *commandlump)
{
  defer_destroy(commandlump);
}


///////////////////////// ResourceManager::destroy //////////////////////////
template<>
void ResourceManager::destroy<CommandLump>(CommandLump const *commandlump)
{
  if (commandlump)
  {
    commandlump->~CommandLump();

    release_slot(const_cast<CommandLump*>(commandlump), sizeof(CommandLump));
  }
}
