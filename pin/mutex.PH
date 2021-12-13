#ifndef PIN_MUTEX_H
#define PIN_MUTEX_H

class mutex {
  PIN_MUTEX _mu;

public:
  mutex() { PIN_MutexInit(&_mu); }
  ~mutex() { PIN_MutexFini(&_mu); }
  void lock() { PIN_MutexLock(&_mu); }
  void unlock() { PIN_MutexUnlock(&_mu); }
};

class lock_guard {
  mutex &_mu;

public:
  lock_guard(mutex &mu) : _mu(mu) { _mu.lock(); }
  ~lock_guard() { _mu.unlock(); }
};

class ptr_lock_guard {
  mutex *_mu;

public:
  ptr_lock_guard(mutex *mu) : _mu(mu) {
    if (_mu)
      _mu->lock();
  }
  ~ptr_lock_guard() {
    if (_mu)
      _mu->unlock();
  }
};
#endif // PIN_MDCACHE_H