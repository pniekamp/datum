//
// Datum - debug
//

//
// Copyright (c) 2015 Peter Niekamp
//

#pragma once

#include "platform.h"
#include <cassert>
#include <iostream>

#ifndef NDEBUG
#define DEBUG
#endif

#ifdef DEBUG
#include <atomic>
#include <intrin.h>
#include <thread>
#include "math/color.h"

inline void *operator new(std::size_t)
{
  assert(false);

  throw std::bad_alloc();
}

inline void operator delete(void *ptr) noexcept
{
  assert(false);
}

inline void operator delete(void *ptr, size_t) noexcept
{
  assert(false);
}

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

#define BEGIN_STAT_BLOCK(name) \
  auto stat_start_##name = __rdtsc();

#define END_STAT_BLOCK(name) \
  {                                                                                   \
    static int stat_count = 0;                                                        \
    static double stat_time = 0;                                                      \
                                                                                      \
    auto stat_end = __rdtsc();                                                        \
    stat_time += (stat_end - stat_start_##name) / clock_frequency();                         \
    stat_count += 1;                                                                  \
    if (stat_time > 1.0)                                                              \
    {                                                                                 \
      std::cout << #name << ": " << 1000 * stat_time / stat_count << "ms" << std::endl;                \
      stat_count = 0;                                                                 \
      stat_time = 0.0;                                                                \
    }                                                                                 \
  }

void update_debug_overlay(class DatumPlatform::GameInput const &input);
//void render_debug_overlay(class RenderList &renderlist, class DatumPlatform::Viewport const &viewport, class Font const *font);

#endif

#ifdef NDEBUG
#define BEGIN_FRAME(...)
#define BEGIN_TIMED_BLOCK(...)
#define END_TIMED_BLOCK(...)
#define update_debug_overlay(...)
#define push_debug_overlay(...)
#endif
