#ifndef __STHREAD_H___
#define __STHREAD_H___

#include "SmpCommon.h"
#include "SMutex.h"
#include "SCondition.h"

typedef void * thread_id_t;
typedef int (*thread_func_t)(void *);

// This maps directly to the "nice" priorities we use in Linux.
enum {
	STHREAD_PRIORITY_LOWEST         = 19,

	/* use for background tasks */
	STHREAD_PRIORITY_BACKGROUND     = 10,

	/* most threads run at normal priority */
	STHREAD_PRIORITY_NORMAL         = 0,

	/* threads currently running a UI that the user is interacting with */
	STHREAD_PRIORITY_FOREGROUND     = -2,

	/* should never be used in practice. regular process might not
	 * be allowed to use this level */
	STHREAD_PRIORITY_HIGHEST        = -20,

	STHREAD_PRIORITY_DEFAULT        = STHREAD_PRIORITY_NORMAL,
	STHREAD_PRIORITY_MORE_FAVORABLE = -1,
	STHREAD_PRIORITY_LESS_FAVORABLE = +1,
};

class SThread
{
public:
	// Create a SThread object, but doesn't create or start the associated
	// thread. See the run() method.
	SThread();
	virtual ~SThread();

	// Start the thread in threadLoop() which needs to be implemented.
	virtual SmpError run(const char* name = 0,
		int priority = STHREAD_PRIORITY_DEFAULT,
		int stack = 0);

	// Ask this object's thread to exit. This function is asynchronous, when the
	// function returns the thread might still be running. Of course, this
	// function can be called from a different thread.
	virtual void requestExit();

	// Good place to do one-time initializations
	virtual SmpError readyToRun();

	// Call requestExit() and wait until this object's thread exits.
	// BE VERY CAREFUL of deadlocks. In particular, it would be silly to call
	// this function from this object's thread. Will return WOULD_BLOCK in
	// that case.
	int requestExitAndWait();

	// Wait until this object's thread exits. Returns immediately if not yet running.
	// Do not call from this object's thread; will return WOULD_BLOCK in that case.
	int join();

protected:
	// exitPending() returns true if requestExit() has been called.
	bool exitPending() const;

private:
	// Derived class must implement threadLoop(). The thread starts its life
	// here. There are two ways of using the SThread object:
	// 1) loop: if threadLoop() returns true, it will be called again if
	//          requestExit() wasn't called.
	// 2) once: if threadLoop() returns false, the thread will exit upon return.
	virtual bool threadLoop() = 0;

	SThread& operator=(const SThread&);
	static  int _threadLoop(void* user);
	thread_id_t mThread;
	mutable SMutex mLock;
	SCondition mThreadExitedCondition;
	SmpError mStatus;
	// note that all accesses of mExitPending and mRunning need to hold mLock
	bool mExitPending;
	bool mRunning;
};

#endif // __STHREAD_H___

