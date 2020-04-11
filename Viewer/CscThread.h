#pragma once

#include "FrmProvideThread.h"

class CscThread : public FrmProvideThread {
public:
	CscThread(SBufferPool * pBufferPool,
			SSafeCQ<BufferInfo> *pBufferQueue,
			long *pPlayFrameID,
			QImageProcessor *pBgr888Processor);

	virtual ~CscThread();

	void deinit();

	bool setupDetail(CViewerDoc *pDoc);
	void sendQuitMsg(long frameID);
	bool loadOrigBuf(long frameID, BYTE *buf);

private:
	CFile mFile;
};
