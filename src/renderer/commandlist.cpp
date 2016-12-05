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
  m_commandbuffer = 0;

  memset(m_addressmap, 0, sizeof(m_addressmap));
}


///////////////////////// CommandList::Destructor ///////////////////////////
CommandList::~CommandList()
{
  if (m_resourcelump)
  {
    context->resourcepool.release_lump(m_resourcelump);
  }
}


///////////////////////// CommandList::begin //////////////////////////////
bool CommandList::begin(VkFramebuffer framebuffer, VkRenderPass renderpass, uint32_t subpass)
{
  assert(context);

  if (m_resourcelump)
  {
    m_commandbuffer = context->resourcepool.acquire_commandbuffer(m_resourcelump).commandbuffer;

    if (m_commandbuffer)
    {
      VkCommandBufferInheritanceInfo inheritanceinfo = {};
      inheritanceinfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
      inheritanceinfo.framebuffer = framebuffer;
      inheritanceinfo.renderPass = renderpass;
      inheritanceinfo.subpass = subpass;

      Vulkan::begin(context->device, m_commandbuffer, inheritanceinfo, VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT | VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT);
    }
  }

  return m_commandbuffer;
}


///////////////////////// CommandList::end ////////////////////////////////
void CommandList::end()
{
  assert(m_commandbuffer);

  Vulkan::end(context->device, m_commandbuffer);
}


///////////////////////// CommandList::acquire //////////////////////////////
CommandList::Descriptor CommandList::acquire(uint32_t set, VkDescriptorSetLayout layout, VkDeviceSize size, Descriptor const &oldset)
{
  assert(set < extentof(m_addressmap));

  Descriptor descriptor = {};

  if (m_resourcelump)
  {
    descriptor.m_used = oldset.m_used;
    descriptor.m_storage = oldset.m_storage;

    if (descriptor.capacity() < descriptor.used() + size)
    {
      release(oldset);

      descriptor.m_used = 0;
      descriptor.m_storage = context->resourcepool.acquire_storagebuffer(m_resourcelump, size);
    }

    if (descriptor.m_storage.buffer)
    {
      descriptor.m_descriptor = context->resourcepool.acquire_descriptorset(m_resourcelump, layout, descriptor.m_storage.buffer, descriptor.m_storage.offset, descriptor.m_storage.capacity);
    }

    m_addressmap[set] = descriptor.m_storage.memory;
  }

  return descriptor;
}


///////////////////////// CommandList::release //////////////////////////////
void CommandList::release(Descriptor const &descriptor)
{
  if (descriptor.m_storage.buffer)
  {
    context->resourcepool.release_storagebuffer(descriptor.m_storage, descriptor.m_used);
  }
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
  assert(commandlist);

  defer_destroy(commandlist);
}


///////////////////////// ResourceManager::destroy //////////////////////////
template<>
void ResourceManager::destroy<CommandList>(CommandList const *commandlist)
{
  assert(commandlist);

  commandlist->~CommandList();

  release_slot(const_cast<CommandList*>(commandlist), sizeof(CommandList));
}
