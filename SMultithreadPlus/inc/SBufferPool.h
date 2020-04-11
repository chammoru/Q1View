#ifndef __SBUFFER_POOL_H__
#define __SBUFFER_POOL_H__

#include "QPort.h"
#include "QDebug.h"

#include "SMutex.h"
#include "SCondition.h"

// not consider only small pool (the # of iteam is less than 50)

enum ItemState {
	AVAILABLE,
	USED,
};

struct Item {
	ItemState state;
	unsigned char *addr;
};

class SBufferPool {
public:
	SBufferPool(size_t poolSize);
	~SBufferPool();

	void setup(size_t bufSize);
	void enable(unsigned char *last_addr);
	void disable();
	unsigned char *checkout();
	int turn_back(unsigned char *addr);
	int turn_back(int item_id);

private:
	inline void init() {
		for (size_t i = 0; i < mPoolSize; i++) {
			mPool[i].state = AVAILABLE;
		}
	}

	inline void freeItems() {
		for (size_t i = 0; i < mPoolSize; i++) {
			if (mPool[i].addr)
				delete [] mPool[i].addr;
		}
	}

	inline void allocItems() {
		for (size_t i = 0; i < mPoolSize; i++) {
			mPool[i].addr = NEW unsigned char[mBufferSize];
			QASSERT(mPool[i].addr != NULL);
		}
	}

	size_t mPoolSize;
	size_t mAvailables;
	size_t mBufferSize;
	Item *mPool;
	SMutex mLock;
	bool mIsReady;
	SCondition mReturnCond;
	int mIndex;
};

#endif // __SBUFFER_POOL_H__
