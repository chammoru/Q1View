#include <stdio.h>
#include "SReqListener.h"

int main(int argc, char * argv[])
{
	ReqMsg msg;
	SReqListener t1("t1", 1), t2("t2", 1), t3("t3"), t4("t4", 2), t5("t5");

	t1.registerReqAudience(t2);
	t1.registerReqAudience(t3);
	t2.registerReqAudience(t4);
	t4.registerReqAudience(t5);

	t1.run();
	t2.run();
	t3.run();
	t4.run();
	t5.run();

	msg.type = ReqMsg::PUSH;
	msg.u.push_message.data = (void *)0x1;
	t1.request(msg);
	msg.type = ReqMsg::PUSH;
	msg.u.push_message.data = (void *)0x3;
	t1.request(msg);
	msg.type = ReqMsg::PUSH;
	msg.u.push_message.data = (void *)0x10;
	t1.request(msg);
	msg.type = ReqMsg::PUSH;
	msg.u.push_message.data = (void *)0x12;
	t1.request(msg);
	msg.type = ReqMsg::PUSH;
	msg.u.push_message.data = (void *)0x14;
	t1.request(msg);

	msg.type = ReqMsg::DONE;
	t1.request(msg);

	t1.join();
	t2.join();
	t3.join();
	t4.join();
	t5.join();

	printf("done\n");

	return 0;
}

