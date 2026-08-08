#include <atomic>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <fcntl.h>
#include <os/lock.h>
#include <pthread.h>
#include <unistd.h>

namespace {
std::atomic<bool> active{false};
std::atomic<std::uintptr_t> auditedThread{0};
std::atomic<std::size_t> lockCalls{0};
std::atomic<std::size_t> fileOpenCalls{0};
std::atomic<std::size_t> writeCalls{0};

bool shouldCount() noexcept {
  return active.load(std::memory_order_relaxed) &&
         auditedThread.load(std::memory_order_relaxed) ==
             reinterpret_cast<std::uintptr_t>(pthread_self());
}

void count(std::atomic<std::size_t>& counter) noexcept {
  if (shouldCount()) {
    counter.fetch_add(1, std::memory_order_relaxed);
  }
}

static_assert(std::atomic<bool>::is_always_lock_free);
static_assert(std::atomic<std::uintptr_t>::is_always_lock_free);
static_assert(std::atomic<std::size_t>::is_always_lock_free);
}

extern "C" void asteRealtimeAuditReset() {
  lockCalls.store(0, std::memory_order_relaxed);
  fileOpenCalls.store(0, std::memory_order_relaxed);
  writeCalls.store(0, std::memory_order_relaxed);
}

extern "C" void asteRealtimeAuditSetActive(int shouldBeActive) {
  if (shouldBeActive != 0) {
    auditedThread.store(reinterpret_cast<std::uintptr_t>(pthread_self()),
                        std::memory_order_relaxed);
    active.store(true, std::memory_order_release);
  } else {
    active.store(false, std::memory_order_release);
  }
}

extern "C" std::size_t asteRealtimeAuditLockCalls() {
  return lockCalls.load(std::memory_order_relaxed);
}
extern "C" std::size_t asteRealtimeAuditFileOpenCalls() {
  return fileOpenCalls.load(std::memory_order_relaxed);
}
extern "C" std::size_t asteRealtimeAuditWriteCalls() {
  return writeCalls.load(std::memory_order_relaxed);
}

#define ASTE_INTERPOSE(replacement, replacee)                                  \
  __attribute__((used)) static const struct {                                  \
    const void* replacementAddress;                                             \
    const void* replaceeAddress;                                                \
  } asteInterpose_##replacement __attribute__((section("__DATA,__interpose"))) = { \
      reinterpret_cast<const void*>(                                            \
          reinterpret_cast<std::uintptr_t>(&replacement)),                     \
      reinterpret_cast<const void*>(                                            \
          reinterpret_cast<std::uintptr_t>(&replacee))}

extern "C" int asteMutexLock(pthread_mutex_t* mutex) {
  count(lockCalls);
  return pthread_mutex_lock(mutex);
}

extern "C" int asteMutexTryLock(pthread_mutex_t* mutex) {
  count(lockCalls);
  return pthread_mutex_trylock(mutex);
}

extern "C" int asteReadLock(pthread_rwlock_t* lock) {
  count(lockCalls);
  return pthread_rwlock_rdlock(lock);
}

extern "C" int asteWriteLock(pthread_rwlock_t* lock) {
  count(lockCalls);
  return pthread_rwlock_wrlock(lock);
}

extern "C" int asteConditionWait(pthread_cond_t* condition,
                                  pthread_mutex_t* mutex) {
  count(lockCalls);
  return pthread_cond_wait(condition, mutex);
}

extern "C" int asteConditionTimedWait(pthread_cond_t* condition,
                                       pthread_mutex_t* mutex,
                                       const timespec* timeout) {
  count(lockCalls);
  return pthread_cond_timedwait(condition, mutex, timeout);
}

extern "C" void asteUnfairLock(os_unfair_lock_t lock) {
  count(lockCalls);
  os_unfair_lock_lock(lock);
}

extern "C" bool asteUnfairTryLock(os_unfair_lock_t lock) {
  count(lockCalls);
  return os_unfair_lock_trylock(lock);
}

extern "C" FILE* asteFileOpen(const char* path, const char* mode) {
  count(fileOpenCalls);
  return std::fopen(path, mode);
}

extern "C" int asteOpen(const char* path, int flags, ...) {
  count(fileOpenCalls);
  if ((flags & O_CREAT) != 0) {
    va_list arguments;
    va_start(arguments, flags);
    const auto mode = static_cast<mode_t>(va_arg(arguments, int));
    va_end(arguments);
    return open(path, flags, mode);
  }
  return open(path, flags);
}

extern "C" int asteOpenAt(int directory, const char* path, int flags, ...) {
  count(fileOpenCalls);
  if ((flags & O_CREAT) != 0) {
    va_list arguments;
    va_start(arguments, flags);
    const auto mode = static_cast<mode_t>(va_arg(arguments, int));
    va_end(arguments);
    return openat(directory, path, flags, mode);
  }
  return openat(directory, path, flags);
}

extern "C" ssize_t asteWrite(int descriptor, const void* data, std::size_t size) {
  count(writeCalls);
  return write(descriptor, data, size);
}

ASTE_INTERPOSE(asteMutexLock, pthread_mutex_lock);
ASTE_INTERPOSE(asteMutexTryLock, pthread_mutex_trylock);
ASTE_INTERPOSE(asteReadLock, pthread_rwlock_rdlock);
ASTE_INTERPOSE(asteWriteLock, pthread_rwlock_wrlock);
ASTE_INTERPOSE(asteConditionWait, pthread_cond_wait);
ASTE_INTERPOSE(asteConditionTimedWait, pthread_cond_timedwait);
ASTE_INTERPOSE(asteUnfairLock, os_unfair_lock_lock);
ASTE_INTERPOSE(asteUnfairTryLock, os_unfair_lock_trylock);
ASTE_INTERPOSE(asteFileOpen, fopen);
ASTE_INTERPOSE(asteOpen, open);
ASTE_INTERPOSE(asteOpenAt, openat);
ASTE_INTERPOSE(asteWrite, write);
#undef ASTE_INTERPOSE
