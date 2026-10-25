// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "AsyncUV.h"

#include "../../../../Libraries/Threading/Threading.h"

#if SC_PLATFORM_WINDOWS
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#else
#include <pthread.h>
#include <sys/time.h>
#endif

namespace
{
static constexpr size_t AsyncUVThreadNameCapacity = 64;
static constexpr size_t AsyncUVThreadNameSlots    = 64;

thread_local void* asyncUVCurrentThreadObject                          = nullptr;
thread_local char  asyncUVCurrentThreadName[AsyncUVThreadNameCapacity] = {};

struct AsyncUVThreadNameEntry
{
    void* thread                          = nullptr;
    char  name[AsyncUVThreadNameCapacity] = {};
};

struct AsyncUVThreadStart
{
    uv_thread_cb entry = nullptr;
    void*        arg   = nullptr;

    unsigned int references = 2; // Worker and join/detach caller each own one reference.
#if SC_PLATFORM_WINDOWS
    HANDLE thread = nullptr;
#else
    pthread_t thread = {};
#endif
};

SC::Mutex& asyncUVThreadNamesMutex()
{
    static SC::Mutex mutex;
    return mutex;
}

void releaseThreadStart(AsyncUVThreadStart* state)
{
    SC::Mutex& mutex = asyncUVThreadNamesMutex();
    mutex.lock();
    const bool destroy = --state->references == 0;
    mutex.unlock();
    if (destroy)
        delete state;
}

#if SC_PLATFORM_WINDOWS
DWORD WINAPI runAsyncUVThread(void* argument)
#else
void* runAsyncUVThread(void* argument)
#endif
{
    AsyncUVThreadStart* state = static_cast<AsyncUVThreadStart*>(argument);

    asyncUVCurrentThreadObject = state;
    state->entry(state->arg);
    asyncUVCurrentThreadObject = nullptr;
    releaseThreadStart(state);
    return 0;
}

AsyncUVThreadNameEntry* asyncUVThreadNames()
{
    static AsyncUVThreadNameEntry entries[AsyncUVThreadNameSlots];
    return entries;
}

void copyThreadName(char* destination, const char* source, size_t capacity)
{
    if (capacity == 0)
    {
        return;
    }
    const size_t sourceLength = strlen(source);
    const size_t copyLength   = sourceLength < capacity - 1 ? sourceLength : capacity - 1;
    memcpy(destination, source, copyLength);
    destination[copyLength] = '\0';
}

void registerThreadName(void* thread)
{
    SC::Mutex& mutex = asyncUVThreadNamesMutex();
    mutex.lock();
    AsyncUVThreadNameEntry* entries = asyncUVThreadNames();
    for (size_t idx = 0; idx < AsyncUVThreadNameSlots; ++idx)
    {
        if (entries[idx].thread == nullptr)
        {
            entries[idx].thread  = thread;
            entries[idx].name[0] = '\0';
            break;
        }
    }
    mutex.unlock();
}

void unregisterThreadName(void* thread)
{
    SC::Mutex& mutex = asyncUVThreadNamesMutex();
    mutex.lock();
    AsyncUVThreadNameEntry* entries = asyncUVThreadNames();
    for (size_t idx = 0; idx < AsyncUVThreadNameSlots; ++idx)
    {
        if (entries[idx].thread == thread)
        {
            entries[idx].thread  = nullptr;
            entries[idx].name[0] = '\0';
            break;
        }
    }
    mutex.unlock();
}

void setRegisteredThreadName(void* thread, const char* name)
{
    if (thread == nullptr)
    {
        return;
    }
    SC::Mutex& mutex = asyncUVThreadNamesMutex();
    mutex.lock();
    AsyncUVThreadNameEntry* entries = asyncUVThreadNames();
    for (size_t idx = 0; idx < AsyncUVThreadNameSlots; ++idx)
    {
        if (entries[idx].thread == thread)
        {
            copyThreadName(entries[idx].name, name, sizeof(entries[idx].name));
            break;
        }
    }
    mutex.unlock();
}

bool getRegisteredThreadName(void* thread, char* name, size_t size)
{
    SC::Mutex& mutex = asyncUVThreadNamesMutex();
    mutex.lock();
    AsyncUVThreadNameEntry* entries = asyncUVThreadNames();
    for (size_t idx = 0; idx < AsyncUVThreadNameSlots; ++idx)
    {
        if (entries[idx].thread == thread)
        {
            copyThreadName(name, entries[idx].name, size);
            mutex.unlock();
            return true;
        }
    }
    mutex.unlock();
    return false;
}

struct AsyncUVMutex
{
#if SC_PLATFORM_WINDOWS
    CRITICAL_SECTION mutex;

    AsyncUVMutex(bool) { InitializeCriticalSection(&mutex); }
    ~AsyncUVMutex() { DeleteCriticalSection(&mutex); }

    void lock() { EnterCriticalSection(&mutex); }
    void unlock() { LeaveCriticalSection(&mutex); }
    int  trylock() { return TryEnterCriticalSection(&mutex) ? 0 : UV_EBUSY; }
#else
    pthread_mutex_t mutex;

    AsyncUVMutex(bool recursive)
    {
        pthread_mutexattr_t attr;
        pthread_mutexattr_init(&attr);
        if (recursive)
        {
            pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
        }
        pthread_mutex_init(&mutex, &attr);
        pthread_mutexattr_destroy(&attr);
    }

    ~AsyncUVMutex() { pthread_mutex_destroy(&mutex); }

    void lock() { pthread_mutex_lock(&mutex); }
    void unlock() { pthread_mutex_unlock(&mutex); }
    int  trylock() { return pthread_mutex_trylock(&mutex) == 0 ? 0 : UV_EBUSY; }
#endif
};

struct AsyncUVCondition
{
#if SC_PLATFORM_WINDOWS
    CONDITION_VARIABLE condition;

    AsyncUVCondition() { InitializeConditionVariable(&condition); }
    ~AsyncUVCondition() = default;

    void wait(AsyncUVMutex& mutex) { SleepConditionVariableCS(&condition, &mutex.mutex, INFINITE); }
    int  timedwait(AsyncUVMutex& mutex, uint64_t timeout)
    {
        uint64_t milliseconds = timeout / 1000000;
        if ((timeout % 1000000) != 0)
        {
            milliseconds++;
        }
        if (milliseconds > INFINITE - 1)
        {
            milliseconds = INFINITE - 1;
        }
        return SleepConditionVariableCS(&condition, &mutex.mutex, static_cast<DWORD>(milliseconds)) ? 0 : UV_ETIMEDOUT;
    }
    void signal() { WakeConditionVariable(&condition); }
    void broadcast() { WakeAllConditionVariable(&condition); }
#else
    pthread_cond_t condition;

    AsyncUVCondition() { pthread_cond_init(&condition, nullptr); }
    ~AsyncUVCondition() { pthread_cond_destroy(&condition); }

    void wait(AsyncUVMutex& mutex) { pthread_cond_wait(&condition, &mutex.mutex); }
    int  timedwait(AsyncUVMutex& mutex, uint64_t timeout)
    {
        timeval now = {};
        gettimeofday(&now, nullptr);

        timespec abstime = {};
        abstime.tv_sec   = now.tv_sec + static_cast<time_t>(timeout / 1000000000);
        abstime.tv_nsec  = static_cast<long>(now.tv_usec * 1000 + timeout % 1000000000);
        if (abstime.tv_nsec >= 1000000000)
        {
            abstime.tv_sec++;
            abstime.tv_nsec -= 1000000000;
        }

        const int result = pthread_cond_timedwait(&condition, &mutex.mutex, &abstime);
        return result == 0 ? 0 : UV_ETIMEDOUT;
    }
    void signal() { pthread_cond_signal(&condition); }
    void broadcast() { pthread_cond_broadcast(&condition); }
#endif
};

struct AsyncUVRWLock
{
    SC::Mutex             mutex;
    SC::ConditionVariable readersQ;
    SC::ConditionVariable writersQ;

    unsigned int activeReaders  = 0;
    unsigned int waitingWriters = 0;
    bool         writerActive   = false;

    void rdlock()
    {
        mutex.lock();
        while (writerActive or waitingWriters > 0)
        {
            readersQ.wait(mutex);
        }
        activeReaders++;
        mutex.unlock();
    }

    int tryrdlock()
    {
        mutex.lock();
        if (writerActive or waitingWriters > 0)
        {
            mutex.unlock();
            return UV_EBUSY;
        }
        activeReaders++;
        mutex.unlock();
        return 0;
    }

    void rdunlock()
    {
        mutex.lock();
        activeReaders--;
        if (activeReaders == 0)
        {
            writersQ.signal();
        }
        mutex.unlock();
    }

    void wrlock()
    {
        mutex.lock();
        waitingWriters++;
        while (writerActive or activeReaders > 0)
        {
            writersQ.wait(mutex);
        }
        waitingWriters--;
        writerActive = true;
        mutex.unlock();
    }

    int trywrlock()
    {
        mutex.lock();
        if (writerActive or activeReaders > 0)
        {
            mutex.unlock();
            return UV_EBUSY;
        }
        writerActive = true;
        mutex.unlock();
        return 0;
    }

    void wrunlock()
    {
        mutex.lock();
        writerActive = false;
        if (waitingWriters > 0)
        {
            writersQ.signal();
        }
        else
        {
            readersQ.broadcast();
        }
        mutex.unlock();
    }
};

struct AsyncUVSemaphore
{
    unsigned int          count = 0;
    SC::Mutex             mutex;
    SC::ConditionVariable condition;

    AsyncUVSemaphore(unsigned int value) : count(value) {}
};

struct AsyncUVBarrier
{
    unsigned int threshold  = 0;
    unsigned int arrived    = 0;
    unsigned int leaving    = 0;
    unsigned int generation = 0;

    SC::Mutex             mutex;
    SC::ConditionVariable condition;

    AsyncUVBarrier(unsigned int count) : threshold(count) {}
};

AsyncUVMutex& uvMutex(uv_mutex_t* handle) { return *reinterpret_cast<AsyncUVMutex*>(handle->sc_opaque); }

AsyncUVCondition& uvCondition(uv_cond_t* cond) { return *reinterpret_cast<AsyncUVCondition*>(cond->sc_opaque); }

AsyncUVRWLock& uvRWLock(uv_rwlock_t* rwlock) { return *reinterpret_cast<AsyncUVRWLock*>(rwlock->sc_opaque); }

AsyncUVSemaphore& uvSemaphore(uv_sem_t* sem) { return *reinterpret_cast<AsyncUVSemaphore*>(sem->sc_opaque); }

AsyncUVBarrier& uvBarrier(uv_barrier_t* barrier) { return *reinterpret_cast<AsyncUVBarrier*>(barrier->sc_opaque); }

static_assert(sizeof(AsyncUVMutex) <= sizeof(((uv_mutex_t*)nullptr)->sc_opaque), "uv_mutex_t");
static_assert(sizeof(AsyncUVCondition) <= sizeof(((uv_cond_t*)nullptr)->sc_opaque), "uv_cond_t");
static_assert(sizeof(AsyncUVRWLock) <= sizeof(((uv_rwlock_t*)nullptr)->sc_opaque), "uv_rwlock_t");
static_assert(sizeof(AsyncUVSemaphore) <= sizeof(((uv_sem_t*)nullptr)->sc_opaque), "uv_sem_t");
static_assert(sizeof(AsyncUVBarrier) <= sizeof(((uv_barrier_t*)nullptr)->sc_opaque), "uv_barrier_t");
} // namespace

UV_API int uv_thread_create(uv_thread_t* tid, uv_thread_cb entry, void* arg)
{
    if (tid == nullptr or entry == nullptr)
    {
        return UV_EINVAL;
    }
    AsyncUVThreadStart* state = new AsyncUVThreadStart();
    state->entry              = entry;
    state->arg                = arg;
    registerThreadName(state);
#if SC_PLATFORM_WINDOWS
    state->thread      = ::CreateThread(nullptr, 0, runAsyncUVThread, state, 0, nullptr);
    const bool created = state->thread != nullptr;
#else
    const bool created = ::pthread_create(&state->thread, nullptr, runAsyncUVThread, state) == 0;
#endif
    if (not created)
    {
        unregisterThreadName(state);
        delete state;
        return -1;
    }
    static_assert(sizeof(uv_thread_t) >= sizeof(void*), "");
    *tid = state;
    return 0;
}

UV_API int uv_thread_create_ex(uv_thread_t* tid, const uv_thread_options_t* params, uv_thread_cb entry, void* arg)
{
    if (params != nullptr and (params->flags & ~UV_THREAD_HAS_STACK_SIZE) != 0)
    {
        return UV_EINVAL;
    }
    return uv_thread_create(tid, entry, arg);
}

UV_API int uv_thread_detach(uv_thread_t* tid)
{
    if (tid == nullptr)
    {
        return UV_EINVAL;
    }
    AsyncUVThreadStart* state = static_cast<AsyncUVThreadStart*>(*tid);
    if (state == nullptr)
    {
        return UV_EINVAL;
    }
#if SC_PLATFORM_WINDOWS
    const bool detached = ::CloseHandle(state->thread) != 0;
#else
    const bool detached = ::pthread_detach(state->thread) == 0;
#endif
    if (not detached)
        return UV_EINVAL;
    *tid = nullptr;
    unregisterThreadName(state);
    releaseThreadStart(state);
    return 0;
}

UV_API int uv_thread_join(uv_thread_t* tid)
{
    if (tid == nullptr)
    {
        return UV_EINVAL;
    }
    AsyncUVThreadStart* state = static_cast<AsyncUVThreadStart*>(*tid);
    if (state == nullptr)
    {
        return UV_EINVAL;
    }
#if SC_PLATFORM_WINDOWS
    if (::WaitForSingleObject(state->thread, INFINITE) != WAIT_OBJECT_0)
        return UV_EINVAL;
    if (not ::CloseHandle(state->thread))
        return UV_EINVAL;
#else
    if (::pthread_join(state->thread, nullptr) != 0)
        return UV_EINVAL;
#endif
    *tid = nullptr;
    unregisterThreadName(state);
    releaseThreadStart(state);
    return 0;
}

UV_API uv_thread_t uv_thread_self(void)
{
    const uintptr_t threadID = static_cast<uintptr_t>(SC::Thread::CurrentThreadID());
    return reinterpret_cast<uv_thread_t>(threadID);
}

UV_API int uv_thread_equal(const uv_thread_t* t1, const uv_thread_t* t2)
{
    if (t1 == nullptr or t2 == nullptr)
    {
        return 0;
    }
    return *t1 == *t2;
}

UV_API int uv_key_create(uv_key_t* key)
{
    if (key == nullptr)
    {
        return UV_EINVAL;
    }
#if SC_PLATFORM_WINDOWS
    const DWORD tlsKey = TlsAlloc();
    if (tlsKey == TLS_OUT_OF_INDEXES)
    {
        return UV_ENOMEM;
    }
    *key = static_cast<uv_key_t>(tlsKey);
#else
    pthread_key_t pthreadKey;
    if (pthread_key_create(&pthreadKey, nullptr) != 0)
    {
        return UV_EINVAL;
    }
    *key = static_cast<uv_key_t>(pthreadKey);
#endif
    return 0;
}

UV_API void uv_key_delete(uv_key_t* key)
{
    if (key == nullptr)
    {
        return;
    }
#if SC_PLATFORM_WINDOWS
    TlsFree(static_cast<DWORD>(*key));
#else
    pthread_key_delete(static_cast<pthread_key_t>(*key));
#endif
}

UV_API void* uv_key_get(uv_key_t* key)
{
    if (key == nullptr)
    {
        return nullptr;
    }
#if SC_PLATFORM_WINDOWS
    return TlsGetValue(static_cast<DWORD>(*key));
#else
    return pthread_getspecific(static_cast<pthread_key_t>(*key));
#endif
}

UV_API void uv_key_set(uv_key_t* key, void* value)
{
    if (key == nullptr)
    {
        return;
    }
#if SC_PLATFORM_WINDOWS
    TlsSetValue(static_cast<DWORD>(*key), value);
#else
    pthread_setspecific(static_cast<pthread_key_t>(*key), value);
#endif
}

UV_API int uv_cpumask_size(void) { return UV_ENOTSUP; }

UV_API int uv_thread_getcpu(void) { return UV_ENOTSUP; }

UV_API int uv_thread_setaffinity(uv_thread_t* tid, char* cpumask, char* oldmask, size_t mask_size)
{
    (void)tid;
    (void)cpumask;
    (void)oldmask;
    (void)mask_size;
    return UV_ENOTSUP;
}

UV_API int uv_thread_getaffinity(uv_thread_t* tid, char* cpumask, size_t mask_size)
{
    (void)tid;
    (void)cpumask;
    (void)mask_size;
    return UV_ENOTSUP;
}

UV_API int uv_thread_setname(const char* name)
{
    if (name == nullptr)
    {
        return UV_EINVAL;
    }
    copyThreadName(asyncUVCurrentThreadName, name, sizeof(asyncUVCurrentThreadName));
    setRegisteredThreadName(asyncUVCurrentThreadObject, asyncUVCurrentThreadName);
    return 0;
}

UV_API int uv_thread_getname(uv_thread_t* tid, char* name, size_t size)
{
    if (tid == nullptr or name == nullptr or size == 0)
    {
        return UV_EINVAL;
    }
    if (*tid == uv_thread_self())
    {
        copyThreadName(name, asyncUVCurrentThreadName, size);
        return 0;
    }
    if (getRegisteredThreadName(*tid, name, size))
    {
        return 0;
    }
    return UV_ENOENT;
}

UV_API int uv_mutex_init(uv_mutex_t* handle)
{
    if (handle == nullptr)
    {
        return UV_EINVAL;
    }
    SC::placementNew(uvMutex(handle), false);
    return 0;
}

UV_API int uv_mutex_init_recursive(uv_mutex_t* handle)
{
    if (handle == nullptr)
    {
        return UV_EINVAL;
    }
    SC::placementNew(uvMutex(handle), true);
    return 0;
}

UV_API void uv_mutex_destroy(uv_mutex_t* handle)
{
    if (handle != nullptr)
    {
        uvMutex(handle).~AsyncUVMutex();
    }
}

UV_API void uv_mutex_lock(uv_mutex_t* handle) { uvMutex(handle).lock(); }

UV_API int uv_mutex_trylock(uv_mutex_t* handle) { return uvMutex(handle).trylock(); }

UV_API void uv_mutex_unlock(uv_mutex_t* handle) { uvMutex(handle).unlock(); }

UV_API int uv_rwlock_init(uv_rwlock_t* rwlock)
{
    if (rwlock == nullptr)
    {
        return UV_EINVAL;
    }
    SC::placementNew(uvRWLock(rwlock));
    return 0;
}

UV_API void uv_rwlock_destroy(uv_rwlock_t* rwlock)
{
    if (rwlock != nullptr)
    {
        uvRWLock(rwlock).~AsyncUVRWLock();
    }
}

UV_API void uv_rwlock_rdlock(uv_rwlock_t* rwlock) { uvRWLock(rwlock).rdlock(); }

UV_API int uv_rwlock_tryrdlock(uv_rwlock_t* rwlock) { return uvRWLock(rwlock).tryrdlock(); }

UV_API void uv_rwlock_rdunlock(uv_rwlock_t* rwlock) { uvRWLock(rwlock).rdunlock(); }

UV_API void uv_rwlock_wrlock(uv_rwlock_t* rwlock) { uvRWLock(rwlock).wrlock(); }

UV_API int uv_rwlock_trywrlock(uv_rwlock_t* rwlock) { return uvRWLock(rwlock).trywrlock(); }

UV_API void uv_rwlock_wrunlock(uv_rwlock_t* rwlock) { uvRWLock(rwlock).wrunlock(); }

UV_API int uv_cond_init(uv_cond_t* cond)
{
    if (cond == nullptr)
    {
        return UV_EINVAL;
    }
    SC::placementNew(uvCondition(cond));
    return 0;
}

UV_API void uv_cond_destroy(uv_cond_t* cond)
{
    if (cond != nullptr)
    {
        uvCondition(cond).~AsyncUVCondition();
    }
}

UV_API void uv_cond_signal(uv_cond_t* cond) { uvCondition(cond).signal(); }

UV_API void uv_cond_broadcast(uv_cond_t* cond) { uvCondition(cond).broadcast(); }

UV_API void uv_cond_wait(uv_cond_t* cond, uv_mutex_t* mutex) { uvCondition(cond).wait(uvMutex(mutex)); }

UV_API int uv_cond_timedwait(uv_cond_t* cond, uv_mutex_t* mutex, uint64_t timeout)
{
    return uvCondition(cond).timedwait(uvMutex(mutex), timeout);
}

UV_API int uv_sem_init(uv_sem_t* sem, unsigned int value)
{
    if (sem == nullptr)
    {
        return UV_EINVAL;
    }
    SC::placementNew(uvSemaphore(sem), value);
    return 0;
}

UV_API void uv_sem_destroy(uv_sem_t* sem)
{
    if (sem != nullptr)
    {
        uvSemaphore(sem).~AsyncUVSemaphore();
    }
}

UV_API void uv_sem_post(uv_sem_t* sem)
{
    AsyncUVSemaphore& semaphore = uvSemaphore(sem);
    semaphore.mutex.lock();
    semaphore.count++;
    semaphore.condition.signal();
    semaphore.mutex.unlock();
}

UV_API void uv_sem_wait(uv_sem_t* sem)
{
    AsyncUVSemaphore& semaphore = uvSemaphore(sem);
    semaphore.mutex.lock();
    while (semaphore.count == 0)
    {
        semaphore.condition.wait(semaphore.mutex);
    }
    semaphore.count--;
    semaphore.mutex.unlock();
}

UV_API int uv_sem_trywait(uv_sem_t* sem)
{
    AsyncUVSemaphore& semaphore = uvSemaphore(sem);
    semaphore.mutex.lock();
    if (semaphore.count == 0)
    {
        semaphore.mutex.unlock();
        return UV_EAGAIN;
    }
    semaphore.count--;
    semaphore.mutex.unlock();
    return 0;
}

UV_API int uv_barrier_init(uv_barrier_t* barrier, unsigned int count)
{
    if (barrier == nullptr or count == 0)
    {
        return UV_EINVAL;
    }
    SC::placementNew(uvBarrier(barrier), count);
    return 0;
}

UV_API void uv_barrier_destroy(uv_barrier_t* barrier)
{
    if (barrier != nullptr)
    {
        uvBarrier(barrier).~AsyncUVBarrier();
    }
}

UV_API int uv_barrier_wait(uv_barrier_t* barrier)
{
    AsyncUVBarrier& object = uvBarrier(barrier);
    object.mutex.lock();

    const unsigned int generation = object.generation;
    object.arrived++;
    if (object.arrived == object.threshold)
    {
        object.arrived = 0;
        object.leaving = object.threshold;
        object.generation++;
        object.condition.broadcast();

        while (object.leaving > 1)
        {
            object.condition.wait(object.mutex);
        }
        object.leaving--;
        object.condition.broadcast();
        object.mutex.unlock();
        return 1;
    }

    while (generation == object.generation)
    {
        object.condition.wait(object.mutex);
    }
    object.leaving--;
    if (object.leaving == 1)
    {
        object.condition.broadcast();
    }
    object.mutex.unlock();
    return 0;
}
