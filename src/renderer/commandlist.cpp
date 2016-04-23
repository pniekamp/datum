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

extern size_t transfer_reservation(RenderContext &context, size_t required);


//|---------------------- CommandList ---------------------------------------
//|--------------------------------------------------------------------------

///////////////////////// CommandList::Constructor //////////////////////////
CommandList::CommandList()
  : context(nullptr)
{
  m_commandbuffer = 0;
  m_resourcelump = nullptr;
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
bool CommandList::begin(RenderContext &context, VkFramebuffer framebuffer, VkRenderPass renderpass, uint32_t subpass, size_t transferreservation)
{
  this->context = &context;

  m_resourcelump = context.resourcepool.aquire_lump();

  if (!m_resourcelump)
    return false;

  transferoffset = transfer_reservation(context, transferreservation);

  m_commandbuffer = context.resourcepool.acquire_commandbuffer(m_resourcelump).commandbuffer;

  VkCommandBufferInheritanceInfo inheritanceinfo = {};
  inheritanceinfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
  inheritanceinfo.framebuffer = framebuffer;
  inheritanceinfo.renderPass = renderpass;
  inheritanceinfo.subpass = subpass;

  Vulkan::begin(context.device, m_commandbuffer, inheritanceinfo, VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT | VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT);

  return true;
}


///////////////////////// CommandList::end ////////////////////////////////
void CommandList::end()
{
  assert(m_commandbuffer);

  Vulkan::end(context->device, m_commandbuffer);
}


///////////////////////// CommandList::acquire //////////////////////////////
CommandList::Descriptor CommandList::acquire(VkDescriptorSetLayout layout, VkDeviceSize size)
{
  assert(m_commandbuffer);

  Descriptor descriptor = {};

  descriptor.m_storage = context->resourcepool.acquire_storagebuffer(m_resourcelump, size);

  if (descriptor.m_storage.buffer)
  {
    descriptor.m_descriptor = context->resourcepool.acquire_descriptorset(m_resourcelump, layout, descriptor.m_storage.buffer, descriptor.m_storage.offset, descriptor.m_storage.capacity);
  }

  return descriptor;
}


///////////////////////// CommandList::release //////////////////////////////
void CommandList::release(Descriptor const &descriptor, VkDeviceSize used)
{
  if (descriptor.m_storage.buffer)
  {
    context->resourcepool.release_storagebuffer(descriptor.m_storage, used);
  }
}


//|---------------------- CommandList Resource ------------------------------
//|--------------------------------------------------------------------------

///////////////////////// ResourceManager::allocate //////////////////////////
template<>
CommandList *ResourceManager::allocate<CommandList>()
{
  auto slot = acquire_slot(sizeof(CommandList));

  if (!slot)
    return nullptr;

  return new(slot) CommandList;
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

  auto slot = const_cast<CommandList*>(commandlist);

  commandlist->~CommandList();

  release_slot(slot, sizeof(CommandList));
}
