#pragma once

#include <list>
#include "QCommon.h"

using namespace std;

// Run-Length Coding for storing line-diff info.
struct RLC
{
	bool mSame;
	long mCount;

	RLC(bool same, long count) : mSame(same), mCount(count) {};
};

void QCalRLC(qu8 *src, qu8 *dst, int w, int h,
			 int stride, int px_w, list<RLC> *diffRLC);
