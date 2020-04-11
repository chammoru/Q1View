#include "SBufferPool.h"

SBufferPool::SBufferPool(size_t poolSize) :
	mPoolSize(poolSize), mBufferSize(0), mIsReady(false), mIndex(0) {
	mPool = NEW Item[mPoolSize];
	QASSERT(mPool != NULL);
	for (size_t i = 0; i < mPoolSize; i++) {
		mPool[i].addr = NULL;
	}
	init();
}

SBufferPool::~SBufferPool() {
	disable();

	freeItems();

	if (mPool)
		delete [] mPool;
}

void SBufferPool::setup(size_t bufSize) {
	SMutex::Autolock autoLock(mLock);

	if (bufSize > mBufferSize) {
		mBufferSize = bufSize;

		freeItems();

		allocItems();
	}

	enable(0);
}

void SBufferPool::enable(unsigned char *last_addr) {
	SMutex::Autolock autoLock(mLock);

	mAvailables = mPoolSize;
	mIsReady = true;

	for (size_t i = 0; i < mPoolSize; i++) {
		if (mPool[i].addr == last_addr) {
			mPool[i].state = USED;
			mAvailables--;
			mIndex = (i + 1) == mPoolSize ? 0 : int(i + 1);
		} else {
			mPool[i].state = AVAILABLE;
		}
	}
}

void SBufferPool::disable() {
	SMutex::Autolock autoLock(mLock);

	mAvailables = 0;
	mIsReady = false;

	mReturnCond.broadcast();
}

unsigned char *SBufferPool::checkout() {
	SMutex::Autolock autoLock(mLock);

	while (mIsReady && mAvailables == 0) {
		mReturnCond.wait(mLock);
	}

	if (!mIsReady)
		return NULL;

	for (size_t i = 0; i < mPoolSize; i++) {
		mIndex = size_t(mIndex + 1) == mPoolSize ? 0 : (mIndex + 1);
		if (mPool[mIndex].state == AVAILABLE) {
			mPool[mIndex].state = USED;
			mAvailables--;

			return mPool[mIndex].addr;
		}
	}

	LOGERR("no item available in pool, this can't be possible");
	QASSERT(0); // never get here

	return NULL;
}

int SBufferPool::turn_back(unsigned char *addr) {
	SMutex::Autolock autoLock(mLock);

	if (!mIsReady)
		return -1;

	for (size_t i = 0; i < mPoolSize; i++) {
		if (mPool[i].addr == addr && mPool[i].state == USED) {
			mPool[i].state = AVAILABLE;
			mAvailables++;

			if (mAvailables == 1) { // if just returned 1
				mReturnCond.signal();
			}

			return 0;
		}
	}

	return -1;
}

int SBufferPool::turn_back(int item_id) {
	SMutex::Autolock autoLock(mLock);

	if (!mIsReady) {
		return -1;
	}

	if (mPool[item_id].state != USED) {
		return -1;
	}

	mPool[item_id].state = AVAILABLE;
	mAvailables++;

	if (mAvailables == 1) { // if just returned 1
		mReturnCond.signal();
	}

	return 0;
}
