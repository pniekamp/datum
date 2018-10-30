//
// Datum - Platform
//

//
// Copyright (c) 2015 Peter Niekamp
//

#include "platform.h"
#include <memory>
#include <cstddef>
#include <iostream>

using namespace std;

namespace
{
  ///////////////////////// map_key_to_modifier ///////////////////////////////
  long map_key_to_modifier(int key)
  {
    switch (key)
    {
      case KB_KEY_SHIFT:
      case KB_KEY_LEFT_SHIFT:
      case KB_KEY_RIGHT_SHIFT:
        return DatumPlatform::GameInput::Shift;

      case KB_KEY_CONTROL:
      case KB_KEY_LEFT_CONTROL:
      case KB_KEY_RIGHT_CONTROL:
        return DatumPlatform::GameInput::Control;

      case KB_KEY_ALT:
      case KB_KEY_LEFT_ALT:
      case KB_KEY_RIGHT_ALT:
        return DatumPlatform::GameInput::Alt;

      default:
        return 0;
    }
  }

  ///////////////////////// append_codepoint //////////////////////////////////
  void append_codepoint(char *str, size_t n, uint32_t codepoint)
  {
    while (*str && n--)
      ++str;

    if (n > 4)
    {
      if (codepoint <= 0x7F)
      {
        str[0] = (char)codepoint;
        str[1] = 0;
      }
      else if (codepoint <= 0x7FF)
      {
        str[0] = 0xC0 | (char)((codepoint >> 6) & 0x1F);
        str[1] = 0x80 | (char)(codepoint & 0x3F);
        str[2] = 0;
      }
      else if (codepoint <= 0xFFFF)
      {
        str[0] = 0xE0 | (char)((codepoint >> 12) & 0x0F);
        str[1] = 0x80 | (char)((codepoint >> 6) & 0x3F);
        str[2] = 0x80 | (char)(codepoint & 0x3F);
        str[3] = 0;
      }
      else if (codepoint <= 0x10FFFF)
      {
        str[0] = 0xF0 | (char)((codepoint >> 18) & 0x0F);
        str[1] = 0x80 | (char)((codepoint >> 12) & 0x3F);
        str[2] = 0x80 | (char)((codepoint >> 6) & 0x3F);
        str[3] = 0x80 | (char)(codepoint & 0x3F);
        str[4] = 0;
      }
    }
  }
}


namespace DatumPlatform
{

  //|---------------------- GameMemory ----------------------------------------
  //|--------------------------------------------------------------------------

  ///////////////////////// GameMemory::initialise ////////////////////////////
  void gamememory_initialise(GameMemory &pool, void *data, size_t capacity)
  {
    pool.size = 0;
    pool.data = data;
    pool.capacity = capacity;

    std::align(alignof(max_align_t), 0, pool.data, pool.capacity);
  }



  //|---------------------- Input Buffer --------------------------------------
  //|--------------------------------------------------------------------------

  ///////////////////////// InputBuffer::Constructor //////////////////////////
  InputBuffer::InputBuffer()
  {
    m_input = {};
  }


  ///////////////////////// InputBuffer::register_viewport ///////////////////
  void InputBuffer::register_viewport(int x, int y, int width, int height)
  {
    m_x = x;
    m_y = y;
    m_width = width;
    m_height = height;
  }


  ///////////////////////// InputBuffer::register_mousemove ///////////////////
  void InputBuffer::register_mousemove(int x, int y, float deltax, float deltay)
  {
    lock_guard<mutex> lock(m_mutex);

    m_events.push_back({ Event::MouseMoveX, x });
    m_events.push_back({ Event::MouseMoveY, y });
    m_events.push_back({ Event::MouseDeltaX, (int)(deltax) });
    m_events.push_back({ Event::MouseDeltaY, (int)(deltay) });
  }


  ///////////////////////// InputBuffer::register_mousepress //////////////////
  void InputBuffer::register_mousepress(GameInput::MouseButton button)
  {
    lock_guard<mutex> lock(m_mutex);

    m_events.push_back({ Event::MousePress, button });
  }


  ///////////////////////// InputBuffer::register_mouserelease ////////////////
  void InputBuffer::register_mouserelease(GameInput::MouseButton button)
  {
    lock_guard<mutex> lock(m_mutex);

    m_events.push_back({ Event::MouseRelease, button });
  }


  ///////////////////////// InputBuffer::register_mousewheel //////////////////
  void InputBuffer::register_mousewheel(float z)
  {
    lock_guard<mutex> lock(m_mutex);

    m_events.push_back({ Event::MouseDeltaZ, (int)(z * 120) });
  }


  ///////////////////////// InputBuffer::register_keydown /////////////////////
  void InputBuffer::register_keypress(int key)
  {
    lock_guard<mutex> lock(m_mutex);

    m_events.push_back({ Event::KeyDown, key });
  }


  ///////////////////////// InputBuffer::register_keyup ///////////////////////
  void InputBuffer::register_keyrelease(int key)
  {
    lock_guard<mutex> lock(m_mutex);

    m_events.push_back({ Event::KeyUp, key });
  }


  ///////////////////////// InputBuffer::register_textinput ///////////////////
  void InputBuffer::register_textinput(uint32_t codepoint)
  {
    lock_guard<mutex> lock(m_mutex);

    m_events.push_back({ Event::Text, codepoint });
  }


  ///////////////////////// InputBuffer::release_all //////////////////////////
  void InputBuffer::release_all()
  {
    lock_guard<mutex> lock(m_mutex);

    m_input = {};

    m_events.clear();
  }


  ///////////////////////// InputBuffer::grab /////////////////////////////////
  GameInput InputBuffer::grab()
  {
    lock_guard<mutex> lock(m_mutex);

    // Mouse Buttons
    m_input.mousebuttons[GameInput::Left].transitions = 0;
    m_input.mousebuttons[GameInput::Right].transitions = 0;
    m_input.mousebuttons[GameInput::Middle].transitions = 0;

    // Mouse Deltas
    m_input.deltamousex = 0;
    m_input.deltamousey = 0;
    m_input.deltamousez = 0;

    // Keyboard
    for(auto &key : m_input.keys)
      key.transitions = 0;

    // Events
    m_input.eventcount = 0;

    auto processed = 0;

    for(auto &evt : m_events)
    {
      switch(evt.type)
      {
        case Event::KeyDown:
          m_input.keys[evt.data].state = true;
          m_input.keys[evt.data].transitions += 1;
          m_input.modifiers |= map_key_to_modifier(evt.data);
          m_input.events[m_input.eventcount].type = GameInput::Event::Key;
          m_input.events[m_input.eventcount].key = evt.data;
          m_input.events[m_input.eventcount].modifiers = m_input.modifiers;
          ++m_input.eventcount;
          break;

        case Event::KeyUp:
          m_input.keys[evt.data].state = false;
          m_input.keys[evt.data].transitions += 1;
          m_input.modifiers &= ~map_key_to_modifier(evt.data);
          break;

        case Event::MouseMoveX:
          m_input.mousex = evt.data;
          break;

        case Event::MouseMoveY:
          m_input.mousey = evt.data;
          break;

        case Event::MouseDeltaX:
          m_input.deltamousex += evt.data * (1.0f/m_width);
          break;

        case Event::MouseDeltaY:
          m_input.deltamousey += evt.data * (1.0f/m_width);
          break;

        case Event::MouseDeltaZ:
          m_input.deltamousez += evt.data * (1.0f/120.0f);
          break;

        case Event::MousePress:
          m_input.mousebuttons[evt.data].state = true;
          m_input.mousebuttons[evt.data].transitions += 1;
          break;

        case Event::MouseRelease:
          m_input.mousebuttons[evt.data].state = false;
          m_input.mousebuttons[evt.data].transitions += 1;
          break;

        case Event::Text:
          m_input.events[m_input.eventcount].type = GameInput::Event::Text;
          m_input.events[m_input.eventcount].text[0] = 0;
          append_codepoint(m_input.events[m_input.eventcount].text, sizeof(m_input.events[m_input.eventcount].text), evt.data);
          ++m_input.eventcount;
          break;
      }

      ++processed;

      if (m_input.eventcount == int(std::extent<decltype(m_input.events)>::value))
        break;
    }

    // keyboard controller
    m_input.controllers[0].move_up = m_input.keys['W'];
    m_input.controllers[0].move_down = m_input.keys['S'];
    m_input.controllers[0].move_left = m_input.keys['A'];
    m_input.controllers[0].move_right = m_input.keys['D'];

    m_events.erase(m_events.begin(), m_events.begin() + processed);

    return m_input;
  }



  //|---------------------- WorkQueue -----------------------------------------
  //|--------------------------------------------------------------------------

  ///////////////////////// WorkQueue::Constructor ////////////////////////////
  WorkQueue::WorkQueue(int threads)
  {
    m_done = false;

    for(int i = 0; i < threads; ++i)
    {
      m_threads.emplace_back([=]() {

        while (!m_done)
        {
          std::function<void()> work;

          {
            unique_lock<std::mutex> lock(m_mutex);

            while (m_queue.empty())
            {
              m_signal.wait(lock);
            }

            work = std::move(m_queue.front());

            m_queue.pop_front();
          }

          work();
        }

      });
    }
  }


  ///////////////////////// WorkQueue::Destructor /////////////////////////////
  WorkQueue::~WorkQueue()
  {
    for(size_t i = 0; i < m_threads.size(); ++i)
      push([=]() { m_done = true; });

    for(auto &thread : m_threads)
      thread.join();
  }


  //|---------------------- File Handle ---------------------------------------
  //|--------------------------------------------------------------------------

  ///////////////////////// FileHandle::Constructor ///////////////////////////
  FileHandle::FileHandle(const char *path)
  {
    m_fio.open(path, ios::in | ios::binary);

    if (!m_fio)
      throw runtime_error(string("FileHandle Open Error: ") + path);
  }


  ///////////////////////// FileHandle::Read //////////////////////////////////
  size_t FileHandle::read(uint64_t position, void *buffer, size_t bytes)
  {  
    lock_guard<mutex> lock(m_lock);

    m_fio.seekg(position);

    m_fio.read((char*)buffer, bytes);

    if (m_fio.bad())
      throw runtime_error("FileHandle Read Error");

    return m_fio.gcount();
  }

} // namespace
