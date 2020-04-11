#pragma once

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>

#include "FrmProvideThread.h"

class VidCapThread : public FrmProvideThread
{
public:
	VidCapThread(SBufferPool * pBufferPool,
					SSafeCQ<BufferInfo> *pBufferQueue,
					cv::VideoCapture &vidCap,
					long *pPlayFrameID,
					QImageProcessor *pBgr888Processor);

	virtual ~VidCapThread();

	bool setupDetail(CViewerDoc *pDoc);
	void sendQuitMsg(long frameID);
	bool loadOrigBuf(long frameID, BYTE *buf);

	cv::VideoCapture &mVidCap;
};
