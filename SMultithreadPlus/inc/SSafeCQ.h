#ifndef __SSAFECQ_H__
#define __SSAFECQ_H__

#include "QPort.h"
#include "QDebug.h"

#include "SMutex.h"
#include "SCondition.h"

template <typename T>
class SSafeCQ {
public:
	SSafeCQ(size_t queueMax);
	~SSafeCQ() {
		delete [] mBins;
	}

	bool push(const T &node);
	bool ordered_push(const T &node, unsigned int ID);
	bool pop(T &node);
	bool try_pop(T &node);
	void set_next_id(unsigned int id);
	void destroy();
	void init();

private:
	// this member function can't be public,
	// because calling action of these functions without mutex is unsafe
	size_t size() const {
		return mSize;
	}

	size_t maxSize() const {
		return mQueueMax;
	}

	bool isEmpty() const {
		return size() == 0;
	}

	size_t mQueueMax;
	T *mBins;
	size_t mFirst, mLast;
	size_t mSize;
	SMutex mLock;
	bool mDestroyed;
	SCondition mQueuePush, mQueuePop, mQueueOrder;
	unsigned int mNextID; // unsigned becaused of considering wrap-around
};

template <typename T>
SSafeCQ<T>::SSafeCQ(size_t queueMax) : mNextID(0) {
	mQueueMax = queueMax;
	mBins = NEW T[mQueueMax];
	QASSERT(mBins != NULL);

	init();
}

template <typename T>
bool SSafeCQ<T>::push(const T &node) {
	SMutex::Autolock autoLock(mLock);

	while (!mDestroyed && mSize == mQueueMax) {
		mQueuePop.wait(mLock);
	}

	if (mDestroyed) {
		return false;
	}

	mBins[mLast] = node;

	if (mLast == mQueueMax - 1) {
		mLast = 0;
	} else {
		mLast++;
	}

	mSize++;

	mQueuePush.signal();

	return true;
}

template <typename T>
bool SSafeCQ<T>::ordered_push(const T &node, unsigned int ID) {
	SMutex::Autolock autoLock(mLock);

	while (!mDestroyed && mNextID != ID) {
		mQueueOrder.wait(mLock);
	}

	while (!mDestroyed && mSize == mQueueMax) {
		mQueuePop.wait(mLock);
	}

	if (mDestroyed) {
		return false;
	}

	mBins[mLast] = node;

	if (mLast == mQueueMax - 1) {
		mLast = 0;
	} else {
		mLast++;
	}

	mSize++;

	mQueuePush.signal();

	mNextID = ID + 1;
	mQueueOrder.broadcast();

	return true;
}

template <typename T>
bool SSafeCQ<T>::pop(T &node) {
	SMutex::Autolock autoLock(mLock);

	while (!mDestroyed && mSize == 0) {
		mQueuePush.wait(mLock);
	}

	if (mDestroyed) {
		return false;
	}

	size_t tmp = mFirst;

	if (mFirst == mQueueMax - 1) {
		mFirst = 0;
	} else {
		mFirst++;
	}

	mSize--;

	node = mBins[tmp];

	mQueuePop.signal();

	return true;
}

template <typename T>
bool SSafeCQ<T>::try_pop(T &node) {
	SMutex::Autolock autoLock(mLock);

	while (mDestroyed || mSize == 0) {
		return false;
	}

	size_t tmp = mFirst;

	if (mFirst == mQueueMax - 1) {
		mFirst = 0;
	} else {
		mFirst++;
	}

	mSize--;

	node = mBins[tmp];

	mQueuePop.signal();

	return true;
}

template <typename T>
void SSafeCQ<T>::set_next_id(unsigned int id) {
	SMutex::Autolock autoLock(mLock);

	mNextID = id;
}

// NOTE: after destroying a queue,
// all threads that used the queue should requestExitAndWait()
template <typename T>
void SSafeCQ<T>::destroy() {
	SMutex::Autolock autoLock(mLock);

	mDestroyed = true;

	mQueueOrder.broadcast();
	mQueuePush.broadcast();
	mQueuePop.broadcast();
}

template <typename T>
void SSafeCQ<T>::init() {
	mFirst = 0;
	mLast = 0;
	mSize = 0;
	mDestroyed = false;
}

#endif // __SSAFECQ_H__
