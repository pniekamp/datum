//
// datumtest.cpp
//

#include "platform.h"
#include "datumtest.h"
#include "leap/pathstring.h"
#include <windows.h>
#include <iostream>

using namespace std;
using namespace leap;
using namespace DatumPlatform;


//|---------------------- Platform ------------------------------------------
//|--------------------------------------------------------------------------

class Platform : public PlatformInterface
{
  public:

    Platform();

    void initialise(size_t gamememorysize);

  public:

    // data access

    virtual handle_t open_handle(const char *identifier) override;

    virtual void read_handle(handle_t handle, uint64_t position, void *buffer, std::size_t n) override;

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

    WorkQueue m_workqueue;
};


///////////////////////// Platform::Constructor /////////////////////////////
Platform::Platform()
{
  m_terminaterequested = false;
}


///////////////////////// Platform::initialise //////////////////////////////
void Platform::initialise(std::size_t gamememorysize)
{
  m_gamememory.reserve(gamememorysize);
  m_gamescratchmemory.reserve(256*1024*1024);
  m_renderscratchmemory.reserve(256*1024*1024);

  gamememory_initialise(gamememory, m_gamememory.data(), m_gamememory.capacity());

  gamememory_initialise(gamescratchmemory, m_gamescratchmemory.data(), m_gamescratchmemory.capacity());

  gamememory_initialise(renderscratchmemory, m_renderscratchmemory.data(), m_renderscratchmemory.capacity());
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

    void init();

    void update(float dt);

    void render(int x, int y, int width, int height);

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
void Game::init()
{
  game_init = datumtest_init;
  game_update = datumtest_update;
  game_render = datumtest_render;

  if (!game_init || !game_update || !game_render)
    throw std::runtime_error("Unable to init game code");

  m_platform.initialise(1*1024*1024*1024);

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
void Game::render(int x, int y, int width, int height)
{
  m_platform.renderscratchmemory.size = 0;

  game_render(m_platform, { x, y, width, height });

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


//|---------------------- Window --------------------------------------------
//|--------------------------------------------------------------------------

struct Window
{
  void init(HINSTANCE hinstance, Game *gameptr);

  int width = 960;
  int height = 540;

  Game *game;

  HWND hwnd;

} window;


LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  switch (uMsg)
  {
    case WM_CLOSE:
      window.game->terminate();
      break;

    case WM_PAINT:
      window.game->render(0, 0, window.width, window.height);
      break;

    case WM_SIZE:
      window.width = lParam & 0xffff;
      window.height = lParam & 0xffff0000 >> 16;
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

  WNDCLASSEX winclass;
  winclass.cbSize = sizeof(WNDCLASSEX);
  winclass.style = CS_HREDRAW | CS_VREDRAW;
  winclass.lpfnWndProc = WndProc;
  winclass.cbClsExtra = 0;
  winclass.cbWndExtra = 0;
  winclass.hInstance = hinstance;
  winclass.hIcon = LoadIcon(NULL, IDI_APPLICATION);
  winclass.hCursor = LoadCursor(NULL, IDC_ARROW);
  winclass.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
  winclass.lpszMenuName = NULL;
  winclass.lpszClassName = "DatumTest";
  winclass.hIconSm = LoadIcon(NULL, IDI_WINLOGO);

  if (!RegisterClassEx(&winclass))
    throw runtime_error("Error registering window class");

  RECT rect = { 0, 0, width, height };
  AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);

  hwnd = CreateWindowEx(0, "DatumTest", "Datum Test", WS_OVERLAPPEDWINDOW | WS_VISIBLE | WS_SYSMENU, 100, 100, rect.right - rect.left, rect.bottom - rect.top, NULL, NULL, hinstance, NULL);

  if (!hwnd)
    throw runtime_error("Error creating window");
}


//|---------------------- main ----------------------------------------------
//|--------------------------------------------------------------------------

int main(int argc, char *args[])
{
  cout << "Datum Test" << endl;

  try
  {
    Game game;

    game.init();

    window.init(GetModuleHandle(NULL), &game);

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
      MSG msg;

      if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
      {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
      }

      RedrawWindow(window.hwnd, NULL, NULL, RDW_INTERNALPAINT);
    }

    updatethread.join();
  }
  catch(const exception &e)
  {
    cout << "Critical Error: " << e.what() << endl;
  }
}
