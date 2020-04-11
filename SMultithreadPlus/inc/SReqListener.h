#ifndef __SREQLISTENER_H__
#define __SREQLISTENER_H__

#include <string>
#include <map>

#include "QPort.h"
#include "QDebug.h"

#include "SMutex.h"
#include "SCondition.h"
#include "SThread.h"
#include "SParcel.h"
#include "SCircularQueue.h"

class ReqMsg {
public:
	enum {
		NONE,
		PUSH,
		DONE,
	} type;

	SParcel p;

	union {
		// if type == PUSH
		struct {
			void *data;
		} push_message;
	} u;

	ReqMsg() : type(NONE) {}

	// NOTE: The default assignment operator will be used
	// ReqMsg& operator=(const ReqMsg&);
};

class SReqListener;

typedef std::map<std::string, SReqListener *> AudienceMap;

class SReqListener : public SThread {
public:
	SReqListener() {
		init(QUEUE_MAX);
	}

	SReqListener(size_t queueMax) {
		init(queueMax);
	}

	SReqListener(const char *name, size_t queueMax = QUEUE_MAX) :
		mName(name) {
		init(queueMax);
	}

	virtual ~SReqListener() {
		delete mQueue;
	}

	virtual SmpError run(const char* name = 0,
		int priority = STHREAD_PRIORITY_DEFAULT,
		int stack = 0);

	void request(const ReqMsg &msg);
	void finish();
	// A listener also becomes presenter later, so it can have its own audience.
	void registerReqAudience(SReqListener &listener);

protected:
	virtual bool threadLoop();
	SmpError readyToRun();

	void broadcast(const ReqMsg &msg);

	// default size of mQueue
	static const size_t QUEUE_MAX = 16;

	std::string mName;

private:
	void init(size_t queueMax) {
		mDone = false;
		mWorking = false;
		mQueue = NEW SCircularQueue<ReqMsg>(queueMax);
	}

	// If you set a msgSnd, this will be broadcasted to your audience.
	virtual bool organize(ReqMsg &msgRcv, ReqMsg &msgSnd);

	SCircularQueue<ReqMsg> *mQueue;
	bool mDone;
	bool mWorking;
	SMutex mLock;
	SCondition mQueuePushed;
	SCondition mQueuePoped;
	SCondition mStarted;
	AudienceMap mReqAudience;
};

#endif // __SREQLISTENER_H__

