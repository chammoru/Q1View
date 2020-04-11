#include <cstdio>
#include <cassert>
#include "SParcel.h"

using namespace std;

// compile with -Wno-multichar
// should be at most 4 characters
enum {
	kByteExample     = 'btex',
	kIntExample      = 'itex',
	kLongLongExample = 'llex',
	kStringExample   = 'stex',
	kPointerExample  = 'ptex',
};

int main()
{
	SParcel parcel;
	bool ret;

	unsigned char z;
	parcel.setByte(kByteExample, 0x12);
	ret = parcel.findByte(kByteExample, &z);
	assert(ret == true);
	printf("0x%x\n", z);
	ret = parcel.eraseByte(kByteExample);
	assert(ret == true);
	ret = parcel.findByte(kByteExample, &z);
	assert(ret == false);

	int a;
	parcel.setInt(kIntExample, 123483);
	ret = parcel.findInt(kIntExample, &a);
	assert(ret == true);
	printf("%d\n", a);
	ret = parcel.eraseInt(kIntExample);
	assert(ret == true);
	ret = parcel.findInt(kIntExample, &a);
	assert(ret == false);

	long long b;
	parcel.setLlong(kLongLongExample, 0x1234123411LL);
	ret = parcel.findLlong(kLongLongExample, &b);
	assert(ret == true);
	printf("0x%llx\n", b);
	ret = parcel.eraseLlong(kLongLongExample);
	assert(ret == true);
	ret = parcel.findLlong(kLongLongExample, &b);
	assert(ret == false);

	string c;
	parcel.setString(kStringExample, "hello world");
	ret = parcel.findString(kStringExample, &c);
	assert(ret == true);
	printf("%s\n", c.c_str());
	ret = parcel.eraseString(kStringExample);
	assert(ret == true);
	ret = parcel.findString(kStringExample, &c);
	assert(ret == false);

	void *d;
	parcel.setPointer(kPointerExample, (void *)0x7ff39840);
	ret = parcel.findPointer(kPointerExample, &d);
	assert(ret == true);
	printf("%p\n", d);
	ret = parcel.erasePointer(kPointerExample);
	assert(ret == true);
	ret = parcel.findPointer(kPointerExample, &d);
	assert(ret == false);

	return 0;
}

