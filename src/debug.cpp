//
// Datum - debug
//

//
// Copyright (c) 2015 Peter Niekamp
//

#include "debug.h"
#include "math/vec.h"
#include "renderer/spritelist.h"
#include <leap.h>
#include <algorithm>
#include <sys/time.h>

using namespace std;
using namespace lml;
using namespace leap;
using namespace DatumPlatform;

#ifdef DEBUG

DebugLogEntry g_debuglog[4096];
std::atomic<size_t> g_debuglogtail;

DebugStatistics g_debugstatistics = {};

namespace
{
  bool g_visible = false;

  bool g_running = false;

  bool g_displayfps = true;
  bool g_displayblocktiming = true;
  bool g_displaygputiming = true;
  bool g_displayfpsgraph = true;

  Vec2 g_mousepos;

  // Frame Timing

  unsigned long long g_frametime;

  float g_fpshistory[1920];
  size_t g_fpshistorytail = 0;

  // Block Timing

  const size_t Frames = 4;
  const size_t MaxThreads = 16;
  const size_t MaxBlocks = 1024;

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

    size_t lastframes[8] = {};
    for(size_t i = 0; i < extentof(g_debuglog); ++i)
    {
      auto &entry = g_debuglog[(i + tail) % extentof(g_debuglog)];

      if (entry.type == DebugLogEntry::FrameMarker)
      {
        lastframes[7] = lastframes[6];
        lastframes[6] = lastframes[5];
        lastframes[5] = lastframes[4];
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

    g_blockbeg = g_debuglog[(lastframes[Frames+1] + tail) % extentof(g_debuglog)].timestamp;
    g_blockend = g_debuglog[(lastframes[1] + tail) % extentof(g_debuglog)].timestamp;

    size_t opencount[MaxThreads] = {};
    size_t openblocks[MaxThreads][48];

    for(size_t i = lastframes[Frames+2]; i < lastframes[0]; ++i)
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

    for(size_t i = lastframes[Frames+2]; i < lastframes[0]; ++i)
    {
      auto entry = g_debuglog[(i + tail) % extentof(g_debuglog)];

      if (entry.type == DebugLogEntry::GpuSubmit)
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
  void push_debug_overlay(DatumPlatform::PlatformInterface &platform, RenderContext &context, ResourceManager *resources, PushBuffer &pushbuffer, DatumPlatform::Viewport const &viewport, Font const *font)
  {
    BEGIN_TIMED_BLOCK(DebugOverlay, Color3(1.0, 0.0, 0.0))

    SpriteList overlay;
    SpriteList::BuildState buildstate;

    if (!overlay.begin(buildstate, platform, context, resources))
      return;

    auto cursor = Vec2(5.0f, 5.0f);

    //
    // Frame Timing
    //

    if (g_displayfps)
    {
      char buffer[128];
      snprintf(buffer, sizeof(buffer), "%f (%.0f fps)", g_frametime / clock_frequency(), clock_frequency() / g_frametime + 0.5);

      overlay.push_text(buildstate, cursor + Vec2(0, font->ascent), font->height(), font, buffer);

      cursor += Vec2(0.0f, font->lineheight());
    }

    //
    // Block Timing
    //

    if (g_displayblocktiming)
    {
      const float BarDepth = 4.0f;
      const float BarHeight = 6.0f;
      const float LabelWidth = 150.0f;
      const float TimingsWidth = viewport.width - 20.0f;
      const float TimingsHeight = 100.0f;

      Vec2 labelorigin = cursor + Vec2(5.0f, 2.0f);
      Vec2 timingsorigin = cursor + Vec2(LabelWidth, 6.0f);

      overlay.push_rect(buildstate, cursor, Rect2({0.0f, 0.0f}, {viewport.width - 10.0f, TimingsHeight}), Color4(0.0f, 0.0f, 0.0f, 0.25f));

      float scale = (TimingsWidth - LabelWidth)/(g_blockend - g_blockbeg);

      Vec2 tippos;
      Thread::Block const *tipblk = nullptr;

      for(size_t i = 0; i < extentof(g_threads); ++i)
      {
        int totalcount = 0;

        if (g_threads[i].count != 0)
        {
          unsigned long long totaltime = 0;

          for(size_t k = 0; k < g_threads[i].count; ++k)
          {
            auto &block = g_threads[i].blocks[k];

            if (block.end < g_blockbeg || block.beg > g_blockend)
              continue;

            auto beg = max(block.beg, g_blockbeg) - g_blockbeg;
            auto end = min(block.end, g_blockend) - g_blockbeg;

            if (block.level == 0)
            {
              totaltime += block.end - block.beg;
              totalcount += 1;
            }

            Rect2 barrect({ beg * scale, BarHeight*block.level }, { end * scale, BarHeight*block.level + 8.0f });

            overlay.push_rect(buildstate, timingsorigin, barrect, block.info->color);

            if (contains(Rect2(timingsorigin + barrect.min, timingsorigin + barrect.max), g_mousepos))
            {
              tippos = floor(timingsorigin + barrect.min);
              tipblk = &block;
            }
          }

          char buffer[128];
          snprintf(buffer, sizeof(buffer), "%s (%f)", g_threads[i].blocks[0].info->name, totaltime / max(totalcount, 1) / clock_frequency());

          overlay.push_text(buildstate, labelorigin + Vec2(0.0f, font->ascent), font->height(), font, buffer);

          labelorigin.y += BarDepth * BarHeight;
          timingsorigin.y += BarDepth * BarHeight;
        }
      }

      if (tipblk)
      {
        char buffer[128];
        snprintf(buffer, sizeof(buffer), "%s (%f)", tipblk->info->name, (tipblk->end - tipblk->beg) / clock_frequency());

        overlay.push_text(buildstate, tippos, font->height(), font, buffer);
      }

      //
      // Gpu Timing
      //

      if (g_displaygputiming)
      {
        Gpu::Block const *tipblk = nullptr;

        if (g_gpu.count != 0)
        {
          unsigned long long totaltime = 0;

          for(size_t k = 0; k < g_gpu.count; ++k)
          {
            auto &block = g_gpu.blocks[k];

            if (block.end < g_blockbeg || block.beg > g_blockend)
              continue;

            auto beg = max(block.beg, g_blockbeg) - g_blockbeg;
            auto end = min(block.end, g_blockend) - g_blockbeg;

            totaltime += block.end - block.beg;

            Rect2 barrect({ beg * scale, 0.0f }, { end * scale, 8.0 });

            overlay.push_rect(buildstate, timingsorigin, barrect, block.info->color);

            if (contains(Rect2(timingsorigin + barrect.min, timingsorigin + barrect.max), g_mousepos))
            {
              tippos = floor(timingsorigin + barrect.min);
              tipblk = &block;
            }
          }

          char buffer[128];
          snprintf(buffer, sizeof(buffer), "GPU (%f)", totaltime / Frames / clock_frequency());

          overlay.push_text(buildstate, labelorigin + Vec2(0.0f, font->ascent), font->height(), font, buffer);

          labelorigin.y += BarDepth * BarHeight;
          timingsorigin.y += BarDepth * BarHeight;
        }

        if (tipblk)
        {
          char buffer[128];
          snprintf(buffer, sizeof(buffer), "%s (%f)", tipblk->info->name, (tipblk->end - tipblk->beg) / clock_frequency());

          overlay.push_text(buildstate, tippos, font->height(), font, buffer);
        }
      }

      cursor += Vec2(0.0f, TimingsHeight + 4.0f);
    }

    //
    // FPS Graph
    //

    if (g_displayfpsgraph)
    {
      const float GraphHeight = 80.0f;
      const float FpsScale = GraphHeight / (1.0f/15.0f);

      overlay.push_rect(buildstate, cursor, Rect2({0.0f, 0.0f}, {viewport.width - 10.0f, GraphHeight}), Color4(0.0f, 0.0f, 0.0f, 0.25f));

      size_t base = max(g_fpshistorytail, (size_t)viewport.width) - viewport.width - 11;

      for(int i = 2; i < viewport.width - 11; ++i)
      {
        auto a = g_fpshistory[(base + i - 1) % extentof(g_fpshistory)] * FpsScale;
        auto b = g_fpshistory[(base + i) % extentof(g_fpshistory)] * FpsScale;

        overlay.push_line(buildstate, cursor + Vec2(i, GraphHeight-a), cursor + Vec2(i+1, GraphHeight-b), Color4(0.5, 0.8, 0.5, 1.0));
      }

      cursor += Vec2(0.0f, GraphHeight + 4.0f);
    }

    overlay.finalise(buildstate);

    auto entry = pushbuffer.push<Renderable::Sprites>();

    if (entry)
    {
      entry->viewport = Rect2(Vec2(viewport.x, viewport.y), Vec2(viewport.x + viewport.width, viewport.y + viewport.height));
      entry->commandlist = overlay.commandlist();
    }

    END_TIMED_BLOCK(DebugOverlay)
  }
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


///////////////////////// dump //////////////////////////////////////////////
void dump(const char *name, Arena const &arena)
{
  cout << "Arena: " << name << " " << arena.size / 1024 / 1024 << "/" << arena.capacity / 1024 / 1024 << " MiB" << endl;
}


///////////////////////// dump //////////////////////////////////////////////
void dump(const char *name, FreeList const &freelist)
{
  cout << "Freelist: " << name;

  for(size_t index = 0; index < extentof(freelist.m_freelist); ++index)
  {
    size_t nodes = 0;

    void *entry = freelist.m_freelist[index];

    while (entry != nullptr)
    {
      auto node = reinterpret_cast<FreeList::Node*>((reinterpret_cast<std::size_t>(entry) + alignof(FreeList::Node) - 1) & -alignof(FreeList::Node));

      nodes += 1;

      entry = node->next;
    }

    cout << " [" << nodes << "]";
  }

  cout << endl;
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

  if (g_visible)
  {
    if (input.keys[KB_KEY_F2].pressed())
    {
      g_displayfpsgraph = !g_displayfpsgraph;
    }

    if (input.keys[KB_KEY_F3].pressed())
    {
      g_displayblocktiming = g_displaygputiming = !g_displayblocktiming;
    }
  }

  g_mousepos = Vec2(input.mousex, input.mousey);
}


///////////////////////// render_debug_overlay //////////////////////////////
void render_debug_overlay(DatumPlatform::PlatformInterface &platform, RenderContext &context, ResourceManager *resources, PushBuffer &pushbuffer, DatumPlatform::Viewport const &viewport, Font const *font)
{
  if (g_running)
  {
    collate_debug_log();
  }

  if (g_visible)
  {
    push_debug_overlay(platform, context, resources, pushbuffer, viewport, font);
  }
}

#endif

