#include "QPort.h"
#include "QDebug.h"

#if defined(ANDROID) || defined(__GNUC__)
# include <errno.h>
# include <string.h>
# include <unistd.h>
# include <stdlib.h>
# include <pthread.h>
# include <sched.h>
# include <sys/resource.h>
# include <sys/prctl.h>
#elif defined(_WIN32)
# include <Windows.h>
# include <process.h>
#else
# error "need to implement for this platform."
#endif

#include "SMutex.h"
#include "SCondition.h"
#include "SThread.h"

/*
 * ===========================================================================
 *      Thread wrappers
 * ===========================================================================
 */

#if defined(ANDROID) || defined(__GNUC__)

/*
 * Create and run a new thread.
 *
 * We create it "detached", so it cleans up after itself.
 */

typedef void* (*pthread_entry)(void*);

struct thread_data_t {
	thread_func_t   entryFunction;
	void*           userData;
	int             priority;
	char *          threadName;

	// we use this trampoline when we need to set the priority with
	// nice/setpriority, and name with prctl.
	static int trampoline(const thread_data_t* t) {
		thread_func_t f = t->entryFunction;
		void* u = t->userData;
		int prio = t->priority;
		char * name = t->threadName;
		delete t;
		setpriority(PRIO_PROCESS, 0, prio);

		if (name) {
			int hasAt = 0;
			int hasDot = 0;
			char *s = name;
			while (*s) {
				if (*s == '.') hasDot = 1;
				else if (*s == '@') hasAt = 1;
				s++;
			}
			int len = s - name;
			if (len < 15 || hasAt || !hasDot) {
				s = name;
			} else {
				s = name + len - 15;
			}
			prctl(PR_SET_NAME, (unsigned long) s, 0, 0, 0);
			free(name);
		}

		return f(u);
	}
};

bool createRawThreadEtc(thread_func_t entryFunction,
		void *userData,
		const char* threadName,
		int threadPriority,
		int threadStackSize,
		thread_id_t *threadId)
{
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

	if (threadPriority != STHREAD_PRIORITY_DEFAULT || threadName != NULL) {
		// Now that the pthread_t has a method to find the associated
		// thread_id_t (pid) from pthread_t, it would be possible to avoid
		// this trampoline in some cases as the parent could set the properties
		// for the child.  However, there would be a race condition because the
		// child becomes ready immediately, and it doesn't work for the name.
		// prctl(PR_SET_NAME) only works for self; prctl(PR_SET_THREAD_NAME) was
		// proposed but not yet accepted.
		thread_data_t *t = NEW thread_data_t;
		QASSERT_R(t != NULL, false);
		t->priority = threadPriority;
		t->threadName = threadName ? strdup(threadName) : NULL;
		t->entryFunction = entryFunction;
		t->userData = userData;
		entryFunction = (thread_func_t)&thread_data_t::trampoline;
		userData = t;
	}

	if (threadStackSize) {
		pthread_attr_setstacksize(&attr, threadStackSize);
	}

	errno = 0;
	pthread_t thread;
	int result = pthread_create(&thread, &attr,
		(pthread_entry)entryFunction, userData);
	pthread_attr_destroy(&attr);
	if (result != 0) {
		return false;
	}

	// Note that *threadID is directly available to the parent only, as it is
	// assigned after the child starts.  Use memory barrier / lock if the child
	// or other threads also need access.
	if (threadId != NULL) {
		*threadId = (thread_id_t)thread; // XXX: this is not portable
	}

	return true;
}

thread_id_t getThreadId()
{
	return (thread_id_t)pthread_self();
}

#elif defined(_WIN32)

/*
 * Trampoline to make us __stdcall-compliant.
 *
 * We're expected to delete "vDetails" when we're done.
 */
struct threadDetails {
	int (*func)(void*);
	void* arg;
};

static unsigned __stdcall threadIntermediary(void *vDetails)
{
	struct threadDetails* pDetails = (struct threadDetails*) vDetails;
	int result;

	result = (*(pDetails->func))(pDetails->arg);

	delete pDetails;

	return (unsigned int) result;
}

/*
 * Create and run a new thread.
 */
static bool doCreateThread(thread_func_t fn, void* arg, thread_id_t *id)
{
	HANDLE hThread;
	struct threadDetails *pDetails = NEW threadDetails; // must be on heap
	QASSERT_R(pDetails != NULL, false);

	pDetails->func = fn;
	pDetails->arg = arg;

	unsigned int thrdaddr;
	hThread = (HANDLE)_beginthreadex(NULL, 0, threadIntermediary, pDetails, 0,
		&thrdaddr);
	QASSERT_R(hThread != 0, false);

	if (id != NULL) {
		*id = (thread_id_t)thrdaddr;
	}

	return true;
}

bool createRawThreadEtc(thread_func_t fn,
		void *userData,
		const char* threadName,
		int threadPriority,
		int threadStackSize,
		thread_id_t *threadId)
{
	return doCreateThread(fn, userData, threadId);
}

thread_id_t getThreadId()
{
	return (thread_id_t)GetCurrentThreadId();
}

#else
# error "need to implement for this platform."
#endif

/*
 * This is our thread object!
 */

SThread::SThread()
	:mThread(thread_id_t(-1)),
	mStatus(SMP_OK),
	mExitPending(false),
	mRunning(false)
{
}

SThread::~SThread()
{
}

SmpError SThread::run(const char* name, int priority, int stack)
{
	SMutex::Autolock _l(mLock);

	if (mRunning) {
		// thread already started
		return SMP_INVALID_REQ;
	}

	// reset status and exitPending to their default value, so we can
	// try again after an error happened (either below, or in readyToRun())
	mStatus = SMP_OK;
	mExitPending = false;
	mThread = thread_id_t(-1);
	mRunning = true;

	bool res;
	res = createRawThreadEtc(_threadLoop, this, name, priority, stack, &mThread);

	if (res == false) {
		mStatus = SMP_UNKNOWN;   // something happened!
		mRunning = false;
		mThread = thread_id_t(-1);

		return SMP_UNKNOWN;
	}

	// Do not refer to mStatus here: The thread is already running (may, in fact
	// already have exited with a valid mStatus result). The SMP_OK indication
	// here merely indicates successfully starting the thread and does not
	// imply successful termination/execution.
	return SMP_OK;

	// Exiting scope of mLock is a memory barrier and allows new thread to run
}

SmpError SThread::readyToRun()
{
	return SMP_OK;
}

int SThread::_threadLoop(void* user)
{
	SThread* const self = static_cast<SThread*>(user);

	bool first = true;
	bool result;

	do {
		if (first) {
			first = false;
			self->mStatus = self->readyToRun();
			result = self->mStatus == SMP_OK;

			if (result && !self->exitPending()) {
				// Binder threads (and maybe others) rely on threadLoop
				// running at least once after a successful ::readyToRun()
				// (unless, of course, the thread has already been asked to exit
				// at that point).
				// This is because threads are essentially used like this:
				//   (new ThreadSubclass())->run();
				// The caller therefore does not retain a strong reference to
				// the thread and the thread would simply disappear after the
				// successful ::readyToRun() call instead of entering the
				// threadLoop at least once.
				result = self->threadLoop();
			}
		} else {
			result = self->threadLoop();
		}

		// establish a scope for mLock
		{
			SMutex::Autolock _l(self->mLock);
			if (result == false || self->mExitPending) {
				self->mExitPending = true;
				self->mRunning = false;
				// clear thread ID so that requestExitAndWait() does not exit if
				// called by a new thread using the same thread ID as this one.
				self->mThread = thread_id_t(-1);
				// note that interested observers blocked in requestExitAndWait are
				// awoken by broadcast, but blocked on mLock until break exits scope
				self->mThreadExitedCondition.broadcast();
				break;
			}
		}
	} while (true);

	return 0;
}

void SThread::requestExit()
{
	SMutex::Autolock _l(mLock);

	mExitPending = true;
}

int SThread::requestExitAndWait()
{
	SMutex::Autolock _l(mLock);

	if (mThread == getThreadId()) {
		LOGWRN(
	        "Thread (this=%p): don't call waitForExit() from this "
	        "Thread object's thread. It's a guaranteed deadlock!",
	        this);
		return SMP_WOULD_BLOCK;
	}

	mExitPending = true;

	while (mRunning == true) {
		mThreadExitedCondition.wait(mLock);
	}

	// This next line is probably not needed any more, but is being left for
	// historical reference. Note that each interested party will clear flag.
	mExitPending = false;

	return mStatus;
}

int SThread::join()
{
	SMutex::Autolock _l(mLock);

	if (mThread == getThreadId()) {
		LOGWRN(
	        "Thread (this=%p): don't call join() from this "
			"Thread object's thread. It's a guaranteed deadlock!",
			this);

		return SMP_WOULD_BLOCK;
	}

	while (mRunning == true) {
		mThreadExitedCondition.wait(mLock);
	}

	return mStatus;
}

bool SThread::exitPending() const
{
	SMutex::Autolock _l(mLock);

	return mExitPending;
}

