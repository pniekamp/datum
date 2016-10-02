//
// Thread and Syncronisation objects
//
//   Peter Niekamp, September, 2000
//

//
// Copyright (C) Peter Niekamp
//
// This code remains the property of the copyright holder.
// The code contained herein may be used for any purpose provided
// this copyright notice is retained
//

#include "leap/threadcontrol.h"
#include "leap/util.h"
#include <algorithm>
#include <windows.h>

using namespace std;

namespace leap { namespace threadlib
{

  //|--------------------- Mutex --------------------------------------------
  //|------------------------------------------------------------------------


  //|///////////////////// Mutex::Constructor ///////////////////////////////
  Mutex::Mutex()
  {
    m_mutex = CreateMutex(NULL, FALSE, NULL);
  }


  //|///////////////////// Mutex::Destructor ////////////////////////////////
  Mutex::~Mutex()
  {
    if (m_mutex != NULL)
      CloseHandle(m_mutex);
  }


  //|///////////////////// Mutex::wait //////////////////////////////////////
  ///
  /// Wait on a Mutex.
  ///
  /// \param[in] timeout milliseconds to wait for lock (-1 = infinite)
  ///
  bool Mutex::wait(int timeout)
  {
    return (WaitForSingleObject(m_mutex, timeout) == WAIT_OBJECT_0);
  }


  //|///////////////////// Mutex::release ///////////////////////////////////
  ///
  /// Release a locked Mutex
  ///
  bool Mutex::release()
  {
    ReleaseMutex(m_mutex);

    return true;
  }





  //|--------------------- CriticalSection ----------------------------------
  //|------------------------------------------------------------------------

  //|///////////////////// CriticalSection::Constructor /////////////////////
  CriticalSection::CriticalSection()
  {
    m_criticalsection = new CRITICAL_SECTION;

    InitializeCriticalSection(static_cast<CRITICAL_SECTION*>(m_criticalsection));
  }


  //|///////////////////// CriticalSection::Constructor /////////////////////
  CriticalSection::~CriticalSection()
  {
    CRITICAL_SECTION *mtx = static_cast<CRITICAL_SECTION*>(m_criticalsection);

    DeleteCriticalSection(mtx);

    delete mtx;
  }


  //|///////////////////// CriticalSection::wait ////////////////////////////
  ///
  /// Wait and lock Critical Section
  ///
  void CriticalSection::wait()
  {
    EnterCriticalSection(static_cast<CRITICAL_SECTION*>(m_criticalsection));
  }


  //|///////////////////// CriticalSection::release /////////////////////////
  ///
  /// Release a locked Critical Section
  ///
  void CriticalSection::release()
  {
    LeaveCriticalSection(static_cast<CRITICAL_SECTION*>(m_criticalsection));
  }



  //|--------------------- Waitable -----------------------------------------
  //|------------------------------------------------------------------------

  struct cond_impl
  {
    enum class WaitType
    {
      Event,
      Latch,
      Semaphore
    };

    cond_impl(WaitType type)
      : type(type),
        value(0)
    {
      if (type == WaitType::Event)
      {
        evt = CreateEvent(NULL, TRUE, FALSE, NULL);
      }

      if (type == WaitType::Latch)
      {
        evt = CreateEvent(NULL, TRUE, FALSE, NULL);
      }

      if (type == WaitType::Semaphore)
      {
        evt = CreateSemaphore(NULL, 0, std::numeric_limits<int>::max(), NULL);
      }

      InitializeCriticalSection(&mtx);
    }

    ~cond_impl()
    {
      EnterCriticalSection(&mtx);
      CloseHandle(evt);
      LeaveCriticalSection(&mtx);
      DeleteCriticalSection(&mtx);
    }

    WaitType type;

    HANDLE evt;
    CRITICAL_SECTION mtx;

    std::atomic<int> value;

    std::vector<Event*> groups;
  };


  //|///////////////////// Waitable::wait ///////////////////////////////////
  ///
  /// Waits for a signal
  ///
  /// \param[in] timeout milliseconds to wait for lock (-1 = infinite)
  ///
  bool Waitable::wait(int timeout)
  {
    cond_impl *impl = static_cast<cond_impl*>(m_handle);

    while (true)
    {
      int value = impl->value.load(std::memory_order_relaxed);

      if (impl->type == cond_impl::WaitType::Event)
      {
        if (value > 0)
          return true;
      }

      if (impl->type == cond_impl::WaitType::Latch)
      {
        if (value == 0)
          return true;
      }

      if (impl->type == cond_impl::WaitType::Semaphore)
      {
        while (value > 0)
        {
          if (impl->value.compare_exchange_weak(value, value - 1))
            return true;
        }
      }

      if (timeout == 0)
        return false;

      if (WaitForSingleObject(impl->evt, timeout) == WAIT_TIMEOUT)
        return false;
    }
  }



  //|--------------------- Event --------------------------------------------
  //|------------------------------------------------------------------------


  //|///////////////////// Event::Constructor ///////////////////////////////
  Event::Event()
  {
    m_handle = new cond_impl(cond_impl::WaitType::Event);
  }


  //|///////////////////// Event::Destructor ////////////////////////////////
  Event::~Event()
  {
    delete static_cast<cond_impl*>(m_handle);
  }


  //|///////////////////// Event::set ///////////////////////////////////////
  ///
  /// Set the Event state to signaled
  ///
  bool Event::set()
  {
    cond_impl *impl = static_cast<cond_impl*>(m_handle);

    EnterCriticalSection(&impl->mtx);

    impl->value.store(1);

    SetEvent(impl->evt);

    for(auto &group : impl->groups)
      group->set();

    LeaveCriticalSection(&impl->mtx);

    return true;
  }


  //|///////////////////// Event::reset /////////////////////////////////////
  ///
  /// Set the Event state to un-signaled
  ///
  bool Event::reset()
  {
    cond_impl *impl = static_cast<cond_impl*>(m_handle);

    EnterCriticalSection(&impl->mtx);

    impl->value.store(0);

    ResetEvent(impl->evt);

    LeaveCriticalSection(&impl->mtx);

    return true;
  }




  //|--------------------- Latch --------------------------------------------
  //|------------------------------------------------------------------------


  //|///////////////////// Latch::Constructor ///////////////////////////////
  Latch::Latch(int count)
  {
    m_handle = new cond_impl(cond_impl::WaitType::Latch);

    static_cast<cond_impl*>(m_handle)->value.store(count);
  }


  //|///////////////////// Latch::Destructor ////////////////////////////////
  Latch::~Latch()
  {
    delete static_cast<cond_impl*>(m_handle);
  }


  //|///////////////////// Latch::release ///////////////////////////////////
  ///
  /// Releases a Latch slot
  ///
  bool Latch::release(int count)
  {
    cond_impl *impl = static_cast<cond_impl*>(m_handle);

    EnterCriticalSection(&impl->mtx);

    int value = impl->value.load(std::memory_order_relaxed);

    while (!impl->value.compare_exchange_weak(value, value - count))
      ;

    if (value == count)
    {
      SetEvent(impl->evt);

      for(auto &group : impl->groups)
        group->set();
    }

    LeaveCriticalSection(&impl->mtx);

    return true;
  }



  //|--------------------- Semaphore ----------------------------------------
  //|------------------------------------------------------------------------


  //|///////////////////// Semaphore::Constructor ///////////////////////////
  Semaphore::Semaphore(int maxcount)
    : m_maxcount(maxcount)
  {
    m_handle = new cond_impl(cond_impl::WaitType::Semaphore);
  }


  //|///////////////////// Semaphore::Destructor ////////////////////////////
  Semaphore::~Semaphore()
  {
    delete static_cast<cond_impl*>(m_handle);
  }


  //|///////////////////// Semaphore::release ///////////////////////////////
  ///
  /// Releases a Semaphore slot
  ///
  bool Semaphore::release(int count)
  {
    cond_impl *impl = static_cast<cond_impl*>(m_handle);

    EnterCriticalSection(&impl->mtx);

    int value = impl->value.load(std::memory_order_relaxed);

    while (!impl->value.compare_exchange_weak(value, clamp(value + count, value, m_maxcount)))
      ;

    ReleaseSemaphore(impl->evt, count, NULL);

    for(auto &group : impl->groups)
      group->set();

    LeaveCriticalSection(&impl->mtx);

    return true;
  }




  //|--------------------- WaitGroup ----------------------------------------
  //|------------------------------------------------------------------------

  struct group_impl
  {
    Event evt;

    vector<Waitable*> events;

    vector<HANDLE> handles;
  };


  //|///////////////////// WaitGroup::WaitGroup /////////////////////////////
  WaitGroup::WaitGroup()
    : m_events(new group_impl)
  {
  }


  //|///////////////////// WaitGroup::Destructor ////////////////////////////
  WaitGroup::~WaitGroup()
  {
    group_impl *impl = static_cast<group_impl*>(m_events);

    while (impl->events.size() != 0)
      remove(*impl->events.front());

    delete static_cast<group_impl*>(m_events);
  }


  //|///////////////////// WaitGroup::size //////////////////////////////////
  size_t WaitGroup::size() const
  {
    return static_cast<group_impl*>(m_events)->events.size();
  }


  //|///////////////////// WaitGroup::add ///////////////////////////////////
  ///
  /// Add Event to Group
  ///
  void WaitGroup::add(Waitable &event)
  {
    group_impl *impl = static_cast<group_impl*>(m_events);

    impl->events.push_back(&event);

    cond_impl *cond = static_cast<cond_impl*>(event.m_handle);

    impl->handles.push_back(cond->evt);

    EnterCriticalSection(&cond->mtx);

    cond->groups.push_back(&impl->evt);

    LeaveCriticalSection(&cond->mtx);
  }


  //|///////////////////// WaitGroup::remove ////////////////////////////////
  ///
  /// Remove Event from Group
  ///
  void WaitGroup::remove(Waitable &event)
  {
    group_impl *impl = static_cast<group_impl*>(m_events);

    impl->events.erase(find(impl->events.begin(), impl->events.end(), &event));

    cond_impl *cond = static_cast<cond_impl*>(event.m_handle);

    impl->handles.erase(find(impl->handles.begin(), impl->handles.end(), cond->evt));

    EnterCriticalSection(&cond->mtx);

    cond->groups.erase(find(cond->groups.begin(), cond->groups.end(), &impl->evt));

    LeaveCriticalSection(&cond->mtx);
  }


  //|///////////////////// WaitGroup::wait_any //////////////////////////////
  ///
  /// Wait for any one Event to become signaled
  ///
  /// \param[in] timeout Period to wait for Events to become signaled
  ///
  bool WaitGroup::wait_any(int timeout)
  {
    group_impl *impl = static_cast<group_impl*>(m_events);

  //  if (impl->handles.size() > MAXIMUM_WAIT_OBJECTS)

    while (true)
    {
      impl->evt.reset();

      if (any_of(impl->events.begin(), impl->events.end(), [](Waitable *i) -> bool { return *i; }))
        return true;

  //    if (WaitForMultipleObjects(impl->handles.size(), impl->handles.data(), FALSE, timeout) == WAIT_TIMEOUT)
  //      return false;

      if (!impl->evt.wait(timeout))
        return false;
    }
  }


  //|///////////////////// WaitGroup::wait_all //////////////////////////////
  ///
  /// Wait for all Events to become signaled
  ///
  /// \param[in] timeout Period to wait for Events to become signaled
  ///
  bool WaitGroup::wait_all(int timeout)
  {
    group_impl *impl = static_cast<group_impl*>(m_events);

    while (true)
    {
      impl->evt.reset();

      if (all_of(impl->events.begin(), impl->events.end(), [](Waitable *i) -> bool { return *i; }))
        return true;

  //    if (WaitForMultipleObjects(impl->handles.size(), impl->handles.data(), TRUE, timeout) == WAIT_TIMEOUT)
  //      return false;

      if (!impl->evt.wait(timeout))
        return false;
    }
  }




  //|--------------------- ReadWriteLock ------------------------------------
  //|------------------------------------------------------------------------


  //|///////////////////// ReadWriteLock::Constructor ///////////////////////
  ///
  /// \param[in] maxreaders Maximum number or simultaneous readers
  ///
  ReadWriteLock::ReadWriteLock(int maxreaders)
    : m_maxreaders(maxreaders),
      m_semaphore(m_maxreaders)
  {
    m_semaphore.release(m_maxreaders);
  }


  //|///////////////////// ReadWriteLock::Destructor ////////////////////////
  ReadWriteLock::~ReadWriteLock()
  {
  }


  //|///////////////////// ReadWriteLock::readwait //////////////////////////
  ///
  /// Wait on a Read Lock
  ///
  /// \param[in] timeout milliseconds to wait for lock (-1 = infinite)
  ///
  bool ReadWriteLock::readwait(int timeout)
  {
    return m_semaphore.wait(timeout);
  }


  //|///////////////////// ReadWriteLock::readrelease ///////////////////////
  ///
  /// Release a Read Lock
  ///
  bool ReadWriteLock::readrelease()
  {
    return m_semaphore.release();
  }


  //|///////////////////// ReadWriteLock::writewait /////////////////////////
  ///
  /// Wait on a Write Lock
  ///
  /// \param[in] timeout milliseconds to wait for lock (-1 = infinite)
  ///
  bool ReadWriteLock::writewait(int timeout)
  {
    SyncLock M(m_mutex);

    for(int i = 0; i < m_maxreaders; ++i)
    {
      if (!m_semaphore.wait(timeout))
      {
        // Failed... unlock what we have locked
        m_semaphore.release(i);
        return false;
      }
    }

    return true;
  }


  //|///////////////////// ReadWriteLock::writerelease //////////////////////
  ///
  /// Release a Write Lock
  ///
  bool ReadWriteLock::writerelease()
  {
    return m_semaphore.release(m_maxreaders);
  }





  //|--------------------- ReaderSyncLock -----------------------------------
  //|------------------------------------------------------------------------


  //|///////////////////// ReaderSyncLock::Constructor //////////////////////
  ///
  /// Scope Based lock/unlock of a ReadWriteLock object
  ///
  /// \param[in] lock The ReadWriteLock object to read-lock
  ///
  ReaderSyncLock::ReaderSyncLock(ReadWriteLock &lock)
  {
    m_readwritelock = NULL;

    if (lock.readwait())
      m_readwritelock = &lock;
  }



  //|///////////////////// ReaderSyncLock::Destructor ///////////////////////
  ReaderSyncLock::~ReaderSyncLock()
  {
    if (m_readwritelock != NULL)
      m_readwritelock->readrelease();
  }




  //|--------------------- WriterSyncLock -----------------------------------
  //|------------------------------------------------------------------------


  //|///////////////////// WriterSyncLock::Constructor //////////////////////
  ///
  /// Scope Based lock/unlock of a ReadWriteLock object
  ///
  /// \param[in] lock The ReadWriteLock object to write-lock
  //|
  WriterSyncLock::WriterSyncLock(ReadWriteLock &lock)
  {
    m_readwritelock = NULL;

    if (lock.writewait())
      m_readwritelock = &lock;
  }



  //|///////////////////// WriterSyncLock::Destructor ///////////////////////
  WriterSyncLock::~WriterSyncLock()
  {
    if (m_readwritelock != NULL)
      m_readwritelock->writerelease();
  }





  //|--------------------- ThreadControl ------------------------------------
  //|------------------------------------------------------------------------


  //|///////////////////// ThreadControl::Constructor ///////////////////////
  ThreadControl::ThreadControl()
  {
  }


  //|///////////////////// ThreadControl::Destructor ////////////////////////
  ThreadControl::~ThreadControl()
  {
    if (m_threads.size() != 0)
      std::terminate();
  }


  //|///////////////////// ThreadControl::create_thread /////////////////////
  ///
  /// Creates a new thread and maintains it in the thread list
  ///
  /// \param[in] address The Start Address for the new thread
  /// \param[in] parameter A value to be passed to the new thread
  ///
  bool ThreadControl::create_thread(Thread address, void *parameter, Priority priority)
  {
    SyncLock M(m_mutex);

    DWORD ThreadID;

    HANDLE handle = ::CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)address, parameter, 0, &ThreadID);
    if (handle == NULL)
      return false;

    switch (priority)
    {
      case Priority::Idle :
        SetThreadPriority(handle, THREAD_PRIORITY_IDLE);
        break;

      case Priority::Lowest :
        SetThreadPriority(handle, THREAD_PRIORITY_LOWEST);
        break;

      case Priority::BelowNormal :
        SetThreadPriority(handle, THREAD_PRIORITY_BELOW_NORMAL);
        break;

      case Priority::Normal :
        SetThreadPriority(handle, THREAD_PRIORITY_NORMAL);
        break;

      case Priority::AboveNormal :
        SetThreadPriority(handle, THREAD_PRIORITY_ABOVE_NORMAL);
        break;

      case Priority::Highest :
        SetThreadPriority(handle, THREAD_PRIORITY_HIGHEST);
        break;

      case Priority::RealTime :
        SetThreadPriority(handle, THREAD_PRIORITY_TIME_CRITICAL);
        break;
    }

    m_threads.push_back(handle);

    return true;
  }


  //|///////////////////// ThreadControl::join //////////////////////////////
  ///
  /// Requests Termination from all threads under ThreadControl
  ///
  /// \param[in] timeout Period to wait for thread to respond (milliseconds)
  ///
  bool ThreadControl::join_threads(int timeout)
  {
    m_terminate.set();

    //
    // check if the thread has exited (give it timeout period to do so)
    //

    while (timeout != 0 && m_threads.size() != 0)
    {
      SyncLock M(m_mutex);

      for(int i = m_threads.size() - 1; i >= 0; --i)
      {
        DWORD exitcode;

        GetExitCodeThread(m_threads[i], &exitcode);

        if (exitcode != STILL_ACTIVE)
        {
          // tidy up...

          CloseHandle(m_threads[i]);

          m_threads.erase(m_threads.begin()+i);
        }
      }

      sleep_for(100);
      timeout -= timeout < 0 ? 0 : min(100, timeout);
    }

    m_terminate.reset();

    return (m_threads.size() == 0);
  }


  //-------------------------- Functions ------------------------------------
  //-------------------------------------------------------------------------

  //|///////////////////////// sleep_for ////////////////////////////////////
  void sleep_for(int timeout)
  {
    Sleep(timeout);
  }


  //|///////////////////////// sleep_til ////////////////////////////////////
  void sleep_til(Waitable &event, int timeout)
  {
    event.wait(timeout);
  }


  //|///////////////////////// sleep_any ////////////////////////////////////
  void sleep_any(WaitGroup &group, int timeout)
  {
    group.wait_any(timeout);
  }


  //|///////////////////////// sleep_all ////////////////////////////////////
  void sleep_all(WaitGroup &group, int timeout)
  {
    group.wait_all(timeout);
  }


  //|///////////////////////// sleep_yield //////////////////////////////////
  void sleep_yield()
  {
    Sleep(0);
  }

} } // namespace
