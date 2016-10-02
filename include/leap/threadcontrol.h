//
// Classes for controling Thread and Synchronisation objects
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

#ifndef THREADCONTROL_HH
#define THREADCONTROL_HH

#include <vector>
#include <memory>
#include <limits>
#include <atomic>

/**
 * \namespace leap::threadlib
 * \brief Theading Primitives and Operations
 *
**/

/**
 * \defgroup leapthread Thread and Signal Handling
 * \brief Thread and Signal Handling
 *
**/

namespace leap { namespace threadlib
{

  //|------------------------- Mutex ------------------------------------------
  //|--------------------------------------------------------------------------
  /**
   * \ingroup leapthread
   *
   * \brief Mutex Object
   *
   * Mutex (Mutual Exclusion) are resursive, lockable again by the owning thread.
   *
  **/

  class Mutex
  {
    public:
      Mutex();
      Mutex(Mutex const &) = delete;
      Mutex(Mutex &&) = delete;
      ~Mutex();

      bool wait(int timeout = -1);
      bool release();

      void *m_mutex;
  };



  //|------------------------- CriticalSection --------------------------------
  //|--------------------------------------------------------------------------
  /**
   * \ingroup leapthread
   *
   * \brief Critical Section Object
   *
   * Critical Sections are fast, non-recursive, blocking mutexs
   *
  **/

  class CriticalSection
  {
    public:
      CriticalSection();
      CriticalSection(CriticalSection const &) = delete;
      CriticalSection(CriticalSection &&) = delete;
      ~CriticalSection();

      void wait();
      void release();

      void *m_criticalsection;
  };



  //|------------------------- SpinLock ---------------------------------------
  //|--------------------------------------------------------------------------
  /**
   * \ingroup leapthread
   *
   * \brief Spin Lock Object
   *
   * Spin Lock fast blocking mutex
   *
  **/

  class SpinLock
  {
    public:
      SpinLock() = default;
      SpinLock(SpinLock const &) = delete;
      SpinLock(SpinLock &&) = delete;

      void wait();
      void release();

      std::atomic_flag m_lock = ATOMIC_FLAG_INIT;
  };

  inline void SpinLock::wait()
  {
    while (m_lock.test_and_set(std::memory_order_acquire))
      ;
  }

  inline void SpinLock::release()
  {
    m_lock.clear(std::memory_order_release);
  }



  //|------------------------- SyncLock ---------------------------------------
  //|--------------------------------------------------------------------------
  /**
   * \ingroup leapthread
   *
   * \brief Scope Baseed Locking Helper
   *
  **/

  class SyncLock
  {
    public:
      SyncLock(Mutex &lock);
      SyncLock(CriticalSection &lock);
      SyncLock(SpinLock &lock);
      SyncLock(SyncLock const &) = delete;
      SyncLock(SyncLock &&) = delete;
      ~SyncLock();

    private:

      enum { mutex, criticalsection, spinlock };

      int const m_type;

      void *m_lock;
  };

  inline SyncLock::SyncLock(Mutex &lock)
    : m_type(mutex), m_lock(&lock)
  {
    lock.wait();
  }

  inline SyncLock::SyncLock(CriticalSection &lock)
    : m_type(criticalsection), m_lock(&lock)
  {
    lock.wait();
  }

  inline SyncLock::SyncLock(SpinLock &lock)
    : m_type(spinlock), m_lock(&lock)
  {
    lock.wait();
  }

  inline SyncLock::~SyncLock()
  {
    switch (m_type)
    {
      case mutex:
        static_cast<Mutex*>(m_lock)->release();
        break;

      case criticalsection:
        static_cast<CriticalSection*>(m_lock)->release();
        break;

      case spinlock:
        static_cast<SpinLock*>(m_lock)->release();
        break;
    }
  }



  //|------------------------- Waitable ---------------------------------------
  //|--------------------------------------------------------------------------
  /**
   * \ingroup leapthread
   *
   * \brief Waitable
   *
  **/

  class Waitable
  {
    public:

      bool wait(int timeout = -1);

      operator bool() { return wait(0); }

      void *m_handle;

    protected:
      Waitable() = default;
  };


  //|------------------------- Event ------------------------------------------
  //|--------------------------------------------------------------------------
  /**
   * \ingroup leapthread
   *
   * \brief Event
   *
  **/

  class Event : public Waitable
  {
    public:
      Event();
      Event(Event const &) = delete;
      Event(Event &&) = delete;
      ~Event();

      bool set();
      bool reset();
  };


  //|------------------------- Latch ------------------------------------------
  //|--------------------------------------------------------------------------
  /**
   * \ingroup leapthread
   *
   * \brief Latch
   *
  **/

  class Latch : public Waitable
  {
    public:
      Latch(int count);
      Latch(Latch const &) = delete;
      Latch(Latch &&) = delete;
      ~Latch();

      bool release(int count = 1);
  };


  //|------------------------- Semaphore --------------------------------------
  //|--------------------------------------------------------------------------
  /**
   * \ingroup leapthread
   *
   * \brief Semaphore
   *
  **/

  class Semaphore : public Waitable
  {
    public:
      Semaphore(int maxcount);
      Semaphore(Semaphore const &) = delete;
      Semaphore(Semaphore &&) = delete;
      ~Semaphore();

      bool release(int count = 1);

      int m_maxcount;
  };



  //|------------------------- WaitGroup --------------------------------------
  //|--------------------------------------------------------------------------
  /**
   * \ingroup leapthread
   *
   * \brief Wait Group
   *
  **/

  class WaitGroup
  {
    public:
      WaitGroup();
      WaitGroup(WaitGroup const &) = delete;
      WaitGroup(WaitGroup &&) = delete;
      ~WaitGroup();

      size_t size() const;

      void add(Waitable &event);
      void remove(Waitable &event);

      bool wait_any(int timeout = -1);
      bool wait_all(int timeout = -1);

      void *m_events;
  };



  //|------------------------- ReadWriteLock ----------------------------------
  //|--------------------------------------------------------------------------
  /**
   * \ingroup leapthread
   *
   * \brief Read Write Lock
   *
  **/

  class ReadWriteLock
  {
    public:
      ReadWriteLock(int maxreaders = 32);
      ~ReadWriteLock();

      bool readwait(int timeout = -1);
      bool readrelease();
      bool writewait(int timeout = -1);
      bool writerelease();

    private:

      int m_maxreaders;

      Mutex m_mutex;
      Semaphore m_semaphore;
  };



  //|------------------------- ReaderSyncLock ---------------------------------
  //|--------------------------------------------------------------------------
  /**
   * \ingroup leapthread
   *
   * \brief Reader Scope Baseed Locking Helper
   *
  **/

  class ReaderSyncLock
  {
    public:
      ReaderSyncLock(ReadWriteLock &lock);
      ReaderSyncLock(ReaderSyncLock const &) = delete;
      ReaderSyncLock(ReaderSyncLock &&) = delete;
      ~ReaderSyncLock();

    private:

      ReadWriteLock *m_readwritelock;
  };



  //|------------------------- WriterSyncLock ---------------------------------
  //|--------------------------------------------------------------------------
  /**
   * \ingroup leapthread
   *
   * \brief Writer Scope Baseed Locking Helper
   *
  **/

  class WriterSyncLock
  {
    public:
      WriterSyncLock(ReadWriteLock &lock);
      WriterSyncLock(WriterSyncLock const &) = delete;
      WriterSyncLock(WriterSyncLock &&) = delete;
      ~WriterSyncLock();

    private:

      ReadWriteLock *m_readwritelock;
  };



  //|------------------------- ThreadControl ----------------------------------
  //|--------------------------------------------------------------------------
  /**
   * \ingroup leapthread
   *
   * \brief Thread Control Operations
   *
   * Thread Maintenance Class (Creation, Lifetime and Termination)
   *
  **/

  class ThreadControl
  {
    public:

      typedef long (*Thread)(void *);

      enum class Priority
      {
        Idle,
        Lowest,
        BelowNormal,
        Normal,
        AboveNormal,
        Highest,
        RealTime
      };

    public:
      ThreadControl();
      ~ThreadControl();

      bool create_thread(Thread address, void *parameter, Priority priority = Priority::Normal);

      bool join_threads(int timeout = -1);

    public:

      Waitable &terminate() { return m_terminate; }

      bool terminating() { return m_terminate; }

    private:

      std::vector<void*> m_threads;

      Event m_terminate;
      CriticalSection m_mutex;

    private:

      //-------------------------- MemberThreads -------------------------
      //------------------------------------------------------------------

      template<typename Function>
      struct Helper
      {
        Function func;

        static long Thread(void *data)
        {
          std::unique_ptr<Helper<Function>> helper(reinterpret_cast<Helper*>(data));

          return helper->func();
        }
      };

    public:

      template<typename Function>
      bool create_thread(Function func, Priority priority = Priority::Normal)
      {
        Helper<Function> *data = new Helper<Function>{ func };

        return create_thread(&Helper<Function>::Thread, data, priority);
      }

      template<typename Object>
      bool create_thread(Object *object, long (Object::*thread)(), Priority priority = Priority::Normal)
      {
        return create_thread([=]() { return (object->*thread)(); }, priority);
      }
  };


  //-------------------------- Functions -------------------------------------
  //--------------------------------------------------------------------------

  //////////////////////////// sleep /////////////////////////////////////////
  void sleep_for(int timeout);
  void sleep_til(Waitable &event, int timeout = -1);
  void sleep_any(WaitGroup &group, int timeout = -1);
  void sleep_all(WaitGroup &group, int timeout = -1);
  void sleep_yield();

} } // namespace tc

#endif // THREADCONTROL_HH
