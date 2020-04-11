#ifndef __SCIRCULAR_QUEUE_H__
#define __SCIRCULAR_QUEUE_H__

#include "QPort.h"
#include "QDebug.h"

template <typename T>
class SCircularQueue {
public:
	SCircularQueue(size_t queueMax);
	~SCircularQueue() {
		delete [] mBins;
	}

	bool push(const T &node);
	bool pop(T &node);

	size_t size() const {
		return mSize;
	}

	size_t maxSize() const {
		return mQueueMax;
	}

	bool isEmpty() const {
		return size() == 0;
	}

private:
	size_t mQueueMax;
	T *mBins;
	size_t mFirst, mLast;
	size_t mSize;
};

template <typename T>
SCircularQueue<T>::SCircularQueue(size_t queueMax) {
	mQueueMax = queueMax;
	mBins = NEW T[mQueueMax];
	QASSERT(mBins != NULL);

	mFirst = 0;
	mLast = 0;
	mSize = 0;
}

template <typename T>
bool SCircularQueue<T>::push(const T &node) {
	if (mSize == mQueueMax) {
		return false;
	}

	mBins[mLast] = node;

	if (mLast == mQueueMax - 1) {
		mLast = 0;
	} else {
		mLast++;
	}

	mSize++;

	return true;
}

template <typename T>
bool SCircularQueue<T>::pop(T &node) {
	if (mSize == 0) {
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

	return true;
}

#endif // __SCIRCULAR_QUEUE_H__
