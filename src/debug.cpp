//
// Datum - debug
//

//
// Copyright (c) 2015 Peter Niekamp
//

#include "debug.h"
#include "math/vec.h"
#include "leap/util.h"
#include <algorithm>
#include <sys/time.h>

using namespace std;
using namespace lml;
using namespace leap;
using namespace DatumPlatform;

#ifdef DEBUG

DebugLogEntry g_debuglog[4096];
std::atomic<size_t> g_debuglogtail;

namespace
{
  bool g_visible = false;

  bool g_running = false;

  Vec2 g_mousepos;

/*
  // Frame Timing

  unsigned long long g_frametime;

  float g_fpshistory[1920];
  size_t g_fpshistorytail = 0;

  // Block Timing

  const size_t Frames = 3;
  const size_t MaxThreads = 16;
  const size_t MaxBlocks = 512;

  unsigned long long g_blockbeg;
  unsigned long long g_blockend;

  struct Thread
  {
    size_t count;

    struct Block
    {
      DebugInfoBlock const *info;

      unsigned long long beg;
      unsigned long long end;

      unsigned long long level;

    } blocks[MaxBlocks];

  } g_threads[MaxThreads];

  struct Gpu
  {
    size_t count;

    struct Block
    {
      DebugInfoBlock const *info;

      unsigned long long beg;
      unsigned long long end;

    } blocks[MaxBlocks];

  } g_gpu;


  ///////////////////////// collate_debug_log ///////////////////////////////
  void collate_debug_log()
  {
    size_t tail = g_debuglogtail;

    size_t lastframes[5] = {};
    for(size_t i = 0; i < extentof(g_debuglog); ++i)
    {
      auto &entry = g_debuglog[(i + tail) % extentof(g_debuglog)];

      if (entry.type == DebugLogEntry::FrameMarker)
      {
        lastframes[4] = lastframes[3];
        lastframes[3] = lastframes[2];
        lastframes[2] = lastframes[1];
        lastframes[1] = lastframes[0];
        lastframes[0] = i;
      }
    }

    std::thread::id threads[MaxThreads] = {};
    for(size_t i = lastframes[4]; i < lastframes[0]; ++i)
    {
      auto &entry = g_debuglog[(i + tail) % extentof(g_debuglog)];

      for(size_t i = 0; i < extentof(threads); ++i)
      {
        if (threads[i] == entry.thread || threads[i] == std::thread::id())
        {
          threads[i] = entry.thread;
          break;
        }
      }
    }

    sort(begin(threads), end(threads), std::greater<>());

    //
    // Frame Timing
    //

    g_frametime = g_debuglog[(lastframes[0] + tail) % extentof(g_debuglog)].timestamp - g_debuglog[(lastframes[1] + tail) % extentof(g_debuglog)].timestamp;

    g_fpshistory[g_fpshistorytail++ % extentof(g_fpshistory)] = g_frametime / clock_frequency();

    //
    // Block Timing
    //

    memset(g_threads, 0, sizeof(g_threads));

    g_blockbeg = g_debuglog[(lastframes[Frames] + tail) % extentof(g_debuglog)].timestamp;
    g_blockend = g_debuglog[(lastframes[0] + tail) % extentof(g_debuglog)].timestamp;

    size_t opencount[MaxThreads] = {};
    size_t openblocks[MaxThreads][48];

    for(size_t i = lastframes[Frames]; i < lastframes[0]; ++i)
    {
      auto entry = g_debuglog[(i + tail) % extentof(g_debuglog)];

      size_t threadindex = 0;
      for(size_t i = 0; i < extentof(threads); ++i)
      {
        if (threads[i] == entry.thread)
          threadindex = i;
      }

      if (entry.type == DebugLogEntry::EnterBlock && entry.info)
      {
        assert(g_threads[threadindex].count < MaxBlocks);
        assert(opencount[threadindex] < extentof(openblocks[0]));

        auto &block = g_threads[threadindex].blocks[g_threads[threadindex].count];

        block.info = entry.info;

        block.beg = entry.timestamp;

        block.level = opencount[threadindex];

        openblocks[threadindex][opencount[threadindex]] = g_threads[threadindex].count;

        opencount[threadindex] += 1;

        g_threads[threadindex].count += 1;
      }

      if (entry.type == DebugLogEntry::ExitBlock && opencount[threadindex] > 0)
      {
        g_threads[threadindex].blocks[openblocks[threadindex][opencount[threadindex]-1]].end = entry.timestamp;

        opencount[threadindex] -= 1;
      }
    }

    //
    // Gpu Timing
    //

    memset(&g_gpu, 0, sizeof(g_gpu));

    unsigned long long basetime = 0;

    for(size_t i = lastframes[Frames]; i < lastframes[0]; ++i)
    {
      auto entry = g_debuglog[(i + tail) % extentof(g_debuglog)];

      if (entry.type == DebugLogEntry::FrameMarker)
      {
        basetime = entry.timestamp;
      }

      if (entry.type == DebugLogEntry::GpuBlock && entry.info)
      {
        assert(g_gpu.count < MaxBlocks);

        auto &block = g_gpu.blocks[g_gpu.count];

        block.info = entry.info;

        block.beg = basetime;
        block.end = basetime = basetime + entry.timestamp * 0.000000001 * clock_frequency();

        g_gpu.count += 1;
      }
    }
  }


  ///////////////////////// push_debug_overlay //////////////////////////////
  void push_debug_overlay(RenderList &renderlist, Viewport const &viewport, Font const *font, bool fps, bool blocktiming, bool gputiming, bool fpsgraph, bool gpumemory)
  {
    BEGIN_TIMED_BLOCK(DebugOverlay, Color3(1.0, 0.0, 0.0))

    Vec3 origin(-0.5f*viewport.width + 5.0f, 0.5f*viewport.height - 5.0f, 0.0f);

    //
    // Frame Timing
    //

    if (fps)
    {
      char buffer[128];
      snprintf(buffer, sizeof(buffer), "%f (%.0f fps)", g_frametime / clock_frequency(), clock_frequency() / g_frametime + 0.5);

      renderlist.push_text(origin + Vec3(0.0f, -font->ascent, 0.0f), font->height(), font, buffer);

      origin += Vec3(0.0f, -21.0f, 0.0f);
    }

    //
    // Block Timing
    //

    if (blocktiming)
    {
      const float BarDepth = 4.0f;
      const float BarHeight = 6.0f;
      const float LabelWidth = 150.0f;
      const float TimingsWidth = viewport.width - 20.0f;
      const float TimingsHeight = 100.0f;

      Vec3 labelorigin = origin + Vec3(5.0f, -2.0f, 0.0f);
      Vec3 timingsorigin = origin + Vec3(LabelWidth, -5.0f, 0.0f);

      renderlist.push_rect(origin, Rect2({0.0f, -TimingsHeight}, {viewport.width - 10.0f, 0.0f}), Color4(0.0f, 0.0f, 0.0f, 0.25f));

      float scale = (TimingsWidth - LabelWidth)/(g_blockend - g_blockbeg);

      Vec3 tippos;
      Thread::Block const *tipblk = nullptr;

      for(size_t i = 0; i < extentof(g_threads); ++i)
      {
        if (g_threads[i].count != 0)
        {
          unsigned long long totaltime = 0;

          for(size_t k = 0; k < g_threads[i].count; ++k)
          {
            auto &block = g_threads[i].blocks[k];

            auto beg = block.beg - g_blockbeg;
            auto end = block.end - g_blockbeg;

            if (block.level == 0)
            {
              totaltime += end - beg;
            }

            Rect2 bar({ beg * scale, -BarHeight*block.level - 8.0f }, { end * scale, -BarHeight*block.level });

            renderlist.push_rect(timingsorigin, bar, block.info->color);

            if (contains(Rect2(timingsorigin.xy + bar.min, timingsorigin.xy + bar.max), g_mousepos))
            {
              tippos = Vec3(timingsorigin.x + bar.min.x, timingsorigin.y + bar.max.y, 0.0f);
              tipblk = &block;
            }
          }

          char buffer[128];
          snprintf(buffer, sizeof(buffer), "%s (%f)", g_threads[i].blocks[0].info->name, totaltime / Frames / clock_frequency());

          renderlist.push_text(labelorigin + Vec3(0.0f, -font->ascent, 0.0f), font->height(), font, buffer);

          labelorigin.y -= BarDepth * BarHeight;
          timingsorigin.y -= BarDepth * BarHeight;
        }
      }

      if (tipblk)
      {
        char buffer[128];
        snprintf(buffer, sizeof(buffer), "%s (%f)", tipblk->info->name, (tipblk->end - tipblk->beg) / clock_frequency());

        renderlist.push_text(tippos, font->height(), font, buffer);
      }

      //
      // Gpu Timing
      //

      if (gputiming)
      {
        Gpu::Block const *tipblk = nullptr;

        if (g_gpu.count != 0)
        {
          unsigned long long totaltime = 0;

          for(size_t k = 0; k < g_gpu.count; ++k)
          {
            auto &block = g_gpu.blocks[k];

            auto beg = block.beg - g_blockbeg;
            auto end = block.end - g_blockbeg;

            totaltime += end - beg;

            Rect2 bar({ beg * scale, -8.0f }, { end * scale, 0.0 });

            renderlist.push_rect(timingsorigin, bar, block.info->color);

            if (contains(Rect2(timingsorigin.xy + bar.min, timingsorigin.xy + bar.max), g_mousepos))
            {
              tippos = Vec3(timingsorigin.x + bar.min.x, timingsorigin.y + bar.max.y, 0.0f);
              tipblk = &block;
            }
          }

          char buffer[128];
          snprintf(buffer, sizeof(buffer), "GPU (%f)", totaltime / Frames / clock_frequency());

          renderlist.push_text(labelorigin + Vec3(0.0f, -font->ascent, 0.0f), font->height(), font, buffer);

          labelorigin.y -= BarDepth * BarHeight;
          timingsorigin.y -= BarDepth * BarHeight;
        }

        if (tipblk)
        {
          char buffer[128];
          snprintf(buffer, sizeof(buffer), "%s (%f)", tipblk->info->name, (tipblk->end - tipblk->beg) / clock_frequency());

          renderlist.push_text(tippos, font->height(), font, buffer);
        }
      }

      origin += Vec3(0.0f, -TimingsHeight - 4.0f, 0.0f);
    }

    //
    // FPS Graph
    //

    if (fpsgraph)
    {
      const float GraphHeight = 80.0f;
      const float FpsScale = GraphHeight / (1.0f/15.0f);

      renderlist.push_rect(origin, Rect2({0.0f, -GraphHeight}, {viewport.width - 10.0f, 0.0f}), Color4(0.0f, 0.0f, 0.0f, 0.25f));

      size_t base = max(g_fpshistorytail, (size_t)viewport.width) - viewport.width - 11;

      for(int i = 2; i < viewport.width - 11; ++i)
      {
        auto a = g_fpshistory[(base + i - 1) % extentof(g_fpshistory)] * FpsScale;
        auto b = g_fpshistory[(base + i) % extentof(g_fpshistory)] * FpsScale;

        renderlist.push_line(origin + Vec3(i, -GraphHeight+a, 0.0f), origin + Vec3(i+1, -GraphHeight+b, 0.0f), Color4(0.5, 0.8, 0.5, 1.0));
      }

      origin += Vec3(0.0f, -GraphHeight - 4.0f, 0.0f);
    }

    //
    // Memory
    //

    if (gpumemory)
    {
      if (GLEW_NVX_gpu_memory_info)
      {
        int dedicated = 0;
        glGetIntegerv(GL_GPU_MEMORY_INFO_DEDICATED_VIDMEM_NVX, &dedicated);

        int total = 0;
        glGetIntegerv(GL_GPU_MEMORY_INFO_TOTAL_AVAILABLE_MEMORY_NVX, &total);

        int available = 0;
        glGetIntegerv(GL_GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX, &available);

        char buffer[512];
        snprintf(buffer, sizeof(buffer), "Dedicated: %0.2fGb, total %0.2fGb, avail: %0.2fGb\n", float(dedicated) / (1024.0f * 1024.0f), float(total) / (1024.0f * 1024.0f), float(available) / (1024.0f * 1024.0f));

        renderlist.push_text(origin + Vec3(0.0f, -font->ascent, 0.0f), font->height(), font, buffer);

        origin += Vec3(0.0f, -21.0f, 0.0f);
      }
    }

    END_TIMED_BLOCK(DebugOverlay)
  }
*/
}


///////////////////////// cycle_frequency /////////////////////////////////
double clock_frequency()
{
  static double frequency = 0;

  if (frequency == 0)
  {
    struct timezone tz;
    struct timeval tvstart, tvstop;
    unsigned long long int cycles[2];
    unsigned long microseconds;

    memset(&tz, 0, sizeof(tz));

    gettimeofday(&tvstart, &tz);
    cycles[0] = __rdtsc();
    gettimeofday(&tvstart, &tz);

    this_thread::sleep_for(chrono::milliseconds(250));

    gettimeofday(&tvstop, &tz);
    cycles[1] = __rdtsc();
    gettimeofday(&tvstop, &tz);

    microseconds = ((tvstop.tv_sec-tvstart.tv_sec)*1000000) + (tvstop.tv_usec-tvstart.tv_usec);

    frequency = (cycles[1]-cycles[0]) / microseconds * 1000000.0;

    cout << "Detected CPU Frequency: " << frequency / 1000000.0 << "MHz" << endl;
  }

  return frequency;
}


///////////////////////// update_debug_overlay //////////////////////////////
void update_debug_overlay(GameInput const &input)
{
  if (input.keys[KB_KEY_F1].pressed())
  {
    if (g_running)
      g_running = false;
    else if (g_visible)
      g_visible = false;
    else
      g_visible = g_running = true;
  }

  g_mousepos = Vec2(input.mousex, input.mousey);
}

/*
///////////////////////// render_debug_overlay //////////////////////////////
void render_debug_overlay(RenderList &renderlist, Viewport const &viewport, Font const *font)
{
  if (g_running)
  {
    collate_debug_log();
  }

  if (g_visible)
  {
    push_debug_overlay(renderlist, viewport, font, true, true, true, true, true);
  }
}
*/
#endif

