//
// Datum - platform
//

//
// Copyright (c) 2015 Peter Niekamp
//

#pragma once

#include "datum.h"
#include <vector>
#include <thread>
#include <deque>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <fstream>

namespace DatumPlatform
{

  //|---------------------- Game Memory ---------------------------------------
  //|--------------------------------------------------------------------------

  void gamememory_initialise(GameMemory &pool, void *data, size_t capacity);



  //|---------------------- Input Buffer --------------------------------------
  //|--------------------------------------------------------------------------

  class InputBuffer
  {
    public:

      enum class EventType
      {
        KeyDown,
        KeyUp,
        MouseMoveX,
        MouseMoveY,
        MouseMoveZ,
        MousePress,
        MouseRelease,
      };

      struct InputEvent
      {
        EventType type;

        int data;
      };

    public:
      InputBuffer();

      void register_viewport(int x, int y, int width, int height);

      void register_mousemove(int x, int y);
      void register_mousepress(GameInput::MouseButton button);
      void register_mouserelease(GameInput::MouseButton button);

      void register_keypress(int key);
      void register_keyrelease(int key);

      void release_all();

    public:

      GameInput grab();

    private:

      GameInput m_input;

      int m_x, m_y, m_width, m_height;

      std::vector<InputEvent> m_events;

      mutable std::mutex m_mutex;
  };



  //|---------------------- WorkQueue -----------------------------------------
  //|--------------------------------------------------------------------------

  class WorkQueue
  {
    public:
      WorkQueue(int threads = 4);
      ~WorkQueue();

      template<typename Func>
      void push(Func &&func)
      {
        std::unique_lock<std::mutex> lock(m_mutex);

        m_queue.push_back(std::forward<Func>(func));

        m_signal.notify_one();
      }

    private:

      std::atomic<bool> m_done;

      std::mutex m_mutex;

      std::condition_variable m_signal;

      std::deque<std::function<void()>> m_queue;

      std::vector<std::thread> m_threads;
  };



  //|---------------------- FileHandle ----------------------------------------
  //|--------------------------------------------------------------------------

  class FileHandle
  {
    public:
      FileHandle(const char *path);

      void read(uint64_t position, void *buffer, std::size_t n);

    private:

      std::mutex m_lock;

      std::fstream m_fio;
  };



} // namespace
