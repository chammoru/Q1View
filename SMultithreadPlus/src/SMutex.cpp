#include "QPort.h"
#include "QDebug.h"

#if defined(_WIN32)
# include <Windows.h>
#endif

#include "SMutex.h"

#if defined(ANDROID) || defined(__GNUC__)

SMutex::SMutex() {
	pthread_mutex_init(&mMutex, NULL);
}

SMutex::SMutex(const char* name) {
	pthread_mutex_init(&mMutex, NULL);
}

SMutex::SMutex(int type, const char* name) {
	if (type == SHARED) {
		pthread_mutexattr_t attr;
		pthread_mutexattr_init(&attr);
		pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
		pthread_mutex_init(&mMutex, &attr);
		pthread_mutexattr_destroy(&attr);
	} else {
		pthread_mutex_init(&mMutex, NULL);
	}
}

SMutex::~SMutex() {
	pthread_mutex_destroy(&mMutex);
}

int SMutex::lock() {
	return -pthread_mutex_lock(&mMutex);
}

void SMutex::unlock() {
	pthread_mutex_unlock(&mMutex);
}

int SMutex::tryLock() {
	return -pthread_mutex_trylock(&mMutex);
}

#elif defined(_WIN32)

SMutex::SMutex()
{
	HANDLE hMutex;

	QASSERT(sizeof(hMutex) == sizeof(mState));

	hMutex = CreateMutex(NULL, FALSE, NULL);
	mState = (void*) hMutex;
}

SMutex::SMutex(const char* name)
{
	// XXX: name not used for now
	HANDLE hMutex;

	QASSERT(sizeof(hMutex) == sizeof(mState));

	hMutex = CreateMutex(NULL, FALSE, NULL);
	mState = (void*) hMutex;
}

SMutex::SMutex(int type, const char* name)
{
	// XXX: type and name not used for now
	HANDLE hMutex;

	QASSERT(sizeof(hMutex) == sizeof(mState));

	hMutex = CreateMutex(NULL, FALSE, NULL);
	mState = (void*) hMutex;
}

SMutex::~SMutex()
{
	CloseHandle((HANDLE) mState);
}

int SMutex::lock()
{
	DWORD dwWaitResult;
	dwWaitResult = WaitForSingleObject((HANDLE) mState, INFINITE);
	return dwWaitResult != WAIT_OBJECT_0 ? -1 : NO_ERROR;
}

void SMutex::unlock()
{
	if (!ReleaseMutex((HANDLE) mState))
		LOGWRN("WARNING: bad result from unlocking mutex");
}

int SMutex::tryLock()
{
	DWORD dwWaitResult;

	dwWaitResult = WaitForSingleObject((HANDLE) mState, 0);
	if (dwWaitResult != WAIT_OBJECT_0 && dwWaitResult != WAIT_TIMEOUT)
		LOGWRN("WARNING: bad result from try-locking mutex");

	return (dwWaitResult == WAIT_OBJECT_0) ? 0 : -1;
}

#else
# error "need to implement for this platform."
#endif

