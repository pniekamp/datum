//
// Datum - debug
//

//
// Copyright (c) 2015 Peter Niekamp
//

#pragma once

#include "platform.h"
#include "memory.h"
#include <cassert>
#include <iostream>

#ifndef NDEBUG
#define DEBUG
#endif

#ifdef DEBUG
#include <atomic>
#include <thread>
#include "math/color.h"

inline __attribute__((always_inline)) void *operator new(std::size_t)
{
  assert(false);

  throw std::bad_alloc();
}

inline __attribute__((always_inline)) void operator delete(void *ptr) noexcept
{
  assert(false);
}

inline __attribute__((always_inline)) void operator delete(void *ptr, size_t) noexcept
{
  assert(false);
}

//
// Timing
//

struct DebugInfoBlock
{
  const char *name;
  const char *filename;
  int linenumber;
  lml::Color3 color;
};

struct DebugLogEntry
{
  enum EntryType
  {
    Empty,
    FrameMarker,
    EnterBlock,
    ExitBlock,
    GpuSubmit,
    GpuBlock,
  };

  DebugInfoBlock const *info;
  EntryType type;
  std::thread::id thread;
  unsigned long long timestamp;
};

extern DebugLogEntry g_debuglog[4096];
extern std::atomic<size_t> g_debuglogtail;

double clock_frequency();

#define BEGIN_FRAME() \
  {                                                                                   \
    unsigned int p;                                                                   \
    size_t entry = g_debuglogtail.fetch_add(1) % extent<decltype(g_debuglog)>::value; \
    g_debuglog[entry].info = nullptr;                                                 \
    g_debuglog[entry].type = DebugLogEntry::FrameMarker;                              \
    g_debuglog[entry].thread = std::this_thread::get_id();                            \
    g_debuglog[entry].timestamp = __rdtscp(&p);                                       \
  }

#define BEGIN_TIMED_BLOCK(name, color) \
  {                                                                                   \
    static const DebugInfoBlock blockinfo = { #name, __FILE__, __LINE__, color};      \
    size_t entry = g_debuglogtail.fetch_add(1) % extent<decltype(g_debuglog)>::value; \
    g_debuglog[entry].info = &blockinfo;                                              \
    g_debuglog[entry].type = DebugLogEntry::EnterBlock;                               \
    g_debuglog[entry].thread = std::this_thread::get_id();                            \
    g_debuglog[entry].timestamp = __rdtsc();                                          \
  }

#define END_TIMED_BLOCK(name) \
  {                                                                                   \
    size_t entry = g_debuglogtail.fetch_add(1) % extent<decltype(g_debuglog)>::value; \
    g_debuglog[entry].info = nullptr;                                                 \
    g_debuglog[entry].type = DebugLogEntry::ExitBlock;                                \
    g_debuglog[entry].thread = std::this_thread::get_id();                            \
    g_debuglog[entry].timestamp = __rdtsc();                                          \
  }


#define GPU_SUBMIT() \
  {                                                                                   \
    size_t entry = g_debuglogtail.fetch_add(1) % extent<decltype(g_debuglog)>::value; \
    g_debuglog[entry].info = nullptr;                                                 \
    g_debuglog[entry].type = DebugLogEntry::GpuSubmit;                                \
    g_debuglog[entry].thread = std::this_thread::get_id();                            \
    g_debuglog[entry].timestamp = __rdtsc();                                          \
  }

#define GPU_TIMED_BLOCK(name, color, start, finish) \
  {                                                                                   \
    static const DebugInfoBlock blockinfo = { #name, __FILE__, __LINE__, color};      \
    size_t entry = g_debuglogtail.fetch_add(1) % extent<decltype(g_debuglog)>::value; \
    g_debuglog[entry].info = &blockinfo;                                              \
    g_debuglog[entry].type = DebugLogEntry::GpuBlock;                                 \
    g_debuglog[entry].thread = std::this_thread::get_id();                            \
    g_debuglog[entry].timestamp = finish - start;                                     \
  }

#define BEGIN_STAT_BLOCK(name) \
  auto stat_start_##name = __rdtsc();

#define END_STAT_BLOCK(name) \
  {                                                                                   \
    static int stat_count = 0;                                                        \
    static double stat_time = 0;                                                      \
                                                                                      \
    auto stat_end = __rdtsc();                                                        \
    stat_time += (stat_end - stat_start_##name) / clock_frequency();                  \
    stat_count += 1;                                                                  \
    if (stat_time > 1.0)                                                              \
    {                                                                                 \
      std::cout << #name << ": " << 1000 * stat_time / stat_count << "ms" << std::endl; \
      stat_count = 0;                                                                 \
      stat_time = 0.0;                                                                \
    }                                                                                 \
  }

//
// Statistics
//

struct DebugStatistics
{
  std::atomic<size_t> renderlumpsused;
  std::atomic<size_t> renderlumpscapacity;
  std::atomic<size_t> renderstorageused;
  std::atomic<size_t> renderstoragecapacity;

  std::atomic<size_t> resourceslotsused;
  std::atomic<size_t> resourceslotscapacity;
  std::atomic<size_t> resourcebufferused;
  std::atomic<size_t> resourcebuffercapacity;

  std::atomic<size_t> entityslotsused;
  std::atomic<size_t> entityslotscapacity;
};

extern DebugStatistics g_debugstatistics;

#define RESOURCE_SET(name, count) g_debugstatistics.name = count;
#define RESOURCE_ACQUIRE(name, count) g_debugstatistics.name += count;
#define RESOURCE_RELEASE(name, count) g_debugstatistics.name -= count;
#define STATISTIC_HIT(name, count) g_debugstatistics.name += count;

//
// Logging
//

#define LOG_ONCE(msg) \
  {                                                                                   \
    static bool logged = false;                                                       \
    if (!logged)                                                                      \
    {                                                                                 \
      std::cout << msg << std::endl;                                                  \
      logged = true;                                                                  \
    }                                                                                 \
  }

//
// Memory
//

void dump(const char *name, Arena const &arena);
void dump(const char *name, FreeList const &freelist);


//
// Menu
//

template<typename T>
void debug_menu_entry(const char *name, T const &value);

template<typename T>
T debug_menu_value(const char *name, T const &value, T const &min, T const &max);

#define DEBUG_MENU_ENTRY(name, value) \
  debug_menu_entry(name, value);

//
// Interface
//

void update_debug_overlay(class DatumPlatform::GameInput const &input, bool *accepted);
void render_debug_overlay(DatumPlatform::PlatformInterface &platform, class RenderContext &context, class ResourceManager *resources, class PushBuffer &pushbuffer, class DatumPlatform::Viewport const &viewport, class Font const *font);

#endif

#ifdef NDEBUG
#define BEGIN_FRAME(...)
#define BEGIN_TIMED_BLOCK(...)
#define END_TIMED_BLOCK(...)
#define GPU_SUBMIT(...)
#define GPU_TIMED_BLOCK(...)
#define BEGIN_STAT_BLOCK(...)
#define END_STAT_BLOCK(...)
#define RESOURCE_SET(...)
#define RESOURCE_ACQUIRE(...)
#define RESOURCE_RELEASE(...)
#define STATISTIC_HIT(...)
#define LOG_ONCE(...)
#define DEBUG_MENU_ENTRY(...)
#define update_debug_overlay(...)
#define render_debug_overlay(...)
#endif
