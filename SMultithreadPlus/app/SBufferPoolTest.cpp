#include <cstdio>

#include "QPort.h"
#include "SMutex.h"
#include "SCondition.h"
#include "SThread.h"
#include "SBufferPool.h"

#ifdef _WIN32
#include "vld.h"
#endif

class Thread1 : public SThread {
public:
	Thread1(SBufferPool *p) {
		mP = p;
	}

	virtual ~Thread1() {

	}

protected:
	bool threadLoop() {
		printf("checkouting\n");
		unsigned char *addr = mP->checkout();
		printf("checkouted %p\n", addr);

		QMSleep(100);
		return true;
	}

private:
	SBufferPool *mP;
};

int main()
{
	SBufferPool bufferPool(30);

	bufferPool.setup(512);

	Thread1 t1(&bufferPool);

	t1.run();

	QMSleep(3000);

	unsigned char *addr = 0;
	do {
		int ret = scanf("%x", (unsigned int*)&addr);
		if (addr == 0 || ret == 0)
			break;
		bufferPool.turn_back(addr);
	} while (1);

	bufferPool.disable();

	t1.requestExitAndWait();

	// second time

	bufferPool.setup(128);

	t1.run();

	QMSleep(3000);

	do {
		int ret = scanf("%x", (unsigned int*)&addr);
		if (addr == 0 || ret == 0)
			break;
		bufferPool.turn_back(addr);
	} while (1);

	bufferPool.disable();

	t1.requestExitAndWait();

	return 0;
}
