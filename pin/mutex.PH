#ifndef PIN_MUTEX_H
#define PIN_MUTEX_H

// #include "lock.PH"

class mutex {
  PIN_MUTEX _mu;

public:
  mutex() { PIN_MutexInit(&_mu); }
  ~mutex() { PIN_MutexFini(&_mu); }
  void lock() { PIN_MutexLock(&_mu); }
  void unlock() { PIN_MutexUnlock(&_mu); }
};

class shared_mutex {
  PIN_RWMUTEX _mu;

public:
  shared_mutex() { PIN_RWMutexInit(&_mu); }
  ~shared_mutex() { PIN_RWMutexFini(&_mu); }
  void lock() { PIN_RWMutexWriteLock(&_mu); }
  void unlock() { PIN_RWMutexUnlock(&_mu); }
  void lock_shared() { PIN_RWMutexReadLock(&_mu); }
  void unlock_shared() { PIN_RWMutexUnlock(&_mu); }
};

class shared_lock {
  shared_mutex &_mu;

public:
  shared_lock(shared_mutex &mu) : _mu(mu) { _mu.lock_shared(); }
  ~shared_lock() { _mu.unlock_shared(); }
};

class unique_lock {
  shared_mutex &_mu;

public:
  unique_lock(shared_mutex &mu) : _mu(mu) { _mu.lock(); }
  ~unique_lock() { _mu.unlock(); }
};

class lock_guard {
  mutex &_mu;

public:
  lock_guard(mutex &mu) : _mu(mu) { _mu.lock(); }
  ~lock_guard() { _mu.unlock(); }
};

template <typename Mutex> class ptr_lock_guard {
  Mutex *_mu;

public:
  ptr_lock_guard(Mutex *mu) : _mu(mu) {
    if (_mu)
      _mu->lock();
  }
  ~ptr_lock_guard() {
    if (_mu)
      _mu->unlock();
  }
};
#endif // PIN_MUTEX_H