//
// datumtest.cpp
//

#include "platform.h"
#include <leap.h>
#include <leap/pathstring.h>
#include <windows.h>
#include <windowsx.h>
#include <vulkan/vulkan.h>
#include <iostream>
#include <algorithm>

using namespace std;
using namespace leap;
using namespace DatumPlatform;

void datumtest_init(DatumPlatform::PlatformInterface &platform);
void datumtest_update(DatumPlatform::PlatformInterface &platform, DatumPlatform::GameInput const &input, float dt);
void datumtest_render(DatumPlatform::PlatformInterface &platform, DatumPlatform::Viewport const &viewport);

//|---------------------- Platform ------------------------------------------
//|--------------------------------------------------------------------------

class Platform : public PlatformInterface
{
  public:

    Platform();

    void initialise(RenderDevice const &renderdevice, size_t gamememorysize, size_t scratchmemorysize);

  public:

    // device

    RenderDevice render_device() override;

    // data access

    handle_t open_handle(const char *identifier) override;
    size_t read_handle(handle_t handle, uint64_t position, void *buffer, size_t bytes) override;
    void close_handle(handle_t handle) override;

    // cursor

    void show_cursor(bool show) override;
    cursor_t create_cursor(int hx, int hy, int width, int height, void const *bits) override;
    void set_cursor_image(cursor_t cursor) override;
    void destroy_cursor(cursor_t cursor) override;
    void set_cursor_position(float x, float y) override;

    // work queue

    void submit_work(void (*func)(PlatformInterface &, void*, void*), void *ldata, void *rdata) override;

    // misc

    void terminate() override;

  public:

    bool terminate_requested() const { return m_terminaterequested.load(std::memory_order_relaxed); }

  protected:

    std::atomic<bool> m_terminaterequested;

    RenderDevice m_renderdevice;

    WorkQueue m_workqueue;
};


///////////////////////// Platform::Constructor /////////////////////////////
Platform::Platform()
{
  m_terminaterequested = false;
}


///////////////////////// Platform::initialise //////////////////////////////
void Platform::initialise(RenderDevice const &renderdevice, size_t gamememorysize, size_t scratchmemorysize)
{
  m_renderdevice = renderdevice;

  gamememory_initialise(gamememory, new char[gamememorysize], gamememorysize);
  gamememory_initialise(gamescratchmemory, new char[scratchmemorysize], scratchmemorysize);
  gamememory_initialise(renderscratchmemory, new char[scratchmemorysize], scratchmemorysize);
}


///////////////////////// PlatformCore::render_device ///////////////////////
RenderDevice Platform::render_device()
{
  return m_renderdevice;
}


///////////////////////// PlatformCore::open_handle /////////////////////////
PlatformInterface::handle_t Platform::open_handle(const char *identifier)
{
  return new FileHandle(pathstring(identifier).c_str());
}


///////////////////////// PlatformCore::read_handle /////////////////////////
size_t Platform::read_handle(PlatformInterface::handle_t handle, uint64_t position, void *buffer, size_t bytes)
{
  return static_cast<FileHandle*>(handle)->read(position, buffer, bytes);
}


///////////////////////// PlatformCore::close_handle ////////////////////////
void Platform::close_handle(PlatformInterface::handle_t handle)
{
  delete static_cast<FileHandle*>(handle);
}


///////////////////////// Platform::show_cursor /////////////////////////////
void Platform::show_cursor(bool show)
{
  throw runtime_error("Not Implemented");
}


///////////////////////// Platform::create_cursor ///////////////////////////
PlatformInterface::cursor_t Platform::create_cursor(int hx, int hy, int width, int height, void const *bits)
{
  throw runtime_error("Not Implemented");
}


///////////////////////// Platform::set_cursor_image ////////////////////////
void Platform::set_cursor_image(cursor_t handle)
{
  throw runtime_error("Not Implemented");
}


///////////////////////// Platform::destroy_cursor //////////////////////////
void Platform::destroy_cursor(cursor_t handle)
{
  throw runtime_error("Not Implemented");
}


///////////////////////// Platform::set_cursor_position /////////////////////
void Platform::set_cursor_position(float x, float y)
{
  throw runtime_error("Not Implemented");
}


///////////////////////// Platform::submit_work /////////////////////////////
void Platform::submit_work(void (*func)(PlatformInterface &, void*, void*), void *ldata, void *rdata)
{
  m_workqueue.push([=]() { func(*this, ldata, rdata); });
}


///////////////////////// Platform::terminate ///////////////////////////////
void Platform::terminate()
{
  m_terminaterequested = true;
}


//|---------------------- Game ----------------------------------------------
//|--------------------------------------------------------------------------

class Game
{
  public:

    Game();

    void init(VkPhysicalDevice physicaldevice, VkDevice device, VkQueue renderqueue, uint32_t renderqueuefamily, VkQueue transferqueue, uint32_t transferqueuefamily, VkQueue updatequeue, uint32_t updatequeuefamily);

    void update(float dt);

    void render(VkImage image, VkSemaphore acquirecomplete, VkSemaphore rendercomplete, int x, int y, int width, int height);

    void terminate();

  public:

    bool running() { return m_running.load(std::memory_order_relaxed); }

    InputBuffer &inputbuffer() { return m_inputbuffer; }

    Platform &platform() { return m_platform; }

  private:

    atomic<bool> m_running;

    game_init_t game_init;
    game_update_t game_update;
    game_render_t game_render;

    InputBuffer m_inputbuffer;

    Platform m_platform;

    int m_fpscount;
    chrono::high_resolution_clock::time_point m_fpstimer;
};


///////////////////////// Game::Contructor //////////////////////////////////
Game::Game()
{
  m_running = false;

  m_fpscount = 0;
  m_fpstimer = std::chrono::high_resolution_clock::now();
}


///////////////////////// Game::init ////////////////////////////////////////
void Game::init(VkPhysicalDevice physicaldevice, VkDevice device, VkQueue renderqueue, uint32_t renderqueuefamily, VkQueue transferqueue, uint32_t transferqueuefamily, VkQueue updatequeue, uint32_t updatequeuefamily)
{
  game_init = datumtest_init;
  game_update = datumtest_update;
  game_render = datumtest_render;

  if (!game_init || !game_update || !game_render)
    throw std::runtime_error("Unable to init game code");

  RenderDevice renderdevice = {};
  renderdevice.device = device;
  renderdevice.physicaldevice = physicaldevice;
  renderdevice.queues[0] = { renderqueue, renderqueuefamily };
  renderdevice.queues[1] = { transferqueue, transferqueuefamily };
  renderdevice.queues[2] = { updatequeue, updatequeuefamily };

  m_platform.initialise(renderdevice, 1*1024*1024*1024, 256*1024*1024);

  game_init(m_platform);

  m_running = true;
}


///////////////////////// Game::update //////////////////////////////////////
void Game::update(float dt)
{
  GameInput input = m_inputbuffer.grab();

  m_platform.gamescratchmemory.size = 0;

  game_update(m_platform, input, dt);

  if (m_platform.terminate_requested())
    terminate();
}


///////////////////////// Game::render //////////////////////////////////////
void Game::render(VkImage image, VkSemaphore acquirecomplete, VkSemaphore rendercomplete, int x, int y, int width, int height)
{
  m_platform.renderscratchmemory.size = 0;

  game_render(m_platform, { x, y, width, height, image, acquirecomplete, rendercomplete });

  ++m_fpscount;

  auto tick = std::chrono::high_resolution_clock::now();

  if (tick - m_fpstimer > std::chrono::seconds(1))
  {
    cout << m_fpscount / std::chrono::duration<double>(tick - m_fpstimer).count() << "fps" << endl;

    m_fpscount = 0;
    m_fpstimer = tick;
  }
}


///////////////////////// Game::terminate ///////////////////////////////////
void Game::terminate()
{
  m_running = false;
}



//|---------------------- Vulkan --------------------------------------------
//|--------------------------------------------------------------------------

#ifndef NDEBUG
#define VALIDATION 0
#endif

struct Vulkan
{
  void init(HINSTANCE hinstance, HWND hwnd);

  void resize();

  void acquire();
  void present();

  void destroy();

  VkInstance instance;
  VkPhysicalDevice physicaldevice;
  VkPhysicalDeviceProperties physicaldeviceproperties;
  VkPhysicalDeviceMemoryProperties physicaldevicememoryproperties;
  VkDevice device;

  VkQueue renderqueue;
  uint32_t renderqueuefamily;

  VkQueue transferqueue;
  uint32_t transferqueuefamily;

  VkQueue updatequeue;
  uint32_t updatequeuefamily;

  VkSurfaceKHR surface;

  VkSwapchainKHR swapchain;
  VkSwapchainCreateInfoKHR swapchaininfo;

  VkCommandPool commandpool;

  VkImage presentimages[3];

  VkSemaphore rendercomplete;
  VkSemaphore acquirecomplete;

  uint32_t imageindex;

  VkDebugReportCallbackEXT debugreportcallback;

} vulkan;


//|//////////////////// Vulkan::init ////////////////////////////////////////
void Vulkan::init(HINSTANCE hinstance, HWND hwnd)
{
  //
  // Instance, Device & Queue
  //

  VkApplicationInfo appinfo = {};
  appinfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  appinfo.pApplicationName = "Datum Test";
  appinfo.pEngineName = "Datum";
  appinfo.apiVersion = VK_MAKE_VERSION(1, 0, 65);

#if VALIDATION
  const char *validationlayers[] = { "VK_LAYER_KHRONOS_validation" };
//  const char *validationlayers[] = { "VK_LAYER_KHRONOS_validation", "VK_LAYER_LUNARG_assistant_layer" };
  const char *instanceextensions[] = { VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_WIN32_SURFACE_EXTENSION_NAME, VK_EXT_DEBUG_REPORT_EXTENSION_NAME };
#else
  const char **validationlayers = nullptr;
  const char *instanceextensions[] = { VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_WIN32_SURFACE_EXTENSION_NAME };
#endif

  VkInstanceCreateInfo instanceinfo = {};
  instanceinfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  instanceinfo.pApplicationInfo = &appinfo;
  instanceinfo.enabledExtensionCount = extentof(instanceextensions);
  instanceinfo.ppEnabledExtensionNames = instanceextensions;
  instanceinfo.enabledLayerCount = extentof(validationlayers);
  instanceinfo.ppEnabledLayerNames = validationlayers;

  if (vkCreateInstance(&instanceinfo, nullptr, &instance) != VK_SUCCESS)
    throw runtime_error("Vulkan CreateInstance failed");

  uint32_t physicaldevicecount = 0;
  vkEnumeratePhysicalDevices(instance, &physicaldevicecount, nullptr);

  if (physicaldevicecount == 0)
    throw runtime_error("Vulkan EnumeratePhysicalDevices failed");

  vector<VkPhysicalDevice> physicaldevices(physicaldevicecount);
  vkEnumeratePhysicalDevices(instance, &physicaldevicecount, physicaldevices.data());

  for(uint32_t i = 0; i < physicaldevicecount; ++i)
  {
    VkPhysicalDeviceProperties physicaldevicesproperties;
    vkGetPhysicalDeviceProperties(physicaldevices[i], &physicaldevicesproperties);

    cout << "Vulkan Physical Device " << i << ": " << physicaldevicesproperties.deviceName << endl;
  }

  physicaldevice = physicaldevices[0];

  vkGetPhysicalDeviceProperties(physicaldevice, &physicaldeviceproperties);

  vkGetPhysicalDeviceMemoryProperties(physicaldevice, &physicaldevicememoryproperties);

  const char* deviceextensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

  VkPhysicalDeviceFeatures devicefeatures = {};
  devicefeatures.shaderClipDistance = true;
  devicefeatures.shaderCullDistance = true;
  devicefeatures.geometryShader = true;
  devicefeatures.shaderTessellationAndGeometryPointSize = true;
  devicefeatures.shaderStorageImageWriteWithoutFormat = true;
  devicefeatures.samplerAnisotropy = true;
  devicefeatures.textureCompressionBC = true;

  uint32_t queuecount = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(physicaldevice, &queuecount, nullptr);

  if (queuecount == 0)
    throw runtime_error("Vulkan vkGetPhysicalDeviceQueueFamilyProperties failed");

  vector<VkQueueFamilyProperties> queueproperties(queuecount);
  vkGetPhysicalDeviceQueueFamilyProperties(physicaldevice, &queuecount, queueproperties.data());

  uint32_t graphicsqueueindex = 0;
  uint32_t transferqueueindex = queuecount;

  for(auto &queue : queueproperties)
  {
    if ((queue.queueFlags & (VK_QUEUE_GRAPHICS_BIT)) != 0)
      graphicsqueueindex = indexof(queueproperties, queue);

    if ((queue.queueFlags & (VK_QUEUE_GRAPHICS_BIT|VK_QUEUE_TRANSFER_BIT)) == VK_QUEUE_TRANSFER_BIT)
      transferqueueindex = indexof(queueproperties, queue);
  }

  float queuepriorities[] = { 0.0f, 0.0f };

  VkDeviceQueueCreateInfo queueinfo[2] = {};
  queueinfo[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  queueinfo[0].queueFamilyIndex = graphicsqueueindex;
  queueinfo[0].queueCount = extentof(queuepriorities) - 1;
  queueinfo[0].pQueuePriorities = queuepriorities;
  queueinfo[1].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  queueinfo[1].queueFamilyIndex = transferqueueindex;
  queueinfo[1].queueCount = 1;
  queueinfo[1].pQueuePriorities = queuepriorities + queueinfo[0].queueCount;

  VkDeviceCreateInfo deviceinfo = {};
  deviceinfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  deviceinfo.queueCreateInfoCount = extentof(queueinfo);
  deviceinfo.pQueueCreateInfos = queueinfo;
  deviceinfo.pEnabledFeatures = &devicefeatures;
  deviceinfo.enabledExtensionCount = extentof(deviceextensions);
  deviceinfo.ppEnabledExtensionNames = deviceextensions;
  deviceinfo.enabledLayerCount = extentof(validationlayers);
  deviceinfo.ppEnabledLayerNames = validationlayers;

  if (vkCreateDevice(physicaldevice, &deviceinfo, nullptr, &device) != VK_SUCCESS)
    throw runtime_error("Vulkan vkCreateDevice failed");

  vkGetDeviceQueue(device, graphicsqueueindex, 0, &renderqueue);
  renderqueuefamily = graphicsqueueindex;

  vkGetDeviceQueue(device, transferqueueindex, 0, &transferqueue);
  transferqueuefamily = transferqueueindex;

#if VALIDATION

  //
  // Debug
  //

  static auto debugmessagecallback = [](VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objtype, uint64_t srcobject, size_t location, int32_t msgcode, const char *layerprefix, const char *msg, void *userdata) -> VkBool32 {
    cout << msg << endl;
    return false;
  };

  auto VkCreateDebugReportCallback = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugReportCallbackEXT");

  VkDebugReportCallbackCreateInfoEXT debugreportinfo = {};
  debugreportinfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT;
  debugreportinfo.pfnCallback = (PFN_vkDebugReportCallbackEXT)debugmessagecallback;
  debugreportinfo.pUserData = nullptr;
  debugreportinfo.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;// | VK_DEBUG_REPORT_INFORMATION_BIT_EXT;

  VkCreateDebugReportCallback(instance, &debugreportinfo, nullptr, &debugreportcallback);

#endif

  //
  // Command Pool
  //

  VkCommandPoolCreateInfo commandpoolinfo = {};
  commandpoolinfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  commandpoolinfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  commandpoolinfo.queueFamilyIndex = graphicsqueueindex;

  if (vkCreateCommandPool(device, &commandpoolinfo, nullptr, &commandpool) != VK_SUCCESS)
    throw runtime_error("Vulkan vkCreateCommandPool failed");

  //
  // Surface
  //

  VkWin32SurfaceCreateInfoKHR surfaceinfo = {};
  surfaceinfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
  surfaceinfo.hinstance = hinstance;
  surfaceinfo.hwnd = hwnd;

  if (vkCreateWin32SurfaceKHR(instance, &surfaceinfo, nullptr, &surface) != VK_SUCCESS)
    throw runtime_error("Vulkan vkCreateWin32SurfaceKHR failed");

  VkBool32 surfacesupport = VK_FALSE;
  vkGetPhysicalDeviceSurfaceSupportKHR(physicaldevice, graphicsqueueindex, surface, &surfacesupport);

  if (surfacesupport != VK_TRUE)
    throw runtime_error("Vulkan vkGetPhysicalDeviceSurfaceSupportKHR error");

  uint32_t formatscount = 0;
  vkGetPhysicalDeviceSurfaceFormatsKHR(physicaldevice, surface, &formatscount, nullptr);

  vector<VkSurfaceFormatKHR> formats(formatscount);
  vkGetPhysicalDeviceSurfaceFormatsKHR(physicaldevice, surface, &formatscount, formats.data());

  if (!any_of(formats.begin(), formats.end(), [](VkSurfaceFormatKHR surface) { return (surface.format == VK_FORMAT_B8G8R8A8_SRGB); }))
    throw runtime_error("Vulkan vkGetPhysicalDeviceSurfaceFormatsKHR error");

  //
  // Swap Chain
  //

  bool vsync = true;
  uint32_t desiredimages = 2;

  uint32_t presentmodescount = 0;
  vkGetPhysicalDeviceSurfacePresentModesKHR(physicaldevice, surface, &presentmodescount, nullptr);

  vector<VkPresentModeKHR> presentmodes(presentmodescount);
  vkGetPhysicalDeviceSurfacePresentModesKHR(physicaldevice, surface, &presentmodescount, presentmodes.data());

  VkPresentModeKHR presentmode = VK_PRESENT_MODE_FIFO_KHR;
  for(size_t i = 0; i < presentmodescount; ++i)
  {
    if (presentmodes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
    {
      presentmode = presentmodes[i];
      desiredimages = 3;
      break;
    }

    if (!vsync && presentmodes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR)
    {
      presentmode = presentmodes[i];
    }
  }

  VkSurfaceCapabilitiesKHR surfacecapabilities;
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicaldevice, surface, &surfacecapabilities);

  if (surfacecapabilities.maxImageCount > 0 && desiredimages > surfacecapabilities.maxImageCount)
    desiredimages = surfacecapabilities.maxImageCount;

  VkSurfaceTransformFlagBitsKHR pretransform = (surfacecapabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) ? VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR : surfacecapabilities.currentTransform;

  swapchaininfo = {};
  swapchaininfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  swapchaininfo.surface = surface;
  swapchaininfo.minImageCount = desiredimages;
  swapchaininfo.imageFormat = VK_FORMAT_B8G8R8A8_SRGB;
  swapchaininfo.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
  swapchaininfo.imageExtent = surfacecapabilities.currentExtent;
  swapchaininfo.imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  swapchaininfo.preTransform = pretransform;
  swapchaininfo.imageArrayLayers = 1;
  swapchaininfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
  swapchaininfo.queueFamilyIndexCount = 0;
  swapchaininfo.pQueueFamilyIndices = nullptr;
  swapchaininfo.presentMode = presentmode;
  swapchaininfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  swapchaininfo.oldSwapchain = VK_NULL_HANDLE;
  swapchaininfo.clipped = true;

  if (vkCreateSwapchainKHR(device, &swapchaininfo, nullptr, &swapchain) != VK_SUCCESS)
    throw runtime_error("Vulkan vkCreateSwapchainKHR failed");

  uint32_t imagescount = 0;
  vkGetSwapchainImagesKHR(device, swapchain, &imagescount, nullptr);

  if (extentof(presentimages) < imagescount)
    throw runtime_error("Vulkan vkGetSwapchainImagesKHR failed");

  vkGetSwapchainImagesKHR(device, swapchain, &imagescount, presentimages);

  //
  // Present Images
  //

  VkCommandBufferAllocateInfo setupbufferinfo = {};
  setupbufferinfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  setupbufferinfo.commandPool = commandpool;
  setupbufferinfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  setupbufferinfo.commandBufferCount = 1;

  VkCommandBuffer setupbuffer;
  if (vkAllocateCommandBuffers(device, &setupbufferinfo, &setupbuffer) != VK_SUCCESS)
    throw runtime_error("Vulkan vkAllocateCommandBuffers failed");

  VkCommandBufferBeginInfo begininfo = {};
  begininfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

  if (vkBeginCommandBuffer(setupbuffer, &begininfo) != VK_SUCCESS)
    throw runtime_error("Vulkan vkBeginCommandBuffer failed");

  for (size_t i = 0; i < imagescount; ++i)
  {
    VkImageMemoryBarrier memorybarrier = {};
    memorybarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    memorybarrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    memorybarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    memorybarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    memorybarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    memorybarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    memorybarrier.image = presentimages[i];
    memorybarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

    vkCmdPipelineBarrier(setupbuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &memorybarrier);
  }

  vkEndCommandBuffer(setupbuffer);

  VkSubmitInfo submitinfo = {};
  submitinfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitinfo.commandBufferCount = 1;
  submitinfo.pCommandBuffers = &setupbuffer;

  vkQueueSubmit(renderqueue, 1, &submitinfo, VK_NULL_HANDLE);

  vkQueueWaitIdle(renderqueue);

  vkFreeCommandBuffers(device, commandpool, 1, &setupbuffer);

  //
  // Chain Semaphores
  //

  VkSemaphoreCreateInfo semaphoreinfo = {};
  semaphoreinfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
  semaphoreinfo.flags = 0;

  if (vkCreateSemaphore(device, &semaphoreinfo, nullptr, &acquirecomplete) != VK_SUCCESS)
    throw runtime_error("Vulkan vkCreateSemaphore failed");

  if (vkCreateSemaphore(device, &semaphoreinfo, nullptr, &rendercomplete) != VK_SUCCESS)
    throw runtime_error("Vulkan vkCreateSemaphore failed");
}


//|//////////////////// Vulkan::destroy /////////////////////////////////////
void Vulkan::destroy()
{
  vkDeviceWaitIdle(device);

  vkDestroySemaphore(device, acquirecomplete, nullptr);
  vkDestroySemaphore(device, rendercomplete, nullptr);

  vkDestroyCommandPool(device, commandpool, nullptr);

  vkDestroySwapchainKHR(device, swapchain, nullptr);

  vkDestroySurfaceKHR(instance, surface, nullptr);

#if VALIDATION
  auto VkDestroyDebugReportCallback = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugReportCallbackEXT");

  VkDestroyDebugReportCallback(instance, debugreportcallback, nullptr);
#endif

  vkDestroyDevice(device, nullptr);

  vkDestroyInstance(instance, nullptr);
}


//|//////////////////// Vulkan::resize //////////////////////////////////////
void Vulkan::resize()
{
  VkSurfaceCapabilitiesKHR surfacecapabilities;
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicaldevice, surface, &surfacecapabilities);

  if (swapchaininfo.imageExtent.width != surfacecapabilities.currentExtent.width || swapchaininfo.imageExtent.height != surfacecapabilities.currentExtent.height)
  {
    swapchaininfo.imageExtent = surfacecapabilities.currentExtent;
    swapchaininfo.oldSwapchain = swapchain;

    if (vkCreateSwapchainKHR(device, &swapchaininfo, nullptr, &swapchain) != VK_SUCCESS)
      throw runtime_error("Vulkan vkCreateSwapchainKHR failed");

    vkDeviceWaitIdle(device);
    vkDestroySwapchainKHR(device, swapchaininfo.oldSwapchain, nullptr);

    uint32_t imagescount = 0;
    vkGetSwapchainImagesKHR(device, swapchain, &imagescount, nullptr);
    vkGetSwapchainImagesKHR(device, swapchain, &imagescount, presentimages);

    VkCommandBufferAllocateInfo setupbufferinfo = {};
    setupbufferinfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    setupbufferinfo.commandPool = commandpool;
    setupbufferinfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    setupbufferinfo.commandBufferCount = 1;

    VkCommandBuffer setupbuffer;
    if (vkAllocateCommandBuffers(device, &setupbufferinfo, &setupbuffer) != VK_SUCCESS)
      throw runtime_error("Vulkan vkAllocateCommandBuffers failed");

    VkCommandBufferBeginInfo begininfo = {};
    begininfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    if (vkBeginCommandBuffer(setupbuffer, &begininfo) != VK_SUCCESS)
      throw runtime_error("Vulkan vkBeginCommandBuffer failed");

    for (size_t i = 0; i < imagescount; ++i)
    {
      VkImageMemoryBarrier memorybarrier = {};
      memorybarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
      memorybarrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
      memorybarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      memorybarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      memorybarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
      memorybarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
      memorybarrier.image = presentimages[i];
      memorybarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

      vkCmdPipelineBarrier(setupbuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &memorybarrier);
    }

    vkEndCommandBuffer(setupbuffer);

    VkSubmitInfo submitinfo = {};
    submitinfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitinfo.commandBufferCount = 1;
    submitinfo.pCommandBuffers = &setupbuffer;

    vkQueueSubmit(renderqueue, 1, &submitinfo, VK_NULL_HANDLE);

    vkQueueWaitIdle(renderqueue);

    vkFreeCommandBuffers(device, commandpool, 1, &setupbuffer);
  }
}


//|//////////////////// Vulkan::acquire /////////////////////////////////////
void Vulkan::acquire()
{
  vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, acquirecomplete, VK_NULL_HANDLE, &imageindex);
}


//|//////////////////// Vulkan::present /////////////////////////////////////
void Vulkan::present()
{
  VkPresentInfoKHR presentinfo = {};
  presentinfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  presentinfo.swapchainCount = 1;
  presentinfo.pSwapchains = &swapchain;
  presentinfo.pImageIndices = &imageindex;
  presentinfo.waitSemaphoreCount = 1;
  presentinfo.pWaitSemaphores = &rendercomplete;

  vkQueuePresentKHR(renderqueue, &presentinfo);
}


//|---------------------- Window --------------------------------------------
//|--------------------------------------------------------------------------

struct Window
{
  void init(HINSTANCE hinstance, Game *gameptr);
  
  void resize(int width, int height);

  void paint(UINT msg, WPARAM wParam, LPARAM lParam);

  void keypress(UINT msg, WPARAM wParam, LPARAM lParam);
  void keyrelease(UINT msg, WPARAM wParam, LPARAM lParam);

  void textinput(UINT msg, WPARAM wParam, LPARAM lParam);

  void mousepress(UINT msg, WPARAM wParam, LPARAM lParam);
  void mouserelease(UINT msg, WPARAM wParam, LPARAM lParam);
  void mousemove(UINT msg, WPARAM wParam, LPARAM lParam);

  void show();

  int width = 960;
  int height = 540;

  Game *game;

  HWND hwnd;

  bool visible;
  bool resizing;

  bool mousewrap;
  int lastmousex, lastmousey;
  int pressmousex, pressmousey;

  uint8_t keysym[256];

} window;


LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  switch (uMsg)
  {
    case WM_CLOSE:
      window.game->terminate();
      break;

    case WM_PAINT:
      window.paint(uMsg, wParam, lParam);
      break;

    case WM_ENTERSIZEMOVE:
      window.resizing = true;
      break;

    case WM_SIZE:
      window.resize(lParam & 0xffff, (lParam & 0xffff0000) >> 16);
      break;

    case WM_EXITSIZEMOVE:
      window.resizing = false;
      break;

    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
      window.keypress(uMsg, wParam, lParam);
      return 0;

    case WM_KEYUP:
    case WM_SYSKEYUP:
      window.keyrelease(uMsg, wParam, lParam);
      return 0;

    case WM_CHAR:
      window.textinput(uMsg, wParam, lParam);
      break;

    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_MBUTTONDOWN:
    case WM_XBUTTONDOWN:
      window.mousepress(uMsg, wParam, lParam);
      break;

    case WM_LBUTTONUP:
    case WM_RBUTTONUP:
    case WM_MBUTTONUP:
    case WM_XBUTTONUP:
      window.mouserelease(uMsg, wParam, lParam);
      break;

    case WM_MOUSEMOVE:
      window.mousemove(uMsg, wParam, lParam);
      break;

    case WM_KILLFOCUS:
      window.mousewrap = false;
      window.game->inputbuffer().release_all();
      break;

    default:
      break;
  }

  return DefWindowProc(hWnd, uMsg, wParam, lParam);
}


//|//////////////////// Window::init ////////////////////////////////////////
void Window::init(HINSTANCE hinstance, Game *gameptr)
{
  game = gameptr;

  visible = false;
  resizing = false;
  mousewrap = false;

  WNDCLASSEX winclass = {};
  winclass.cbSize = sizeof(WNDCLASSEX);
  winclass.style = CS_HREDRAW | CS_VREDRAW;
  winclass.lpfnWndProc = WndProc;
  winclass.hInstance = hinstance;
  winclass.hIcon = LoadIcon(NULL, IDI_APPLICATION);
  winclass.hCursor = LoadCursor(NULL, IDC_ARROW);
  winclass.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
  winclass.lpszClassName = "DatumTest";
  winclass.hIconSm = LoadIcon(NULL, IDI_WINLOGO);

  if (!RegisterClassEx(&winclass))
    throw runtime_error("Error registering window class");

  DWORD dwstyle = WS_OVERLAPPEDWINDOW | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
  DWORD dwexstyle = WS_EX_APPWINDOW | WS_EX_WINDOWEDGE;

  if (false) // fullscreen
  {
    DEVMODE devmode = {};
    devmode.dmSize = sizeof(devmode);
    devmode.dmPelsWidth = GetSystemMetrics(SM_CXSCREEN);
    devmode.dmPelsHeight = GetSystemMetrics(SM_CYSCREEN);
    devmode.dmBitsPerPel = 32;
    devmode.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;

    if (ChangeDisplaySettings(&devmode, CDS_FULLSCREEN) != DISP_CHANGE_SUCCESSFUL)
      throw runtime_error("Error going fullscreen");

    width = devmode.dmPelsWidth;
    height = devmode.dmPelsHeight;

    dwstyle = WS_POPUP | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
    dwexstyle = WS_EX_APPWINDOW;
  }

  RECT rect = { 0, 0, width, height };
  AdjustWindowRectEx(&rect, dwstyle, FALSE, dwexstyle);

  hwnd = CreateWindowEx(dwexstyle, "DatumTest", "Datum Test", dwstyle, CW_USEDEFAULT, CW_USEDEFAULT, rect.right - rect.left, rect.bottom - rect.top, NULL, NULL, hinstance, NULL);

  if (!hwnd)
    throw runtime_error("Error creating window");

  keysym[VK_ESCAPE] = KB_KEY_ESCAPE;
  keysym[VK_TAB] = KB_KEY_TAB;
  keysym[VK_RETURN] = KB_KEY_ENTER;
  keysym[VK_SPACE] = KB_KEY_SPACE;
  keysym[VK_MENU] = KB_KEY_ALT;
  keysym[VK_SHIFT] = KB_KEY_SHIFT;
  keysym[VK_CONTROL] = KB_KEY_CONTROL;
  keysym[VK_LEFT] = KB_KEY_LEFT; keysym[VK_DOWN] = KB_KEY_DOWN; keysym[VK_RIGHT] = KB_KEY_RIGHT; keysym[VK_UP] = KB_KEY_UP;
  keysym[VK_HOME] = KB_KEY_HOME; keysym[VK_END] = KB_KEY_END; keysym[VK_INSERT] = KB_KEY_INSERT; keysym[VK_DELETE] = KB_KEY_DELETE; keysym[VK_NEXT] = KB_KEY_NEXT; keysym[VK_PRIOR] = KB_KEY_PRIOR;
  keysym[VK_F1] = KB_KEY_F1;  keysym[VK_F2] = KB_KEY_F2;  keysym[VK_F3] = KB_KEY_F3;  keysym[VK_F4] = KB_KEY_F4;  keysym[VK_F5] = KB_KEY_F5;  keysym[VK_F6] = KB_KEY_F6;  keysym[VK_F7] = KB_KEY_F7;  keysym[VK_F8] = KB_KEY_F8;  keysym[VK_F9] = KB_KEY_F9;  keysym[VK_F10] = KB_KEY_F10;
  keysym['1'] = '1';  keysym['2'] = '2';  keysym['3'] = '3';  keysym['4'] = '4';  keysym['5'] = '5';  keysym['6'] = '6';  keysym['7'] = '7';  keysym['8'] = '8';  keysym['9'] = '9';  keysym['0'] = '0';  keysym[VK_OEM_MINUS] = '-';  keysym[VK_OEM_PLUS] = '=';  keysym[VK_BACK] = KB_KEY_BACKSPACE;
  keysym['Q'] = 'Q';  keysym['W'] = 'W';  keysym['E'] = 'E';  keysym['R'] = 'R';  keysym['T'] = 'T';  keysym['Y'] = 'Y';  keysym['U'] = 'U';  keysym['I'] = 'I';  keysym['O'] = 'O';  keysym['P'] = 'P';  keysym['['] = '[';  keysym[']'] = ']';  keysym['\\'] = '\\';
  keysym['A'] = 'A';  keysym['S'] = 'S';  keysym['D'] = 'D';  keysym['F'] = 'F';  keysym['G'] = 'G';  keysym['H'] = 'H';  keysym['J'] = 'J';  keysym['K'] = 'K';  keysym['L'] = 'L';  keysym[':'] = ':';  keysym['\''] = '\'';
  keysym['Z'] = 'Z';  keysym['X'] = 'X';  keysym['C'] = 'C';  keysym['V'] = 'V';  keysym['B'] = 'B';  keysym['N'] = 'N';  keysym['M'] = 'M';  keysym[VK_OEM_COMMA] = ',';  keysym[VK_OEM_PERIOD] = '.';
  keysym[VK_NUMPAD0] = KB_KEY_NUMPAD0;  keysym[VK_NUMPAD1] = KB_KEY_NUMPAD1;  keysym[VK_NUMPAD2] = KB_KEY_NUMPAD2;  keysym[VK_NUMPAD3] = KB_KEY_NUMPAD3;  keysym[VK_NUMPAD4] = KB_KEY_NUMPAD4;  keysym[VK_NUMPAD5] = KB_KEY_NUMPAD5;  keysym[VK_NUMPAD6] = KB_KEY_NUMPAD6;  keysym[VK_NUMPAD7] = KB_KEY_NUMPAD7;  keysym[VK_NUMPAD8] = KB_KEY_NUMPAD8;  keysym[VK_NUMPAD9] = KB_KEY_NUMPAD9;
  keysym[VK_OEM_2] = '/'; keysym[VK_OEM_3] = '~';
}


//|//////////////////// Window::resize //////////////////////////////////////
void Window::resize(int width, int height)
{
  if (width != 0 && height != 0)
  {
    window.width = width;
    window.height = height;

    window.game->inputbuffer().register_viewport(0, 0, width, height);

    if (vulkan.surface)
    {
      vulkan.resize();
    }
  }

  window.visible = (vulkan.surface && width != 0 && height != 0);
}


//|//////////////////// Window::paint ///////////////////////////////////////
void Window::paint(UINT msg, WPARAM wParam, LPARAM lParam)
{
  if (window.visible && !window.resizing)
  {
    vulkan.acquire();
    window.game->render(vulkan.presentimages[vulkan.imageindex], vulkan.acquirecomplete, vulkan.rendercomplete, 0, 0, window.width, window.height);
    vulkan.present();
  }
}


//|//////////////////// Window::keypress ////////////////////////////////////
void Window::keypress(UINT msg, WPARAM wParam, LPARAM lParam)
{
  uint8_t key = wParam;

  switch(key)
  {
    case VK_SHIFT:
      game->inputbuffer().register_keypress((MapVirtualKey((lParam & 0x00ff0000) >> 16, MAPVK_VSC_TO_VK_EX) == VK_RSHIFT) ? KB_KEY_RIGHT_SHIFT : KB_KEY_LEFT_SHIFT);
      break;

    case VK_CONTROL:
      game->inputbuffer().register_keypress((lParam & 0x01000000) ? KB_KEY_RIGHT_CONTROL : KB_KEY_LEFT_CONTROL);
      break;

    case VK_MENU:
      game->inputbuffer().register_keypress((lParam & 0x01000000) ? KB_KEY_RIGHT_ALT : KB_KEY_LEFT_ALT);
      break;

    default:
      game->inputbuffer().register_keypress(keysym[key]);
      break;
  }
}


//|//////////////////// Window::keyrelease //////////////////////////////////
void Window::keyrelease(UINT msg, WPARAM wParam, LPARAM lParam)
{
  uint8_t key = wParam;

  game->inputbuffer().register_keyrelease(keysym[key]);
}


//|//////////////////// Window::textinput ///////////////////////////////////
void Window::textinput(UINT msg, WPARAM wParam, LPARAM lParam)
{
  if (wParam >= ' ' && wParam != 127)
  {
    game->inputbuffer().register_textinput(wParam);
  }
}


//|//////////////////// Window::mousepress //////////////////////////////////
void Window::mousepress(UINT msg, WPARAM wParam, LPARAM lParam)
{
  switch(msg)
  {
    case WM_LBUTTONDOWN:
      game->inputbuffer().register_mousepress(GameInput::Left);
      break;

    case WM_RBUTTONDOWN:
      game->inputbuffer().register_mousepress(GameInput::Right);
      break;

    case WM_MBUTTONDOWN:
      game->inputbuffer().register_mousepress(GameInput::Middle);
      break;
  }

  mousewrap = true;
  pressmousex = lastmousex = GET_X_LPARAM(lParam);
  pressmousey = lastmousey = GET_Y_LPARAM(lParam);

  game->inputbuffer().register_mousemove(pressmousex, pressmousey, 0, 0);

  ShowCursor(false);

  SetCapture(hwnd);
}


//|//////////////////// Window::mouserelease ////////////////////////////////
void Window::mouserelease(UINT msg, WPARAM wParam, LPARAM lParam)
{
  switch(msg)
  {
    case WM_LBUTTONUP:
      game->inputbuffer().register_mouserelease(GameInput::Left);
      break;

    case WM_RBUTTONUP:
      game->inputbuffer().register_mouserelease(GameInput::Right);
      break;

    case WM_MBUTTONUP:
      game->inputbuffer().register_mouserelease(GameInput::Middle);
      break;
  }

  mousewrap = false;

  POINT pos = { pressmousex, pressmousey };
  ClientToScreen(hwnd, &pos);
  SetCursorPos(pos.x, pos.y);

  ShowCursor(true);

  ReleaseCapture();
}


//|//////////////////// Window::mousemove ///////////////////////////////////
void Window::mousemove(UINT msg, WPARAM wParam, LPARAM lParam)
{
  int deltax = 0;
  int deltay = 0;

  int mousex = GET_X_LPARAM(lParam);
  int mousey = GET_Y_LPARAM(lParam);

  if (mousewrap)
  {
    deltax = mousex - lastmousex;
    deltay = mousey - lastmousey;

    if (mousex != width/2 || mousey != height/2)
    {
      POINT pos = { width/2, height/2 };
      ClientToScreen(hwnd, &pos);
      SetCursorPos(pos.x, pos.y);
      lastmousex = width/2;
      lastmousey = height/2;
    }
  }

  game->inputbuffer().register_mousemove(mousex, mousey, deltax, deltay);
}


//|//////////////////// Window::show ////////////////////////////////////////
void Window::show()
{
  ShowWindow(hwnd, SW_SHOW);

  visible = true;
}



//|---------------------- main ----------------------------------------------
//|--------------------------------------------------------------------------

int main(int argc, char *args[])
{
  cout << "Datum Test" << endl;

  try
  {
    Game game;

    window.init(GetModuleHandle(NULL), &game);

    vulkan.init(GetModuleHandle(NULL), window.hwnd);

    game.init(vulkan.physicaldevice, vulkan.device, vulkan.renderqueue, vulkan.renderqueuefamily, vulkan.transferqueue, vulkan.transferqueuefamily, vulkan.updatequeue, vulkan.updatequeuefamily);

    window.show();

    thread updatethread([&]() {

      int hz = 60;

#if 0
      auto dt = std::chrono::nanoseconds(std::chrono::seconds(1)) / hz;

      auto tick = std::chrono::high_resolution_clock::now();

      while (game.running())
      {
        game.update(1.0f/hz);

        tick += dt;

        while (std::chrono::high_resolution_clock::now() < tick)
          ;
      }
#else
      LARGE_INTEGER frequency;
      QueryPerformanceFrequency(&frequency);
      auto dt = frequency.QuadPart / hz;

      LARGE_INTEGER now;
      QueryPerformanceCounter(&now);
      auto tick = now.QuadPart;

      while (game.running())
      {
        game.update(1.0f/hz);

        tick += dt;

        while (now.QuadPart < tick)
          QueryPerformanceCounter(&now);
      }
#endif
    });

    try
    {
      while (game.running())
      {
        MSG msg;

        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
          TranslateMessage(&msg);
          DispatchMessage(&msg);
        }
        else
        {
          RedrawWindow(window.hwnd, NULL, NULL, RDW_INTERNALPAINT);
        }
      }
    }
    catch(exception &e)
    {
      cout << "Critical Error: " << e.what() << endl;

      game.terminate();
    }

    updatethread.join();

    vulkan.destroy();
  }
  catch(exception &e)
  {
    cout << "Critical Error: " << e.what() << endl;
  }
}
