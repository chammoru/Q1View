#pragma once

#include "FrmProvideThread.h"
#include "QOcv.h"

class CscThread : public FrmProvideThread {
public:
	CscThread(SBufferPool * pBufferPool,
			SSafeCQ<BufferInfo> *pBufferQueue,
			long *pPlayFrameID,
			q1::ImageProcessor *pBgr888Processor);

	virtual ~CscThread();

	void deinit();

	bool setupDetail(CViewerDoc *pDoc);
	void sendQuitMsg(long frameID);
	bool loadOrigBuf(long frameID, BYTE *buf);

private:
	CFile mFile;
};
