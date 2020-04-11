#ifndef __SMUTEX_H__
#define __SMUTEX_H__

#if defined(ANDROID) || defined(__GNUC__)
# include <pthread.h>
#endif

class SCondition;

/*
 * Simple mutex class.  The implementation is system-dependent.
 *
 * The mutex must be unlocked by the thread that locked it.  They are not
 * recursive, i.e. the same thread can't lock it multiple times.
 */
class SMutex {
public:
	enum {
		PRIVATE = 0,
		SHARED = 1
	};

	SMutex();
	SMutex(const char* name);
	SMutex(int type, const char* name = NULL);
	~SMutex();

	// lock or unlock the mutex
	int lock();
	void unlock();

	// lock if possible; returns 0 on success, error otherwise
	int tryLock();

	// Manages the mutex automatically. It'll be locked when Autolock is
	// constructed and released when Autolock goes out of scope.
	class Autolock {
	public:
		inline Autolock(SMutex& mutex) : mLock(mutex)  { mLock.lock(); }
		inline Autolock(SMutex* mutex) : mLock(*mutex) { mLock.lock(); }
		inline ~Autolock() { mLock.unlock(); }
	private:
		SMutex& mLock;
	};

private:
	friend class SCondition;

	// A mutex cannot be copied
	SMutex(const SMutex&);
	SMutex& operator = (const SMutex&);

#if defined(ANDROID) || defined(__GNUC__)
	pthread_mutex_t mMutex;
#elif defined(_WIN32)
	void _init();
	void *mState;
#else
# error "need to implement for this platform."
#endif

};

#endif // __SMUTEX_H__

