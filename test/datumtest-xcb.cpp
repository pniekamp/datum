//
// datumtest.cpp
//

#include "platform.h"
#include "leap/pathstring.h"
#include <vulkan/vulkan.h>
#include <iostream>

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

    void initialise(RenderDevice const &renderdevice, size_t gamememorysize);

  public:

    // device

    virtual RenderDevice render_device() override;


    // data access

    virtual handle_t open_handle(const char *identifier) override;

    virtual void read_handle(handle_t handle, uint64_t position, void *buffer, size_t n) override;

    virtual void close_handle(handle_t handle) override;


    // work queue

    void submit_work(void (*func)(PlatformInterface &, void*, void*), void *ldata, void *rdata) override;


    // misc

    void terminate() override;

  public:

    bool terminate_requested() const { return m_terminaterequested.load(std::memory_order_relaxed); }

  protected:

    std::atomic<bool> m_terminaterequested;

    std::vector<char> m_gamememory;
    std::vector<char> m_gamescratchmemory;
    std::vector<char> m_renderscratchmemory;

    RenderDevice m_renderdevice;

    WorkQueue m_workqueue;
};


///////////////////////// Platform::Constructor /////////////////////////////
Platform::Platform()
{
  m_terminaterequested = false;
}


///////////////////////// Platform::initialise //////////////////////////////
void Platform::initialise(RenderDevice const &renderdevice, size_t gamememorysize)
{
  m_renderdevice = renderdevice;

  m_gamememory.reserve(gamememorysize);
  m_gamescratchmemory.reserve(256*1024*1024);
  m_renderscratchmemory.reserve(256*1024*1024);

  gamememory_initialise(gamememory, m_gamememory.data(), m_gamememory.capacity());

  gamememory_initialise(gamescratchmemory, m_gamescratchmemory.data(), m_gamescratchmemory.capacity());

  gamememory_initialise(renderscratchmemory, m_renderscratchmemory.data(), m_renderscratchmemory.capacity());
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
void Platform::read_handle(PlatformInterface::handle_t handle, uint64_t position, void *buffer, size_t n)
{
  static_cast<FileHandle*>(handle)->read(position, buffer, n);
}


///////////////////////// PlatformCore::close_handle ////////////////////////
void Platform::close_handle(PlatformInterface::handle_t handle)
{
  delete static_cast<FileHandle*>(handle);
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

    void init(VkPhysicalDevice physicaldevice, VkDevice device);

    void update(float dt);

    void render(VkImage image, VkSemaphore aquirecomplete, VkSemaphore rendercomplete, int x, int y, int width, int height);

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
    chrono::system_clock::time_point m_fpstimer;
};


///////////////////////// Game::Contructor //////////////////////////////////
Game::Game()
{
  m_running = false;

  m_fpscount = 0;
  m_fpstimer = std::chrono::high_resolution_clock::now();
}


///////////////////////// Game::init ////////////////////////////////////////
void Game::init(VkPhysicalDevice physicaldevice, VkDevice device)
{
  game_init = datumtest_init;
  game_update = datumtest_update;
  game_render = datumtest_render;

  if (!game_init || !game_update || !game_render)
    throw std::runtime_error("Unable to init game code");

  m_platform.initialise({ physicaldevice, device }, 1*1024*1024*1024);

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
void Game::render(VkImage image, VkSemaphore aquirecomplete, VkSemaphore rendercomplete, int x, int y, int width, int height)
{
  m_platform.renderscratchmemory.size = 0;

  game_render(m_platform, { x, y, width, height, image, aquirecomplete, rendercomplete });

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
  void init(xcb_connection_t *connection, xcb_window_t window);

  void resize();

  void acquire();
  void present();

  void destroy();

  VkInstance instance;
  VkPhysicalDevice physicaldevice;
  VkPhysicalDeviceProperties physicaldeviceproperties;
  VkPhysicalDeviceMemoryProperties physicaldevicememoryproperties;
  VkDevice device;
  VkQueue queue;

  VkSurfaceKHR surface;
  VkSurfaceFormatKHR surfaceformat;

  VkSwapchainKHR swapchain;
  VkSwapchainCreateInfoKHR swapchaininfo;

  VkCommandPool commandpool;

  VkImage presentimages[3];

  VkRenderPass renderpass;

  VkSemaphore rendercomplete;
  VkSemaphore acquirecomplete;

  uint32_t imageindex;

  VkDebugReportCallbackEXT debugreportcallback;

} vulkan;


//|//////////////////// Vulkan::init ////////////////////////////////////////
void Vulkan::init(xcb_connection_t *connection, xcb_window_t window)
{
  //
  // Instance, Device & Queue
  //

  VkApplicationInfo appinfo = {};
  appinfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  appinfo.pApplicationName = "Datum Test";
  appinfo.pEngineName = "Datum";
  appinfo.apiVersion = VK_MAKE_VERSION(1, 0, 8);

#if VALIDATION
  const char *validationlayers[] = { "VK_LAYER_LUNARG_standard_validation" };
//  const char *validationlayers[] = { "VK_LAYER_GOOGLE_threading", "VK_LAYER_LUNARG_mem_tracker", "VK_LAYER_LUNARG_object_tracker", "VK_LAYER_LUNARG_draw_state", "VK_LAYER_LUNARG_param_checker", "VK_LAYER_LUNARG_swapchain", "VK_LAYER_LUNARG_device_limits", "VK_LAYER_LUNARG_image", "VK_LAYER_GOOGLE_unique_objects" };
  const char *instanceextensions[] = { VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_XCB_SURFACE_EXTENSION_NAME, VK_EXT_DEBUG_REPORT_EXTENSION_NAME };
#else
  const char *validationlayers[] = { };
  const char *instanceextensions[] = { VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_XCB_SURFACE_EXTENSION_NAME };
#endif

  VkInstanceCreateInfo instanceinfo = {};
  instanceinfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  instanceinfo.pApplicationInfo = &appinfo;
  instanceinfo.enabledExtensionCount = std::extent<decltype(instanceextensions)>::value;
  instanceinfo.ppEnabledExtensionNames = instanceextensions;
  instanceinfo.enabledLayerCount = std::extent<decltype(validationlayers)>::value;
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

  uint32_t queuecount = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(physicaldevice, &queuecount, nullptr);

  if (queuecount == 0)
    throw runtime_error("Vulkan vkGetPhysicalDeviceQueueFamilyProperties failed");

  vector<VkQueueFamilyProperties> queueproperties(queuecount);
  vkGetPhysicalDeviceQueueFamilyProperties(physicaldevice, &queuecount, queueproperties.data());

  uint32_t queueindex = 0;
  while (queueindex < queuecount && !(queueproperties[queueindex].queueFlags & VK_QUEUE_GRAPHICS_BIT))
    ++queueindex;

  array<float, 1> queuepriorities = { 0.0f };

  VkDeviceQueueCreateInfo queueinfo = {};
  queueinfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  queueinfo.queueFamilyIndex = queueindex;
  queueinfo.queueCount = 2;
  queueinfo.pQueuePriorities = queuepriorities.data();

  const char* deviceextensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

  VkDeviceCreateInfo deviceinfo = {};
  deviceinfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  deviceinfo.queueCreateInfoCount = 1;
  deviceinfo.pQueueCreateInfos = &queueinfo;
  deviceinfo.pEnabledFeatures = nullptr;
  deviceinfo.enabledExtensionCount = std::extent<decltype(deviceextensions)>::value;
  deviceinfo.ppEnabledExtensionNames = deviceextensions;
  deviceinfo.enabledLayerCount = std::extent<decltype(validationlayers)>::value;;
  deviceinfo.ppEnabledLayerNames = validationlayers;

  if (vkCreateDevice(physicaldevice, &deviceinfo, nullptr, &device) != VK_SUCCESS)
    throw runtime_error("Vulkan vkCreateDevice failed");

  vkGetPhysicalDeviceProperties(physicaldevice, &physicaldeviceproperties);

  vkGetPhysicalDeviceMemoryProperties(physicaldevice, &physicaldevicememoryproperties);

  vkGetDeviceQueue(device, queueindex, 0, &queue);

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
  commandpoolinfo.queueFamilyIndex = queueindex;

  if (vkCreateCommandPool(device, &commandpoolinfo, nullptr, &commandpool) != VK_SUCCESS)
    throw runtime_error("Vulkan vkCreateCommandPool failed");

  //
  // Surface
  //

  VkXcbSurfaceCreateInfoKHR surfaceinfo = {};
  surfaceinfo.sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
  surfaceinfo.connection = connection;
  surfaceinfo.window = window;

  if (vkCreateXcbSurfaceKHR(instance, &surfaceinfo, nullptr, &surface) != VK_SUCCESS)
    throw runtime_error("Vulkan vkCreateWin32SurfaceKHR failed");

  VkBool32 surfacesupport = VK_FALSE;
  vkGetPhysicalDeviceSurfaceSupportKHR(physicaldevice, queueindex, surface, &surfacesupport);

  if (surfacesupport != VK_TRUE)
    throw runtime_error("Vulkan vkGetPhysicalDeviceSurfaceSupportKHR error");

  uint32_t formatscount = 0;
  vkGetPhysicalDeviceSurfaceFormatsKHR(physicaldevice, surface, &formatscount, nullptr);

  vector<VkSurfaceFormatKHR> formats(formatscount);
  vkGetPhysicalDeviceSurfaceFormatsKHR(physicaldevice, surface, &formatscount, formats.data());

  surfaceformat = formats[0];

  if (surfaceformat.format == VK_FORMAT_UNDEFINED)
    surfaceformat.format = VK_FORMAT_B8G8R8A8_UNORM;

  //
  // Swap Chain
  //

  bool vsync = true;
  uint32_t desiredimages = 3;

  VkSurfaceCapabilitiesKHR surfacecapabilities;
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicaldevice, surface, &surfacecapabilities);

  if (surfacecapabilities.maxImageCount > 0 && desiredimages > surfacecapabilities.maxImageCount)
    desiredimages = surfacecapabilities.maxImageCount;

  uint32_t presentmodescount = 0;
  vkGetPhysicalDeviceSurfacePresentModesKHR(physicaldevice, surface, &presentmodescount, nullptr);

  vector<VkPresentModeKHR> presentmodes(presentmodescount);
  vkGetPhysicalDeviceSurfacePresentModesKHR(physicaldevice, surface, &presentmodescount, presentmodes.data());

  VkPresentModeKHR presentmode = VK_PRESENT_MODE_FIFO_KHR;
  for(size_t i = 0; i < presentmodescount; ++i)
  {
    if ((vsync && presentmodes[i] == VK_PRESENT_MODE_MAILBOX_KHR) || (!vsync && presentmodes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR))
    {
      presentmode = presentmodes[i];
      break;
    }
  }

  VkSurfaceTransformFlagBitsKHR pretransform = (surfacecapabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) ? VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR : surfacecapabilities.currentTransform;

  swapchaininfo = {};
  swapchaininfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  swapchaininfo.surface = surface;
  swapchaininfo.minImageCount = desiredimages;
  swapchaininfo.imageFormat = surfaceformat.format;
  swapchaininfo.imageColorSpace = surfaceformat.colorSpace;
  swapchaininfo.imageExtent = surfacecapabilities.currentExtent;
  swapchaininfo.imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
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

  if (extent<decltype(presentimages)>::value < imagescount)
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

  vkQueueSubmit(queue, 1, &submitinfo, VK_NULL_HANDLE);

  vkQueueWaitIdle(queue);

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

  if (surfacecapabilities.currentExtent.width == 0 || surfacecapabilities.currentExtent.height == 0)
    return;

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

    vkQueueSubmit(queue, 1, &submitinfo, VK_NULL_HANDLE);

    vkQueueWaitIdle(queue);

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

  vkQueuePresentKHR(queue, &presentinfo);
}


//|---------------------- Window --------------------------------------------
//|--------------------------------------------------------------------------

struct Window
{
  void init(Game *gameptr);

  void handle_event(xcb_generic_event_t const *event);

  void keypress(xcb_key_press_event_t const *event);
  void keyrelease(xcb_key_release_event_t const *event);

  void mousepress(xcb_button_press_event_t const *event);
  void mouserelease(xcb_button_release_event_t const *event);
  void mousemove(xcb_motion_notify_event_t const *event);

  void show();

  int width = 960;
  int height = 540;

  Game *game;

  xcb_connection_t *connection;
  xcb_screen_t *screen;
  xcb_window_t window;

  xcb_intern_atom_reply_t *wm_protocols;
  xcb_intern_atom_reply_t *wm_delete_window;

  xcb_cursor_t normalcursor;
  xcb_cursor_t blankcursor;

  bool mousewrap;
  int mousex, mousey;
  int lastmousex, lastmousey;
  int deltamousex, deltamousey;

  uint8_t keysym[256];

} window;


//|//////////////////// Window::init ////////////////////////////////////////
void Window::init(Game *gameptr)
{
  game = gameptr;

  mousewrap = false;

  int scn;
  connection = xcb_connect(nullptr, &scn);
  if (!connection)
    throw runtime_error("Error creating xcb connection");

  auto setup = xcb_get_setup(connection);

  auto iter = xcb_setup_roots_iterator(setup);
  while (scn-- > 0)
    xcb_screen_next(&iter);

  screen = iter.data;

  window = xcb_generate_id(connection);

  auto value_mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;

  uint32_t value_list[32] = {};
  value_list[0] = screen->black_pixel;
  value_list[1] = XCB_EVENT_MASK_KEY_PRESS | XCB_EVENT_MASK_KEY_RELEASE | XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE | XCB_EVENT_MASK_POINTER_MOTION | XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_STRUCTURE_NOTIFY;

  xcb_create_window(connection, XCB_COPY_FROM_PARENT, window, screen->root, 0, 0, width, height, 0, XCB_WINDOW_CLASS_INPUT_OUTPUT, screen->root_visual, value_mask, value_list);

  auto utf8_string = xcb_intern_atom_reply(connection, xcb_intern_atom(connection, 1, 11, "UTF8_STRING"), 0);
  auto _net_wm_name = xcb_intern_atom_reply(connection, xcb_intern_atom(connection, 1, 12, "_NET_WM_NAME"), 0);

  xcb_change_property(connection, XCB_PROP_MODE_REPLACE, window, _net_wm_name->atom, utf8_string->atom, 8, 9, "DatumTest");

  wm_protocols = xcb_intern_atom_reply(connection, xcb_intern_atom(connection, 1, 12, "WM_PROTOCOLS"), 0);

  wm_delete_window = xcb_intern_atom_reply(connection, xcb_intern_atom(connection, 0, 16, "WM_DELETE_WINDOW"), 0);

  xcb_change_property(connection, XCB_PROP_MODE_REPLACE, window, wm_protocols->atom, 4, 32, 1, &wm_delete_window->atom);

  free(wm_protocols);

  normalcursor = XCB_CURSOR_NONE;

  blankcursor = xcb_generate_id(connection);

  auto pixmap = xcb_generate_id(connection);

  xcb_create_pixmap(connection, 1, pixmap, screen->root, 1, 1);

  xcb_create_cursor(connection, blankcursor, pixmap, pixmap, 0, 0, 0, 0, 0, 0, 0, 0);

  xcb_map_window(connection, window);

  if (!window)
    throw runtime_error("Error creating window");

  keysym[9] = KB_KEY_ESCAPE;
  keysym[23] = KB_KEY_TAB;
  keysym[36] = KB_KEY_ENTER;
  keysym[65] = KB_KEY_SPACE;
  keysym[64] = KB_KEY_LEFT_ALT;  keysym[108] = KB_KEY_RIGHT_ALT;
  keysym[50] = KB_KEY_LEFT_SHIFT;  keysym[62] = KB_KEY_RIGHT_SHIFT;
  keysym[37] = KB_KEY_LEFT_CONTROL;  keysym[105] = KB_KEY_RIGHT_CONTROL;
  keysym[113] = KB_KEY_LEFT; keysym[116] = KB_KEY_DOWN; keysym[114] = KB_KEY_RIGHT; keysym[111] = KB_KEY_UP;
  keysym[67] = KB_KEY_F1;  keysym[68] = KB_KEY_F2;  keysym[69] = KB_KEY_F3;  keysym[70] = KB_KEY_F4;  keysym[71] = KB_KEY_F5;  keysym[72] = KB_KEY_F6;  keysym[73] = KB_KEY_F7;  keysym[74] = KB_KEY_F8;  keysym[75] = KB_KEY_F9;  keysym[76] = KB_KEY_F10;
  keysym[10] = '1';  keysym[11] = '2';  keysym[12] = '3';  keysym[13] = '4';  keysym[14] = '5';  keysym[15] = '6';  keysym[16] = '7';  keysym[17] = '8';  keysym[18] = '9';  keysym[19] = '0';  keysym[20] = '-';  keysym[21] = '=';  keysym[22] = KB_KEY_BACKSPACE;
  keysym[24] = 'Q';  keysym[25] = 'W';  keysym[26] = 'E';  keysym[27] = 'R';  keysym[28] = 'T';  keysym[29] = 'Y';  keysym[30] = 'U';  keysym[31] = 'I';  keysym[32] = 'O';  keysym[33] = 'P';  keysym[34] = '[';  keysym[35] = ']';  keysym[36] = '\\';
  keysym[38] = 'A';  keysym[39] = 'S';  keysym[40] = 'D';  keysym[41] = 'F';  keysym[42] = 'G';  keysym[43] = 'H';  keysym[44] = 'J';  keysym[45] = 'K';  keysym[46] = 'L';  keysym[47] = ':';  keysym[48] = '\'';
  keysym[52] = 'Z';  keysym[53] = 'X';  keysym[54] = 'C';  keysym[55] = 'V';  keysym[56] = 'B';  keysym[57] = 'N';  keysym[58] = 'M';  keysym[59] = ',';  keysym[60] = '.';  keysym[61] = '/';
  keysym[90] = KB_KEY_NUMPAD0;  keysym[87] = KB_KEY_NUMPAD1;  keysym[88] = KB_KEY_NUMPAD2;  keysym[89] = KB_KEY_NUMPAD3;  keysym[83] = KB_KEY_NUMPAD4;  keysym[84] = KB_KEY_NUMPAD5;  keysym[85] = KB_KEY_NUMPAD6;  keysym[79] = KB_KEY_NUMPAD7;  keysym[80] = KB_KEY_NUMPAD8;  keysym[81] = KB_KEY_NUMPAD9;

  xcb_flush(connection);
}


//|//////////////////// Window::handle_event ////////////////////////////////
void Window::handle_event(xcb_generic_event_t const *event)
{
  switch (event->response_type & 0x7f)
  {
    case XCB_CLIENT_MESSAGE:
      if (reinterpret_cast<xcb_client_message_event_t const *>(event)->data.data32[0] == wm_delete_window->atom)
        game->terminate();
      break;

    case XCB_CONFIGURE_NOTIFY:
      width = reinterpret_cast<xcb_configure_notify_event_t const *>(event)->width;
      height = reinterpret_cast<xcb_configure_notify_event_t const *>(event)->height;
      vulkan.resize();
      break;

    case XCB_KEY_PRESS:
      keypress(reinterpret_cast<xcb_key_press_event_t const *>(event));
      break;

    case XCB_KEY_RELEASE:
      keyrelease(reinterpret_cast<xcb_key_release_event_t const *>(event));
      break;

    case XCB_BUTTON_PRESS:
      mousepress(reinterpret_cast<xcb_button_press_event_t const *>(event));
      break;

    case XCB_BUTTON_RELEASE:
      mouserelease(reinterpret_cast<xcb_button_release_event_t const *>(event));
      break;

    case XCB_MOTION_NOTIFY:
      mousemove(reinterpret_cast<xcb_motion_notify_event_t const *>(event));
      break;

    default:
      break;
  }
}


//|//////////////////// Window::keypress ////////////////////////////////////
void Window::keypress(xcb_key_press_event_t const *event)
{
  game->inputbuffer().register_keypress(keysym[event->detail]);
}


//|//////////////////// Window::keyrelease //////////////////////////////////
void Window::keyrelease(xcb_key_release_event_t const *event)
{
  game->inputbuffer().register_keyrelease(keysym[event->detail]);
}


//|//////////////////// Window::mousepress //////////////////////////////////
void Window::mousepress(xcb_button_press_event_t const *event)
{
  switch(event->detail)
  {
    case 1:
      game->inputbuffer().register_mousepress(GameInput::Left);
      break;

    case 2:
      game->inputbuffer().register_mousepress(GameInput::Right);
      break;

    case 3:
      game->inputbuffer().register_mousepress(GameInput::Middle);
      break;
  }

  mousewrap = true;

  mousex = event->event_x;
  mousey = event->event_y;

  game->inputbuffer().register_mousemove(mousex, mousey);

  xcb_change_window_attributes(connection, window, XCB_CW_CURSOR, &blankcursor);

  xcb_flush(connection);
}


//|//////////////////// Window::mouserelease ////////////////////////////////
void Window::mouserelease(xcb_button_release_event_t const *event)
{
  switch(event->detail)
  {
    case 1:
      game->inputbuffer().register_mouserelease(GameInput::Left);
      break;

    case 2:
      game->inputbuffer().register_mouserelease(GameInput::Right);
      break;

    case 3:
      game->inputbuffer().register_mouserelease(GameInput::Middle);
      break;
  }

  mousewrap = false;

  game->inputbuffer().register_mousemove(mousex, mousey);

  xcb_warp_pointer(connection, XCB_NONE, window, 0, 0, 0, 0, mousex, mousey);

  xcb_change_window_attributes(connection, window, XCB_CW_CURSOR, &normalcursor);

  xcb_flush(connection);
}


//|//////////////////// Window::mousemove ///////////////////////////////////
void Window::mousemove(xcb_motion_notify_event_t const *event)
{
  if (mousewrap)
  {
    if (event->event_x != width/2 || event->event_y != height/2)
    {
      deltamousex += event->event_x - lastmousex;
      deltamousey += event->event_y - lastmousey;

      xcb_warp_pointer(connection, XCB_NONE, window, 0, 0, 0, 0, width/2, height/2);

      xcb_flush(connection);
    }
  }
  else
  {
    deltamousex = 0;
    deltamousey = 0;
    mousex = event->event_x;
    mousey = event->event_y;
  }

  game->inputbuffer().register_mousemove(mousex + deltamousex, mousey + deltamousey);

  lastmousex = event->event_x;
  lastmousey = event->event_y;
}



//|//////////////////// Window::show ////////////////////////////////////////
void Window::show()
{
}



//|---------------------- main ----------------------------------------------
//|--------------------------------------------------------------------------

int main(int argc, char *args[])
{
  cout << "Datum Test" << endl;

  try
  {
    Game game;

    window.init(&game);

    vulkan.init(window.connection, window.window);

    window.show();

    game.init(vulkan.physicaldevice, vulkan.device);

    thread updatethread([&]() {

      int hz = 60;

      auto dt = std::chrono::nanoseconds(std::chrono::seconds(1)) / hz;

      auto tick = std::chrono::high_resolution_clock::now();

      while (game.running())
      {
        game.update(1.0f/hz);

        tick += dt;

        while (std::chrono::high_resolution_clock::now() < tick)
          ;
      }
    });

    while (game.running())
    {
      if (xcb_generic_event_t *event = xcb_poll_for_event(window.connection))
      {
        window.handle_event(event);

        free(event);
      }
      else
      {
        vulkan.acquire();
        game.render(vulkan.presentimages[vulkan.imageindex], vulkan.acquirecomplete, vulkan.rendercomplete, 0, 0, window.width, window.height);
        vulkan.present();
      }
    }

    updatethread.join();

    vulkan.destroy();
  }
  catch(const exception &e)
  {
    cout << "Critical Error: " << e.what() << endl;
  }
}
