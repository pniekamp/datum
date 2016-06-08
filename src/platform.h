//
// Datum - platform interface
//

//
// Copyright (c) 2015 Peter Niekamp
//

#pragma once

#include <cstddef>
#include <vulkan/vulkan.h>

namespace DatumPlatform
{
  inline namespace v1
  {

    //|---------------------- GameMemory ----------------------------------------
    //|--------------------------------------------------------------------------

    struct GameMemory
    {
      std::size_t size;
      std::size_t capacity;

      void *data;
    };


    //|---------------------- GameInput -----------------------------------------
    //|--------------------------------------------------------------------------

    struct GameButton
    {
      bool state;
      int transitions;

      inline bool down() const { return (state); }
      inline bool pressed() const { return (state && transitions == 1) || (transitions > 1); }
    };

    struct GameController
    {
      GameButton move_up;
      GameButton move_down;
      GameButton move_left;
      GameButton move_right;

      GameButton action_up;
      GameButton action_down;
      GameButton action_left;
      GameButton action_right;

      GameButton left_shoulder;
      GameButton right_shoulder;

      GameButton back;
      GameButton start;
    };

    struct GameInput
    {
      enum MouseButton
      {
        Left,
        Right,
        Middle,
      };

      enum Modifiers
      {
        Shift = 0x01,
        Control = 0x02,
        Alt = 0x04,
      };

      long modifiers;

      GameButton keys[256];

      float mousex, mousey, mousez;

      GameButton mousebuttons[3];

      GameController controllers[5];
    };


    //|---------------------- RenderDevice --------------------------------------
    //|--------------------------------------------------------------------------

    struct RenderDevice
    {
      VkPhysicalDevice physicaldevice;

      VkDevice device;
    };


    //|---------------------- Viewport ------------------------------------------
    //|--------------------------------------------------------------------------

    struct Viewport
    {
      int x;
      int y;
      int width;
      int height;

      VkImage image;
      VkSemaphore acquirecomplete;
      VkSemaphore rendercomplete;
    };


    //|---------------------- PlatformInterface ---------------------------------
    //|--------------------------------------------------------------------------

    struct PlatformInterface
    {
      GameMemory gamememory;
      GameMemory gamescratchmemory;
      GameMemory renderscratchmemory;

      // device

      virtual RenderDevice render_device() = 0;


      // data access

      typedef void *handle_t;

      virtual handle_t open_handle(const char *identifier) = 0;

      virtual void read_handle(handle_t handle, uint64_t position, void *buffer, std::size_t n) = 0;

      virtual void close_handle(handle_t handle) = 0;


      // work queue

      virtual void submit_work(void (*func)(PlatformInterface &, void*, void*), void *ldata, void *rdata) = 0;


      // misc

      virtual void terminate() = 0;
    };
  }
}


// Game Interface

typedef void (*game_init_t)(DatumPlatform::PlatformInterface &platform);
typedef void (*game_reinit_t)(DatumPlatform::PlatformInterface &platform);
typedef void (*game_update_t)(DatumPlatform::PlatformInterface &platform, DatumPlatform::GameInput const &input, float dt);
typedef void (*game_render_t)(DatumPlatform::PlatformInterface &platform, DatumPlatform::Viewport const &viewport);


// Keyboard Key Codes

#define KB_KEY_BACKSPACE     0x08
#define KB_KEY_TAB           0x09
#define KB_KEY_ENTER         0x0D
#define KB_KEY_SHIFT         0x10
#define KB_KEY_CONTROL       0x11
#define KB_KEY_ALT           0x12
#define KB_KEY_ESCAPE        0x1B

#define KB_KEY_SPACE         0x20
#define KB_KEY_PRIOR         0x21
#define KB_KEY_NEXT          0x22
#define KB_KEY_END           0x23
#define KB_KEY_HOME          0x24
#define KB_KEY_LEFT          0x25
#define KB_KEY_UP            0x26
#define KB_KEY_RIGHT         0x27
#define KB_KEY_DOWN          0x28
#define KB_KEY_INSERT        0x2D
#define KB_KEY_DELETE        0x2E

#define KB_KEY_NUMPAD0       0x60
#define KB_KEY_NUMPAD1       0x61
#define KB_KEY_NUMPAD2       0x62
#define KB_KEY_NUMPAD3       0x63
#define KB_KEY_NUMPAD4       0x64
#define KB_KEY_NUMPAD5       0x65
#define KB_KEY_NUMPAD6       0x66
#define KB_KEY_NUMPAD7       0x67
#define KB_KEY_NUMPAD8       0x68
#define KB_KEY_NUMPAD9       0x69

#define KB_KEY_MULTIPLY      0x6A
#define KB_KEY_ADD           0x6B
#define KB_KEY_SEPARATOR     0x6C
#define KB_KEY_SUBTRACT      0x6D
#define KB_KEY_DECIMAL       0x6E
#define KB_KEY_DIVIDE        0x6F

#define KB_KEY_F1            0x70
#define KB_KEY_F2            0x71
#define KB_KEY_F3            0x72
#define KB_KEY_F4            0x73
#define KB_KEY_F5            0x74
#define KB_KEY_F6            0x75
#define KB_KEY_F7            0x76
#define KB_KEY_F8            0x77
#define KB_KEY_F9            0x78
#define KB_KEY_F10           0x79
#define KB_KEY_F11           0x7A
#define KB_KEY_F12           0x7B
#define KB_KEY_F13           0x7C
#define KB_KEY_F14           0x7D
#define KB_KEY_F15           0x7E
#define KB_KEY_F16           0x7F
#define KB_KEY_F17           0x80
#define KB_KEY_F18           0x81
#define KB_KEY_F19           0x82
#define KB_KEY_F20           0x83
#define KB_KEY_F21           0x84
#define KB_KEY_F22           0x85
#define KB_KEY_F23           0x86
#define KB_KEY_F24           0x87

#define KB_KEY_LEFT_SHIFT    0xA0
#define KB_KEY_RIGHT_SHIFT   0xA1
#define KB_KEY_LEFT_CONTROL  0xA2
#define KB_KEY_RIGHT_CONTROL 0xA3
#define KB_KEY_LEFT_ALT      0xA4
#define KB_KEY_RIGHT_ALT     0xA5

