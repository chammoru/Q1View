#pragma once

#include "FrmProvideThread.h"

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>

class FileFinderThread : public FrmProvideThread {
public:
	FileFinderThread(SBufferPool * pBufferPool,
		SSafeCQ<BufferInfo> *pBufferQueue,
		long *pPlayFrameID,
		QImageProcessor *pBgr888Processor);

	virtual ~FileFinderThread();

	bool setupDetail(CViewerDoc *pDoc);
	void sendQuitMsg(long frameID);
	bool loadOrigBuf(long frameID, BYTE *buf);

private:
	CFileFind mFinder;
	cv::Mat mOcvMat;
	BOOL mWorking;
};
