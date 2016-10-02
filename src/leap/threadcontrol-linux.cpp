//
// Thread and Syncronisation objects
//
//   Peter Niekamp, November, 2005
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
#include <cstring>
#include <cassert>
#include <atomic>
#include <pthread.h>
#include <sys/time.h>
#include <unistd.h>
#include <errno.h>

using namespace std;

#ifdef __ANDROID__

#define PTHREAD_ADAPTIVE_MUTEX_INITIALIZER_NP PTHREAD_MUTEX_INITIALIZER
#define PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP PTHREAD_RECURSIVE_MUTEX_INITIALIZER

static int pthread_mutex_timedlock(pthread_mutex_t *mutex, struct timespec *timeout)
{
  timeval now;
  timespec sleepytime;

  sleepytime.tv_sec = 0;
  sleepytime.tv_nsec = 10000000; /* 10ms */

  int retcode;

  while ((retcode = pthread_mutex_trylock(mutex)) == EBUSY)
  {
    gettimeofday(&now, NULL);

    if (now.tv_sec >= timeout->tv_sec && (now.tv_usec * 1000) >= timeout->tv_nsec)
      return ETIMEDOUT;

    nanosleep(&sleepytime, NULL);
  }

  return retcode;
}

static int pthread_tryjoin_np(pthread_t thid, void **ret_val)
{
  return pthread_join(thid, ret_val);
}

#endif

namespace leap { namespace threadlib
{

  //|--------------------- Mutex --------------------------------------------
  //|------------------------------------------------------------------------


  //|///////////////////// Mutex::Constructor ///////////////////////////////
  Mutex::Mutex()
  {
    m_mutex = new pthread_mutex_t(PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP);
  }


  //|///////////////////// Mutex::Destructor ////////////////////////////////
  Mutex::~Mutex()
  {
    pthread_mutex_t *mtx = static_cast<pthread_mutex_t*>(m_mutex);

    pthread_mutex_destroy(mtx);

    delete mtx;
  }


  //|///////////////////// Mutex::wait //////////////////////////////////////
  ///
  /// Wait on a Mutex.
  ///
  /// \param[in] timeout milliseconds to wait for lock (-1 = infinite)
  ///
  bool Mutex::wait(int timeout)
  {
    pthread_mutex_t *mtx = static_cast<pthread_mutex_t*>(m_mutex);

    if (timeout >= 0)
    {
      timespec abstime;
      clock_gettime(CLOCK_REALTIME, &abstime);
      abstime.tv_sec = abstime.tv_sec + (abstime.tv_nsec / 1000000 + timeout) / 1000;
      abstime.tv_nsec = (abstime.tv_nsec / 1000000 + timeout) % 1000 * 1000000;

      return (pthread_mutex_timedlock(mtx, &abstime) == 0);
    }
    else
    {
      return (pthread_mutex_lock(mtx) == 0);
    }
  }


  //|///////////////////// Mutex::release ///////////////////////////////////
  ///
  /// Release a locked Mutex
  ///
  bool Mutex::release()
  {
    return (pthread_mutex_unlock(static_cast<pthread_mutex_t*>(m_mutex)) == 0);
  }





  //|--------------------- CriticalSection ----------------------------------
  //|------------------------------------------------------------------------

  //|///////////////////// CriticalSection::Constructor /////////////////////
  CriticalSection::CriticalSection()
  {
    m_criticalsection = new pthread_mutex_t(PTHREAD_ADAPTIVE_MUTEX_INITIALIZER_NP);
  }


  //|///////////////////// CriticalSection::Constructor /////////////////////
  CriticalSection::~CriticalSection()
  {
    pthread_mutex_t *mtx = static_cast<pthread_mutex_t*>(m_criticalsection);

    pthread_mutex_destroy(mtx);

    delete mtx;
  }


  //|///////////////////// CriticalSection::wait ////////////////////////////
  ///
  /// Wait and lock Critical Section
  ///
  void CriticalSection::wait()
  {
    pthread_mutex_lock(static_cast<pthread_mutex_t*>(m_criticalsection));
  }


  //|///////////////////// CriticalSection::release /////////////////////////
  ///
  /// Release a locked Critical Section
  ///
  void CriticalSection::release()
  {
    pthread_mutex_unlock(static_cast<pthread_mutex_t*>(m_criticalsection));
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
      evt = PTHREAD_COND_INITIALIZER;
      mtx = PTHREAD_MUTEX_INITIALIZER;
    }

    ~cond_impl()
    {
      pthread_mutex_lock(&mtx);
      pthread_cond_destroy(&evt);
      pthread_mutex_unlock(&mtx);
      pthread_mutex_destroy(&mtx);
    }

    WaitType type;

    pthread_cond_t evt;
    pthread_mutex_t mtx;

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

      int retcode = 0;

      pthread_mutex_lock(&impl->mtx);

      while (impl->value == value && retcode != ETIMEDOUT)
      {
        if (timeout >= 0)
        {
          timespec abstime;
          clock_gettime(CLOCK_REALTIME, &abstime);
          abstime.tv_sec = abstime.tv_sec + (abstime.tv_nsec / 1000000 + timeout) / 1000;
          abstime.tv_nsec = (abstime.tv_nsec / 1000000 + timeout) % 1000 * 1000000;

          retcode = pthread_cond_timedwait(&impl->evt, &impl->mtx, &abstime);
        }
        else
        {
          retcode = pthread_cond_wait(&impl->evt, &impl->mtx);
        }
      }

      pthread_mutex_unlock(&impl->mtx);

      if (retcode == ETIMEDOUT)
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

    pthread_mutex_lock(&impl->mtx);

    impl->value.store(1);

    pthread_cond_broadcast(&impl->evt);

    for(auto &group : impl->groups)
      group->set();

    pthread_mutex_unlock(&impl->mtx);

    return true;
  }


  //|///////////////////// Event::reset /////////////////////////////////////
  ///
  /// Set the Event state to un-signaled
  ///
  bool Event::reset()
  {
    cond_impl *impl = static_cast<cond_impl*>(m_handle);

    impl->value.store(0);

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

    pthread_mutex_lock(&impl->mtx);

    int value = impl->value.load(std::memory_order_relaxed);

    while (!impl->value.compare_exchange_weak(value, value - count))
      ;

    if (value == count)
    {
      pthread_cond_broadcast(&impl->evt);

      for(auto &group : impl->groups)
        group->set();
    }

    pthread_mutex_unlock(&impl->mtx);

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

    pthread_mutex_lock(&impl->mtx);

    int value = impl->value.load(std::memory_order_relaxed);

    while (!impl->value.compare_exchange_weak(value, clamp(value + count, value, m_maxcount)))
      ;

    for(int i = 0; i < count; ++i)
      pthread_cond_signal(&impl->evt);

    for(auto &group : impl->groups)
      group->set();

    pthread_mutex_unlock(&impl->mtx);

    return true;
  }




  //|--------------------- WaitGroup ----------------------------------------
  //|------------------------------------------------------------------------

  struct group_impl
  {
    Event evt;

    vector<Waitable*> events;
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

    pthread_mutex_lock(&cond->mtx);

    cond->groups.push_back(&impl->evt);

    pthread_mutex_unlock(&cond->mtx);
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

    pthread_mutex_lock(&cond->mtx);

    cond->groups.erase(find(cond->groups.begin(), cond->groups.end(), &impl->evt));

    pthread_mutex_unlock(&cond->mtx);
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

    while (true)
    {
      impl->evt.reset();

      if (any_of(impl->events.begin(), impl->events.end(), [](Waitable *i) -> bool { return *i; }))
        return true;

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
  bool ThreadControl::create_thread(Thread address, void *parameter, Priority /*priority*/)
  {
    SyncLock M(m_mutex);

    pthread_t handle;
    if (pthread_create(&handle, NULL, (void*(*)(void*))address, parameter) != 0)
      return false;

  //  pthread_setschedparam(handle, priority);

    m_threads.push_back((void*)handle);

    return true;
  }


  //|///////////////////// ThreadControl::join_threads //////////////////////
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
        if (pthread_tryjoin_np((pthread_t)m_threads[i], NULL) == 0)
        {
          // tidy up...

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
    usleep(timeout*1000);
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
    sched_yield();
  }

} } // namespace
