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
#include <leap/threadcontrol.h>
#include <atomic>
#include <typeindex>
#include <algorithm>
#include <fstream>

using namespace std;
using namespace lml;
using namespace leap;
using namespace leap::threadlib;
using namespace DatumPlatform;

#ifdef DEBUG

DebugLogEntry g_debuglog[4096];
std::atomic<size_t> g_debuglogtail;

DebugInfoBlock *g_infoblocks[256];
std::atomic<size_t> g_infoblockcount;

namespace
{
  bool g_visible = false;

  bool g_running = false;

  bool g_displayfps = true;
  bool g_displayblocktiming = true;
  bool g_displaygputiming = true;
  bool g_displayframegraph = false;
  bool g_displaydebugmenu = true;

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
    size_t blockcount;

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
    size_t blockcount;

    struct Block
    {
      DebugInfoBlock const *info;

      unsigned long long beg;
      unsigned long long end;

    } blocks[MaxBlocks];

  } g_gpu;

  // Resources

  struct Resources
  {
    size_t renderlumpsused;
    size_t renderlumpscapacity;
    size_t renderstorageused;
    size_t renderstoragecapacity;
    size_t resourceslotsused;
    size_t resourceslotscapacity;
    size_t resourcebufferused;
    size_t resourcebuffercapacity;
    size_t entityslotsused;
    size_t entityslotscapacity;

  } g_resources;

  // Menu

  struct Menu
  {
    size_t entrycount;

    struct Entry
    {
      const char *name;

      const char *label;

      type_index type = typeid(void);
      alignas(alignof(max_align_t)) char value_[256];

      bool editable = false;
      alignas(alignof(max_align_t)) char min_[256];
      alignas(alignof(max_align_t)) char max_[256];

      template<typename T> T &value() { union { T*a; char*b; } p; p.b = value_; return *p.a; }
      template<typename T> T &min() { union { T*a; char*b; } p; p.b = min_; return *p.a; }
      template<typename T> T &max() { union { T*a; char*b; } p; p.b = max_; return *p.a; }

    } entries[64];

    size_t groupcount;

    struct Group
    {
      int namelen;
      const char *name;

      bool expanded = false;

      size_t entrycount;

      Entry *entries[32];

    } groups[16];

    SpinLock m_mutex;

  } g_menu;

  struct Ui
  {
    struct Interaction
    {
      enum Type
      {
        None,
        ToggleVisible,
        ToggleBlockTiming,
        ToggleFrameGraph,
        ToggleGroup,
        ToggleBoolEntry,
        SlideFloat0Entry,
        SlideFloat1Entry,
        SlideFloat2Entry,
      };

      Type type;
      size_t id;
    };

    std::atomic<Interaction> hot;
    std::atomic<Interaction> nexthot;

    Vec2 mousepos;

    bool mousecaptured;
    Vec2 mousepresspos;

  } g_interaction;


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
        assert(g_threads[threadindex].blockcount < MaxBlocks);
        assert(opencount[threadindex] < extentof(openblocks[0]));

        auto &block = g_threads[threadindex].blocks[g_threads[threadindex].blockcount];

        block.info = entry.info;

        block.beg = entry.timestamp;

        block.level = opencount[threadindex];

        openblocks[threadindex][opencount[threadindex]] = g_threads[threadindex].blockcount;

        opencount[threadindex] += 1;

        g_threads[threadindex].blockcount += 1;
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
        assert(g_gpu.blockcount < MaxBlocks);

        auto &block = g_gpu.blocks[g_gpu.blockcount];

        block.info = entry.info;

        block.beg = basetime;
        block.end = basetime = basetime + entry.timestamp * 0.000000001 * clock_frequency();

        g_gpu.blockcount += 1;
      }
    }

    //
    // Resources
    //

    for(size_t i = 0; i < extentof(g_debuglog); ++i)
    {
      auto &entry = g_debuglog[(i + tail) % extentof(g_debuglog)];

      switch (entry.type)
      {
        case DebugLogEntry::RenderLump:
          g_resources.renderlumpsused = entry.resourceused;
          g_resources.renderlumpscapacity = entry.resourcecapacity;
          break;

        case DebugLogEntry::RenderStorage:
          g_resources.renderstorageused = entry.resourceused;
          g_resources.renderstoragecapacity = entry.resourcecapacity;
          break;

        case DebugLogEntry::ResourceSlot:
          g_resources.resourceslotsused = entry.resourceused;
          g_resources.resourceslotscapacity = entry.resourcecapacity;
          break;

        case DebugLogEntry::ResourceBuffer:
          g_resources.resourcebufferused = entry.resourceused;
          g_resources.resourcebuffercapacity = entry.resourcecapacity;
          break;

        case DebugLogEntry::EntitySlot:
          g_resources.entityslotsused = entry.resourceused;
          g_resources.entityslotscapacity = entry.resourcecapacity;
          break;

        default:
          break;
      }
    }
  }


  ///////////////////////// ishot ///////////////////////////////////////////
  bool ishot(Ui::Interaction::Type type)
  {
    auto hot = g_interaction.hot.load();

    return (hot.type == type);
  }

  bool ishot(Ui::Interaction::Type type, size_t id)
  {
    auto hot = g_interaction.hot.load();

    return (hot.type == type && hot.id == id);
  }


  ///////////////////////// push_debug_overlay //////////////////////////////
  void push_debug_overlay(DatumPlatform::PlatformInterface &platform, RenderContext &context, ResourceManager *resources, PushBuffer &pushbuffer, DatumPlatform::Viewport const &viewport, Font const *font, Ui::Interaction *interaction)
  {
    BEGIN_TIMED_BLOCK(DebugOverlay, Color3(1.0f, 0.0f, 0.0f))

    SpriteList overlay;
    SpriteList::BuildState buildstate;

    if (!overlay.begin(buildstate, platform, context, resources))
      return;

    auto mousepos = viewport.width * g_interaction.mousepos;

    auto cursor = Vec2(5.0f, 5.0f);

    //
    // Frame Timing
    //

    if (g_displayfps)
    {
      char buffer[128];
      snprintf(buffer, sizeof(buffer), "%f (%.0f fps)", g_frametime / clock_frequency(), clock_frequency() / g_frametime + 0.5);

      overlay.push_text(buildstate, cursor + Vec2(0, font->ascent), font->height(), font, buffer, ishot(Ui::Interaction::ToggleVisible) ? Color4(1, 1, 0) : Color4(1, 1, 1));

      if (contains(Rect2(cursor, cursor + Vec2(font->width(buffer), font->lineheight())), mousepos))
        *interaction = { Ui::Interaction::ToggleVisible };

      cursor += Vec2(0.0f, font->lineheight() + 2);
    }

    if (g_visible)
    {
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

        Vec2 labelorigin = cursor + Vec2(16.0f, 2.0f);
        Vec2 timingsorigin = cursor + Vec2(LabelWidth, 6.0f);

        overlay.push_rect(buildstate, cursor, Rect2({0.0f, 0.0f}, {viewport.width - 10.0f, TimingsHeight}), Color4(0.0f, 0.0f, 0.0f, 0.25f));

        float scale = (TimingsWidth - LabelWidth)/(g_blockend - g_blockbeg);

        Vec2 tippos;
        Thread::Block const *tipblk = nullptr;

        for(size_t i = 0; i < extentof(g_threads); ++i)
        {
          int totalcount = 0;

          if (g_threads[i].blockcount != 0)
          {
            unsigned long long totaltime = 0;

            for(size_t k = 0; k < g_threads[i].blockcount; ++k)
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

              if (contains(Rect2(timingsorigin + barrect.min, timingsorigin + barrect.max), mousepos))
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

          if (g_gpu.blockcount != 0)
          {
            unsigned long long totaltime = 0;

            for(size_t k = 0; k < g_gpu.blockcount; ++k)
            {
              auto &block = g_gpu.blocks[k];

              if (block.end < g_blockbeg || block.beg > g_blockend)
                continue;

              auto beg = max(block.beg, g_blockbeg) - g_blockbeg;
              auto end = min(block.end, g_blockend) - g_blockbeg;

              totaltime += block.end - block.beg;

              Rect2 barrect({ beg * scale, 0.0f }, { end * scale, 8.0 });

              overlay.push_rect(buildstate, timingsorigin, barrect, block.info->color);

              if (contains(Rect2(timingsorigin + barrect.min, timingsorigin + barrect.max), mousepos))
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

        overlay.push_text(buildstate, cursor + Vec2(7, font->ascent), font->height(), font, "-", ishot(Ui::Interaction::ToggleBlockTiming) ? Color4(1, 1, 0) : Color4(1, 1, 1));

        if (contains(Rect2(cursor, cursor + Vec2(15, font->lineheight())), mousepos))
          *interaction = { Ui::Interaction::ToggleBlockTiming };

        cursor += Vec2(0.0f, TimingsHeight + 4.0f);
      }
      else
      {
        char buffer[] = "Block Timing";

        overlay.push_text(buildstate, cursor + Vec2(5, font->ascent), font->height(), font, buffer, ishot(Ui::Interaction::ToggleBlockTiming) ? Color4(1, 1, 0) : Color4(1, 1, 1));

        if (contains(Rect2(cursor, cursor + Vec2(font->width(buffer), font->lineheight())), mousepos))
          *interaction = { Ui::Interaction::ToggleBlockTiming };

        cursor += Vec2(0.0f, font->lineheight() + 2);
      }

      //
      // Graphs
      //

      if (g_displayframegraph)
      {
        const float GraphHeight = 80.0f;
        const float FpsScale = GraphHeight / (1.0f/15.0f);

        overlay.push_rect(buildstate, cursor, Rect2({0.0f, 0.0f}, {viewport.width - 10.0f, GraphHeight + 5.0f}), Color4(0.0f, 0.0f, 0.0f, 0.25f));

        size_t fpswidth = viewport.width - 75;
        size_t fpsbase = max(g_fpshistorytail, fpswidth) - fpswidth;

        for(size_t i = 1, j = 1; i < fpswidth; ++i, ++j)
        {
          auto a = g_fpshistory[(fpsbase + i - 1) % extentof(g_fpshistory)] * FpsScale;
          auto b = g_fpshistory[(fpsbase + i) % extentof(g_fpshistory)] * FpsScale;

          overlay.push_line(buildstate, cursor + Vec2(j, max(GraphHeight-a, 0.0f)), cursor + Vec2(j+1, max(GraphHeight-b, 0.0f)), Color4(0.5f, 0.8f, 0.5f, 1.0f));
        }

        size_t resbase = viewport.width - 70;

        char tiptxt[128] = {};

        auto resourceslots = g_resources.resourceslotsused / (float)g_resources.resourceslotscapacity;
        overlay.push_rect(buildstate, cursor + Vec2(resbase, 0), Rect2({0, GraphHeight * (1 - resourceslots)}, {12, GraphHeight}), Color4(0.2f, 0.2f, 0.7f, 1.0f));

        if (contains(Rect2(cursor + Vec2(resbase, 0), cursor + Vec2(resbase + 12, GraphHeight)), mousepos))
        {
          snprintf(tiptxt, sizeof(tiptxt), "Resource Slots (%zu / %zu)", g_resources.resourceslotsused, g_resources.resourceslotscapacity);
        }

        auto resourcebuffer = g_resources.resourcebufferused / (float)g_resources.resourcebuffercapacity;
        overlay.push_rect(buildstate, cursor + Vec2(resbase + 15, 0), Rect2({0, GraphHeight * (1 - resourcebuffer)}, {12, GraphHeight}), Color4(0.2f, 0.2f, 0.7f, 1.0f));

        if (contains(Rect2(cursor + Vec2(resbase + 15, 0), cursor + Vec2(resbase + 27, GraphHeight)), mousepos))
        {
          snprintf(tiptxt, sizeof(tiptxt), "Resource Buffers (%zu / %zu)", g_resources.resourcebufferused, g_resources.resourcebuffercapacity);
        }

        auto renderstorage = g_resources.renderstorageused / (float)g_resources.renderstoragecapacity;
        overlay.push_rect(buildstate, cursor + Vec2(resbase + 30, 0), Rect2({0, GraphHeight * (1 - renderstorage)}, {12, GraphHeight}), Color4(0.2f, 0.2f, 0.7f, 1.0f));

        if (contains(Rect2(cursor + Vec2(resbase + 30, 0), cursor + Vec2(resbase + 42, GraphHeight)), mousepos))
        {
          snprintf(tiptxt, sizeof(tiptxt), "Render Storage (%zu / %zu)", g_resources.renderstorageused, g_resources.renderstoragecapacity);
        }

        auto renderlumps = g_resources.renderlumpsused / (float)g_resources.renderlumpscapacity;
        overlay.push_rect(buildstate, cursor + Vec2(resbase + 45, 0), Rect2({0, GraphHeight * (1 - renderlumps)}, {12, GraphHeight}), Color4(0.2f, 0.2f, 0.7f, 1.0f));

        if (contains(Rect2(cursor + Vec2(resbase + 45, 0), cursor + Vec2(resbase + 57, GraphHeight)), mousepos))
        {
          snprintf(tiptxt, sizeof(tiptxt), "Render Lumps (%zu / %zu)", g_resources.renderlumpsused, g_resources.renderlumpscapacity);
        }

        if (tiptxt[0] != 0)
        {
          overlay.push_text(buildstate, Vec2(mousepos.x - font->width(tiptxt), mousepos.y), font->height(), font, tiptxt);
        }

        overlay.push_text(buildstate, cursor + Vec2(7, font->ascent), font->height(), font, "-", ishot(Ui::Interaction::ToggleFrameGraph) ? Color4(1, 1, 0) : Color4(1, 1, 1));

        if (contains(Rect2(cursor, cursor + Vec2(15, font->lineheight())), mousepos))
          *interaction = { Ui::Interaction::ToggleFrameGraph };

        cursor += Vec2(0.0f, GraphHeight + 4.0f);
      }
      else
      {
        char buffer[] = "Frame Graph";

        overlay.push_text(buildstate, cursor + Vec2(5, font->ascent), font->height(), font, buffer, ishot(Ui::Interaction::ToggleFrameGraph) ? Color4(1, 1, 0) : Color4(1, 1, 1));

        if (contains(Rect2(cursor, cursor + Vec2(font->width(buffer), font->lineheight())), mousepos))
          *interaction = { Ui::Interaction::ToggleFrameGraph };

        cursor += Vec2(0.0f, font->lineheight() + 2);
      }

      //
      // Debug Menu
      //

      if (g_displaydebugmenu)
      {
        SyncLock M(g_menu.m_mutex);

        for(size_t i = 0; i < g_menu.groupcount; ++i)
        {
          auto &group = g_menu.groups[i];

          char buffer[128] = "";
          strncat(buffer, group.name, group.namelen);

          overlay.push_text(buildstate, cursor + Vec2(5, font->ascent), font->height(), font, buffer, ishot(Ui::Interaction::ToggleGroup, i) ? Color4(1, 1, 0) : Color4(1, 1, 1));

          if (contains(Rect2(cursor, cursor + Vec2(5 + font->width(buffer), font->lineheight())), mousepos))
            *interaction = { Ui::Interaction::ToggleGroup, i };

          cursor += Vec2(0.0f, font->lineheight() + 2);

          if (group.expanded)
          {
            for(size_t j = 0; j < group.entrycount; ++j)
            {
              size_t k = group.entries[j] - g_menu.entries;

              auto &entry = g_menu.entries[k];

              int x = 15;

              snprintf(buffer, sizeof(buffer), "%s: ", entry.label);

              overlay.push_text(buildstate, cursor + Vec2(x, font->ascent), font->height(), font, buffer);

              x += font->width(buffer);

              if (entry.type == typeid(bool))
              {
                snprintf(buffer, sizeof(buffer), "%s", entry.value<bool>() ? "true" : "false");

                overlay.push_text(buildstate, cursor + Vec2(x, font->ascent), font->height(), font, buffer, ishot(Ui::Interaction::ToggleBoolEntry, k) ? Color4(1, 1, 0) : Color4(1, 1, 1));

                if (entry.editable && contains(Rect2(cursor + Vec2(x, 0), cursor + Vec2(x + font->width(buffer), font->lineheight())), mousepos))
                  *interaction = { Ui::Interaction::ToggleBoolEntry, k };

                x += font->width(buffer);
              }

              if (entry.type == typeid(float) || entry.type == typeid(Vec2) || entry.type == typeid(Vec3) || entry.type == typeid(Color3))
              {
                snprintf(buffer, sizeof(buffer), "%f", entry.value<float[]>()[0]);

                overlay.push_text(buildstate, cursor + Vec2(x, font->ascent), font->height(), font, buffer, ishot(Ui::Interaction::SlideFloat0Entry, k) ? Color4(1, 1, 0) : Color4(1, 1, 1));

                if (entry.editable && contains(Rect2(cursor + Vec2(x, 0), cursor + Vec2(x + font->width(buffer), font->lineheight())), mousepos))
                  *interaction = { Ui::Interaction::SlideFloat0Entry, k };

                x += font->width(buffer);
              }

              if (entry.type == typeid(Vec2) || entry.type == typeid(Vec3) || entry.type == typeid(Color3))
              {
                snprintf(buffer, sizeof(buffer), " %f", entry.value<float[]>()[1]);

                overlay.push_text(buildstate, cursor + Vec2(x, font->ascent), font->height(), font, ",");
                overlay.push_text(buildstate, cursor + Vec2(x, font->ascent), font->height(), font, buffer, ishot(Ui::Interaction::SlideFloat1Entry, k) ? Color4(1, 1, 0) : Color4(1, 1, 1));

                if (entry.editable && contains(Rect2(cursor + Vec2(x, 0), cursor + Vec2(x + font->width(buffer), font->lineheight())), mousepos))
                  *interaction = { Ui::Interaction::SlideFloat1Entry, k };

                x += font->width(buffer);
              }

              if (entry.type == typeid(Vec3) || entry.type == typeid(Color3))
              {
                snprintf(buffer, sizeof(buffer), " %f", entry.value<float[]>()[2]);

                overlay.push_text(buildstate, cursor + Vec2(x, font->ascent), font->height(), font, ",");
                overlay.push_text(buildstate, cursor + Vec2(x, font->ascent), font->height(), font, buffer, ishot(Ui::Interaction::SlideFloat2Entry, k) ? Color4(1, 1, 0) : Color4(1, 1, 1));

                if (entry.editable && contains(Rect2(cursor + Vec2(x, 0), cursor + Vec2(x + font->width(buffer), font->lineheight())), mousepos))
                  *interaction = { Ui::Interaction::SlideFloat2Entry, k };

                x += font->width(buffer);
              }

              cursor += Vec2(0.0f, font->lineheight() + 2);
            }
          }
        }
      }
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

///////////////////////// DebugInfoBlock ////////////////////////////////////
DebugInfoBlock::DebugInfoBlock(const char *name, const char *filename, int linenumber, Color3 color)
  : name(name), filename(filename), linenumber(linenumber), color(color)
{
  static SpinLock mutex;

  mutex.wait();

  assert(g_infoblockcount < extentof(g_infoblocks));

  g_infoblocks[g_infoblockcount] = this;

  g_infoblockcount += 1;

  mutex.release();
}


///////////////////////// cycle_frequency /////////////////////////////////
double clock_frequency()
{
  static double frequency = 0;

  if (frequency == 0)
  {
#if 0
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
 #endif
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


///////////////////////// debug_menu_entry //////////////////////////////////
template<typename T>
void debug_menu_entry(const char *name, T const &value)
{
  SyncLock M(g_menu.m_mutex);

  assert(sizeof(T) < sizeof(Menu::Entry::value_));

  auto entry = find_if(g_menu.entries, g_menu.entries + g_menu.entrycount, [=](auto &entry) { return (entry.name == name); });

  if (entry == g_menu.entries + g_menu.entrycount)
  {
    assert(g_menu.entrycount < extentof(g_menu.entries));

    g_menu.entrycount += 1;

    entry->name = name;

    entry->label = name;

    for(size_t k = 0; name[k] != 0; ++k)
    {
      if (name[k] == '/')
      {
        entry->label = name + k + 1;

        auto group = find_if(g_menu.groups, g_menu.groups + g_menu.groupcount, [=](auto &group) { return (strncmp(group.name, name, k) == 0); });

        if (group == g_menu.groups + g_menu.groupcount)
        {
          assert(g_menu.groupcount < extentof(g_menu.groups));

          g_menu.groupcount += 1;

          group->name = name;
          group->namelen = k;
          group->entrycount = 0;
        }

        group->entries[group->entrycount++] = entry;
      }
    }
  }

  entry->type = typeid(T);

  entry->template value<T>() = value;
}

template void debug_menu_entry<bool>(const char *name, bool const &value);
template void debug_menu_entry<float>(const char *name, float const &value);
template void debug_menu_entry<Vec2>(const char *name, Vec2 const &value);
template void debug_menu_entry<Vec3>(const char *name, Vec3 const &value);
template void debug_menu_entry<Color3>(const char *name, Color3 const &value);


///////////////////////// debug_menu_value //////////////////////////////////
template<typename T>
T debug_menu_value(const char *name, T const &value, T const &min, T const &max)
{
  SyncLock M(g_menu.m_mutex);

  auto entry = find_if(g_menu.entries, g_menu.entries + g_menu.entrycount, [=](auto &entry) { return (strcmp(entry.name, name) == 0); });

  if (entry == g_menu.entries + g_menu.entrycount)
    return value;

  entry->editable = true;
  entry->template min<T>() = min;
  entry->template max<T>() = max;

  return entry->template value<T>();
}

template bool debug_menu_value<bool>(const char *name, bool const &value, bool const &min, bool const &max);
template float debug_menu_value<float>(const char *name, float const &value, float const &min, float const &max);
template Vec2 debug_menu_value<Vec2>(const char *name, Vec2 const &value, Vec2 const &min, Vec2 const &max);
template Vec3 debug_menu_value<Vec3>(const char *name, Vec3 const &value, Vec3 const &min, Vec3 const &max);
template Color3 debug_menu_value<Color3>(const char *name, Color3 const &value, Color3 const &min, Color3 const &max);


///////////////////////// update_debug_overlay //////////////////////////////
void update_debug_overlay(GameInput const &input, bool *accepted)
{
  if (input.keys[KB_KEY_F1].pressed())
  {
    g_running = !g_running;
  }

  auto interaction = g_interaction.hot.load();

  if (interaction.type != Ui::Interaction::None)
  {
    if (input.mousebuttons[GameInput::Left].pressed())
    {
      g_interaction.mousecaptured = true;
      g_interaction.mousepresspos = Vec2(input.mousex, input.mousey);
    }
  }

  if (g_interaction.mousecaptured)
  {
    if (input.mousebuttons[GameInput::Left].down())
    {
      switch(interaction.type)
      {
        case Ui::Interaction::SlideFloat0Entry:
          g_menu.entries[interaction.id].value<float[]>()[0] = clamp(g_menu.entries[interaction.id].value<float[]>()[0] + 0.5f * (g_menu.entries[interaction.id].max<float[]>()[0] - g_menu.entries[interaction.id].min<float[]>()[0]) * (input.mousex - g_interaction.mousepos.x), g_menu.entries[interaction.id].min<float[]>()[0], g_menu.entries[interaction.id].max<float[]>()[0]);
          break;

        case Ui::Interaction::SlideFloat1Entry:
          g_menu.entries[interaction.id].value<float[]>()[1] = clamp(g_menu.entries[interaction.id].value<float[]>()[1] + 0.5f * (g_menu.entries[interaction.id].max<float[]>()[1] - g_menu.entries[interaction.id].min<float[]>()[1]) * (input.mousex - g_interaction.mousepos.x), g_menu.entries[interaction.id].min<float[]>()[1], g_menu.entries[interaction.id].max<float[]>()[1]);
          break;

        case Ui::Interaction::SlideFloat2Entry:
          g_menu.entries[interaction.id].value<float[]>()[2] = clamp(g_menu.entries[interaction.id].value<float[]>()[2] + 0.5f * (g_menu.entries[interaction.id].max<float[]>()[2] - g_menu.entries[interaction.id].min<float[]>()[2]) * (input.mousex - g_interaction.mousepos.x), g_menu.entries[interaction.id].min<float[]>()[2], g_menu.entries[interaction.id].max<float[]>()[2]);
          break;

        default:
          break;
      }
    }

    if (!input.mousebuttons[GameInput::Left].down())
    {
      switch(interaction.type)
      {
        case Ui::Interaction::ToggleVisible:
          g_running = true;
          g_visible = !g_visible;
          break;

        case Ui::Interaction::ToggleBlockTiming:
          g_displayblocktiming = !g_displayblocktiming;
          break;

        case Ui::Interaction::ToggleFrameGraph:
          g_displayframegraph = !g_displayframegraph;
          break;

        case Ui::Interaction::ToggleGroup:
          g_menu.groups[interaction.id].expanded = !g_menu.groups[interaction.id].expanded;
          break;

        case Ui::Interaction::ToggleBoolEntry:
          g_menu.entries[interaction.id].value<bool>() = !g_menu.entries[interaction.id].value<bool>();
          break;

        default:
          break;
      }

      g_interaction.mousecaptured = false;
    }

    *accepted = true;
  }
  else
  {
    g_interaction.hot.store(g_interaction.nexthot);
  }

  g_interaction.mousepos = Vec2(input.mousex, input.mousey);
}


///////////////////////// render_debug_overlay //////////////////////////////
void render_debug_overlay(DatumPlatform::PlatformInterface &platform, RenderContext &context, ResourceManager *resources, PushBuffer &pushbuffer, DatumPlatform::Viewport const &viewport, Font const *font)
{
  if (g_running)
  {
    collate_debug_log();
  }

  Ui::Interaction interaction = { Ui::Interaction::None };

  if (g_running || g_visible)
  {
    push_debug_overlay(platform, context, resources, pushbuffer, viewport, font, &interaction);
  }

  g_interaction.nexthot = interaction;
}


///////////////////////// stream_debuglog ///////////////////////////////////
void stream_debuglog(const char *filename)
{
  static ofstream fout;
  static size_t lastinfo;
  static size_t lastentry;

  if (!fout.is_open())
  {
    fout.open(filename, ios_base::out | ios_base::trunc | ios_base::binary);

    assert(fout.is_open());

    DebugLogHeader header;

    fout.write((const char *)&header, sizeof(header));

    lastinfo = 0;
    lastentry = max(g_debuglogtail.load(), extentof(g_debuglog)) - extentof(g_debuglog);
  }

  char buffer[8192];

  for(size_t i = 0, end = g_infoblockcount - lastinfo; i < end; ++i, ++lastinfo)
  {
    DebugLogChunk *chunk = (DebugLogChunk *)buffer;
    chunk->type = 1;
    chunk->length = sizeof(DebugLogInfoChunk);

    DebugLogInfoChunk *infochunk = (DebugLogInfoChunk *)(buffer + sizeof(DebugLogChunk));

    infochunk->id = g_infoblocks[lastinfo];
    strncpy(infochunk->name, g_infoblocks[lastinfo]->name, sizeof(infochunk->name));
    strncpy(infochunk->filename, g_infoblocks[lastinfo]->filename, sizeof(infochunk->filename));
    infochunk->linenumber = g_infoblocks[lastinfo]->linenumber;
    infochunk->color = g_infoblocks[lastinfo]->color;

    fout.write(buffer, sizeof(DebugLogChunk) + chunk->length);
  }

  for(size_t end = g_debuglogtail; lastentry < end; )
  {
    constexpr size_t MaxEntries = (sizeof(buffer) - sizeof(DebugLogChunk) - sizeof(DebugLogEntryChunk)) / sizeof(DebugLogEntry);

    DebugLogChunk *chunk = (DebugLogChunk *)buffer;
    chunk->type = 2;
    chunk->length = sizeof(DebugLogEntryChunk);

    DebugLogEntryChunk *entrychunk = (DebugLogEntryChunk *)(buffer + sizeof(DebugLogChunk));
    entrychunk->entrycount = 0;

    for(size_t i = 0, end = min(g_debuglogtail - lastentry, MaxEntries); i < end; ++i, ++lastentry)
    {
      entrychunk->entrycount += 1;
      entrychunk->entries[i] = g_debuglog[lastentry % extentof(g_debuglog)];

      chunk->length += sizeof(DebugLogEntry);
    }

    fout.write(buffer, sizeof(DebugLogChunk) + chunk->length);
  }
}

#endif

