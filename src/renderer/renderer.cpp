//
// Datum - renderer
//

//
// Copyright (c) 2016 Peter Niekamp
//

#include "renderer.h"
#include "corepack.h"
#include "assetpack.h"
#include "leap/util.h"
#include "leap/lml/matrixconstants.h"
#include <vector>
#include <limits>
#include <algorithm>
#include "debug.h"

using namespace std;
using namespace lml;
using namespace vulkan;
using namespace DatumPlatform;
using leap::extentof;

enum VertexLayout
{
  position_offset = 0,
  texcoord_offset = 12,
  normal_offset = 20,
  tangent_offset = 32,

  stride = 48
};

enum ShaderLocation
{
  vertex_position = 0,
  vertex_texcoord = 1,
  vertex_normal = 2,
  vertex_tangent = 3,

};


//|---------------------- PushBuffer ----------------------------------------
//|--------------------------------------------------------------------------

///////////////////////// PushBuffer::Constructor ///////////////////////////
PushBuffer::PushBuffer(allocator_type const &allocator, std::size_t slabsize)
{
  m_slabsize = slabsize;
  m_slab = allocate<char, alignof(Header)>(allocator, m_slabsize);

  m_tail = m_slab;
}


///////////////////////// PushBuffer::push //////////////////////////////////
void *PushBuffer::push(Renderable::Type type, size_t size, size_t alignment)
{
  assert(((size_t)m_tail & (alignof(Header)-1)) == 0);
  assert(size + alignment + sizeof(Header) + alignof(Header) < std::numeric_limits<decltype(Header::size)>::max());

  auto header = reinterpret_cast<Header*>(m_tail);

  void *aligned = header + 1;

  size_t space = m_slabsize - (reinterpret_cast<size_t>(aligned) - reinterpret_cast<size_t>(m_slab));

  size = ((size - 1)/alignof(Header) + 1) * alignof(Header);

  if (!std::align(alignment, size, aligned, space))
    return nullptr;

  header->type = type;
  header->size = reinterpret_cast<size_t>(aligned) + size - reinterpret_cast<size_t>(header);

  m_tail = static_cast<char*>(aligned) + size;

  return aligned;
}




//|---------------------- Renderer ------------------------------------------
//|--------------------------------------------------------------------------

namespace
{

} // namespace


///////////////////////// prepare_render_context ////////////////////////////
bool prepare_render_context(DatumPlatform::PlatformInterface &platform, RendererContext &context, AssetManager *assets)
{
  if (context.initialised)
    return true;

  if (context.device == 0)
  {
    //
    // Vulkan Device
    //

    auto renderdevice = platform.render_device();

    initialise_vulkan_device(&context.device, renderdevice.physicaldevice, renderdevice.device);

    // Frame Fences

    for(auto &fence: context.framefences)
    {
      fence = create_fence(context.device, VK_FENCE_CREATE_SIGNALED_BIT);
    }

    // Primary Command Pool

    context.commandpool = create_commandpool(context.device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

    // Primary Command Buffers

    for(auto &commandbuffer : context.commandbuffers)
    {
      commandbuffer = allocate_commandbuffer(context.device, context.commandpool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    }
  }

  context.frame = 0;

  context.fbowidth = 0;
  context.fboheight = 0;

  context.initialised = true;

  return true;
}


///////////////////////// render ////////////////////////////////////////////
void render(RendererContext &context, DatumPlatform::Viewport const &viewport, Camera const &camera, PushBuffer const &renderables, RenderParams const &params)
{
  auto frameindex = context.frame % extentof(context.commandbuffers);

  auto &buffer = context.commandbuffers[frameindex];

  wait(context.device, context.framefences[frameindex]);

  begin(context.device, buffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

  transition_aquire(context.device, buffer, viewport.image);

  clear(context.device, buffer, viewport.image, Color4(0.5f + 0.5f*sin(context.frame * 0.0001), 0.5f + 0.5f*cos(context.frame * 0.0002), 0.5f + 0.5f*sin(context.frame * 0.0003), 1.0f));

  transition_present(context.device, buffer, viewport.image);

  end(context.device, buffer);

  submit(context.device, buffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, viewport.aquirecomplete, viewport.rendercomplete, context.framefences[frameindex]);

  ++context.frame;
}
