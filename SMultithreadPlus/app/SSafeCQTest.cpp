#include <cstdio>

#include "QPort.h"
#include "SMutex.h"
#include "SCondition.h"
#include "SThread.h"
#include "SSafeCQ.h"

class Thread1 : public SThread {
public:
	Thread1(SSafeCQ<int> *q) {
		mQ = q;
	}

	virtual ~Thread1() {

	}

protected:
	bool threadLoop() {
		printf("pushing\n");
		mQ->push(10);
		printf("pushed\n");
		return true;
	}

private:
	SSafeCQ<int> *mQ;
};

class Thread2 : public SThread {
public:
	Thread2(SSafeCQ<int> *q) {
		mQ = q;
	}

	virtual ~Thread2() {

	}

protected:
	bool threadLoop() {
		QMSleep(1);
		int a;
		printf("popping\n");
		mQ->pop(a);
		printf("popped\n");
		return true;
	}

private:
	SSafeCQ<int> *mQ;
};

int main()
{
	SSafeCQ<int> *q = new SSafeCQ<int>(3);

	Thread1 push1(q), push2(q);
	Thread2 pop1(q), pop2(q);

	push1.run();
	push2.run();
	pop1.run();
	pop2.run();

	QMSleep(5000);

	q->destroy();

	push1.requestExitAndWait();
	push2.requestExitAndWait();
	pop1.requestExitAndWait();
	pop2.requestExitAndWait();

	push1.join();
	push2.join();
	pop1.join();
	pop2.join();

	delete q;

	return 0;
}
