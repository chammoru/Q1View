#ifndef SVSC_FILTER_H_
#define SVSC_FILTER_H_

#include "Filter.h"
#include "svsc.h"

class SvscFilter : public Filter
{
public:
	SvscFilter(char* name);
	~SvscFilter();

	virtual AvatError start(MediaConfig* mediaConfig);
	virtual AvatError stop();
	virtual AvatError process(NodeBuffer* buffer);

private:
	const char* mFaceBinPath;

	int mWidth;
	int mHeight;

	int  mType;
	int* mCost;
	int  mTotalFrame;
	long long* mPosition;

	DbTable*   mDbFrameTable;
	DbTable*   mDbFaceTable;
	TableQueue* mTableQueue;
};

#endif // SVSC_FILTER_H_

