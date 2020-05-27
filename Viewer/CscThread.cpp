#include "stdafx.h"
#include "CscThread.h"

#include "qimage_cs.h"

#include "QCommon.h"

#include "QOcv.h"

CscThread::CscThread(SBufferPool * pBufferPool,
					 SSafeCQ<BufferInfo> *pBufferQueue,
					 long *pPlayFrameID,
					 q1::ImageProcessor *pBgr888Processor)
: FrmProvideThread(pBufferPool, pBufferQueue, pPlayFrameID, pBgr888Processor)
{
}

CscThread::~CscThread()
{
	deinit();
}

bool CscThread::setupDetail(CViewerDoc *pDoc)
{
	CFileException e;
	BOOL ok	= mFile.Open(pDoc->mPathName,
		CFile::modeRead | CFile::shareDenyNone | CFile::typeBinary, &e);
	if (!ok) {
		e.ReportError();
		return false;
	}

	mFrames = long(mFile.GetLength() / mOrigSceneSize);

	return true;
}

void CscThread::sendQuitMsg(long frameID)
{
	if (frameID != mFrames - 1 + NUM_THREADS)
		return;

	BufferInfo bi;
	bi.ID = MSG_QUIT;
	bi.addr = 0;

	mBufferQueue->push(bi);
}

bool CscThread::loadOrigBuf(long frameID, BYTE *buf)
{
	if (frameID >= mFrames)
		return false;

	ULONGLONG pos = ULONGLONG(frameID) * mOrigSceneSize;
	mFile.Seek(pos, CFile::begin);

	mFile.Read(buf, UINT(mOrigSceneSize));

	return true;
}

void CscThread::deinit()
{
	if (mFile.m_hFile != CFile::hFileNull)
		mFile.Close();
}

