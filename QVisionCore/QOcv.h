// Copyright (C) 2014 Kyuwon Kim <chammoru@gmail.com>

#ifndef __QIMAGE_CV__
#define __QIMAGE_CV__

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>

#define NUM_ORIENT_BIN 16

typedef cv::Vec<float, NUM_ORIENT_BIN> VecBNf; // Bin #

struct QOcvOrientInfo {
	QOcvOrientInfo()
	{
		mOrientUnit = 180.f / NUM_ORIENT_BIN;
		mOrientBins = new float[NUM_ORIENT_BIN];
		for (int i = 0; i < NUM_ORIENT_BIN; i++)
			mOrientBins[i] = mOrientUnit * i;
	};

	~QOcvOrientInfo()
	{
		delete [] mOrientBins;
	}

	float *mOrientBins;
	float mOrientUnit;
};

void QOcvIntegralOrientHist(cv::Mat &image, cv::Mat OrientIntegral[NUM_ORIENT_BIN]);
void QOcvCalPointHoG(VecBNf &ptHoG, cv::Mat IntegralAngles[NUM_ORIENT_BIN],
					 int x1, int y1, int x2, int y2);
void QOcvCalPointHoG(VecBNf &ptHoG, cv::Mat IntegralAngles[NUM_ORIENT_BIN],
					 int x, int y, int ptHogSize);

class QImageProcessor {
public:
	QImageProcessor() {}
	virtual ~QImageProcessor() {}
	virtual bool process(cv::Mat &src, cv::Mat &dst) = 0;
};

class QHoGCascade : public QImageProcessor
{
	bool process(cv::Mat &src, cv::Mat &dst);
};

class QLatenSvm : public QImageProcessor
{
	bool process(cv::Mat &src, cv::Mat &dst);
};

#endif /* __QIMAGE_CV__ */
