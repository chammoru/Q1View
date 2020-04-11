#ifndef __SCONDITION_H__
#define __SCONDITION_H__

typedef unsigned long long nsecs_t;

/*
 * SCondition variable class.  The implementation is system-dependent.
 *
 * SCondition variables are paired up with mutexes.  Lock the mutex,
 * call wait(), then either re-wait() if things aren't quite what you want,
 * or unlock the mutex and continue.  All threads calling wait() must
 * use the same mutex for a given SCondition.
 */
class SCondition {
public:
	enum {
		PRIVATE = 0,
		SHARED = 1
	};

	SCondition();
	SCondition(int type);
	~SCondition();

	// Wait on the condition variable.  Lock the mutex before calling.
	int wait(SMutex& mutex);
	// same with relative timeout
	int waitRelative(SMutex& mutex, nsecs_t reltime);
	// Signal the condition variable, allowing one thread to continue.
	void signal();
	// Signal the condition variable, allowing all threads to continue.
	void broadcast();

private:
#if defined(ANDROID) || defined(__GNUC__)
	pthread_cond_t mCond;
#elif defined(_WIN32)
	void *mState;
#else
# error "need to implement for this platform."
#endif
};

#endif // __SCONDITION_H__

