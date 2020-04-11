#pragma once

#include "SThread.h"

#include "QPort.h"
#include "SSafeCQ.h"

#include "ViewerDoc.h"

class QImageProcessor;

class FrmProvideThread : public SThread
{
public:
	FrmProvideThread(SBufferPool * pBufferPool,
					SSafeCQ<BufferInfo> *pBufferQueue,
					long *pPlayFrameID,
					QImageProcessor *pBgr888Processor)
	: mBufferQueue(pBufferQueue)
	, mBufferPool(pBufferPool)
	, mPlayFrameIdPtr(pPlayFrameID)
	, mFrames(0)
	, mOrigBuf(NULL)
	, mOrigBufSize(0)
	, mBufOffset2(0)
	, mBufOffset3(0)
	, mBgr888Processor(pBgr888Processor)
	, mRot(QROT_000)
	{}

	virtual ~FrmProvideThread(void)
	{
		if (mOrigBuf)
			delete [] mOrigBuf;
	}

	inline long GetNextFrameID()
	{
		return qcmn_atomic_inc(mPlayFrameIdPtr);
	}

	inline bool setup(CViewerDoc *pDoc)
	{
		mW = pDoc->mW;
		mH = pDoc->mH;
		mRot = pDoc->mRot;
		if (mRot == QROT_090 || mRot == QROT_270)
			QSWAP(mH, mW);

		mOrigSceneSize = pDoc->mOrigSceneSize;
		if (mOrigBufSize < mOrigSceneSize) {
			if (mOrigBuf)
				delete [] mOrigBuf;

			mOrigBufSize = mOrigSceneSize;
			mOrigBuf = new BYTE[mOrigBufSize];
		}
		mBufOffset2 = pDoc->mBufOffset2;
		mBufOffset3 = pDoc->mBufOffset3;

		mColorSpace = pDoc->mColorSpace;
		mCsc2Rgb888 = pDoc->mCsc2Rgb888;

		return setupDetail(pDoc);
	}

	virtual bool threadLoop();

protected:
	virtual bool setupDetail(CViewerDoc *pDoc) = 0;
	virtual void sendQuitMsg(long frameID) = 0;
	virtual bool loadOrigBuf(long frameID, BYTE *buf) = 0;

	SSafeCQ<BufferInfo> *mBufferQueue;
	SBufferPool *mBufferPool;
	
	int mW, mH;
	long mFrames;
	long *mPlayFrameIdPtr;

	BYTE *mOrigBuf;
	size_t mOrigBufSize;
	size_t mOrigSceneSize;

	int mColorSpace;
	int mBufOffset2, mBufOffset3;
	QIMAGE_CSC_FN mCsc2Rgb888;
	QImageProcessor *mBgr888Processor;
	QROTATION mRot;
};
