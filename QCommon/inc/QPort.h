#ifndef __QPORT__
#define __QPORT__

#if defined(ANDROID) || defined(__GNUC__)
#include <unistd.h>
#define QMSleep(x) (usleep((x) * 1000))
#elif defined(_WIN32)
#include <Windows.h>
#define QMSleep(x) (Sleep(x))
#else
# error "need to implement for this platform."
#endif

#include <new>

#define NEW new(std::nothrow)

#if (_M_IX86 || _M_AMD64 || _M_IA64) && defined(_WIN32)
#include <Intrin.h>
#define qcmn_atomic_inc _InterlockedIncrement
#define qcmn_atomic_dec _InterlockedDecrement
#elif defined(__GNUC__)
/* __sync_fetch_and_add is used for (*A)++ */
#define qcmn_atomic_inc(A) __sync_add_and_fetch((A), 1);
#else
# error "need to implement for this platform."
#endif

#endif // __QPORT__

