#include "QDebug.h"

#include "SReqListener.h"

using namespace std;

bool SReqListener::threadLoop() {
	while (true) {
		bool condinue = true;
		ReqMsg msgRcv, msgSnd;

		{
			SMutex::Autolock autoLock(mLock);

			while (!mDone && mQueue->isEmpty()) {
				mQueuePushed.wait(mLock);
			}

			// finish only when queue is empty
			if (mDone && mQueue->isEmpty()) {
				break;
			}

			mQueue->pop(msgRcv);

			if (mQueue->size() == mQueue->maxSize() - 1) {
				mQueuePoped.signal();
			}
		}

		if (msgRcv.type == ReqMsg::DONE) {
			broadcast(msgRcv);
			mDone = true;
			break;
		}

		condinue = organize(msgRcv, msgSnd);

		if (msgSnd.type != ReqMsg::NONE) {
			broadcast(msgSnd);
		}

		if (!condinue) {
			break;
		}
	}

	mWorking = false;

	return false;
}

SmpError SReqListener::readyToRun() {
	SMutex::Autolock autoLock(mLock);

	mWorking = true;
	mStarted.signal();

	return SThread::readyToRun();
}

SmpError SReqListener::run(const char* name, int priority, int stack) {
	return SThread::run(mName.c_str(), priority);
}

void SReqListener::broadcast(const ReqMsg &msg) {
	AudienceMap::iterator it;

	for (it = mReqAudience.begin(); it != mReqAudience.end(); it++) {
		SReqListener * listener = (*it).second;
		listener->request(msg);
	}
}

// The example code of organize virtual method
bool SReqListener::organize(ReqMsg &msgRcv, ReqMsg &msgSnd) {
	LOGINF("%s: %p", mName.c_str(), msgRcv.u.push_message.data);

	if (!mName.compare("t1") && msgRcv.u.push_message.data == (void *)0x1) {
		LOGVBS("t1 sends a message to t2");

		ReqMsg msg;

		msg.type = ReqMsg::PUSH;
		msg.u.push_message.data = (void *)0xbaad;

		AudienceMap::iterator it = mReqAudience.find("t2");
		if (it != mReqAudience.end()) {
			SReqListener *listener = it->second;
			listener->request(msg);
		}
	}

	// just copy the orignal message
	msgSnd = msgRcv;

	return true;
}

void SReqListener::request(const ReqMsg &msg) {
	SMutex::Autolock autoLock(mLock);

	if (mQueue->size() >= mQueue->maxSize()) {
		LOGVBS("%s: wait for queueing, current size: %llu",
			mName.c_str(), mQueue->size());
		QASSERT(mQueue->size() == mQueue->maxSize());

		mQueuePoped.wait(mLock);
	}

	mQueue->push(msg);
	mQueuePushed.signal();
}

void SReqListener::finish() {
	SMutex::Autolock autoLock(mLock);

	// 'finish' should take effect only after the thread works
	if (!mWorking && !mDone)
		mStarted.wait(mLock);

	mDone = true;
	mQueuePushed.signal();
}

void SReqListener::registerReqAudience(SReqListener &listener) {
	SMutex::Autolock autoLock(mLock);

	mReqAudience[listener.mName] = &listener;
}

