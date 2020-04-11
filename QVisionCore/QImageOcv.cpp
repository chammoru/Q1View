// Copyright (C) 2014 Kyuwon Kim <chammoru@gmail.com>

#include "QDebug.h"
#include "QTickMeter.h"
#include "QImageOcv.h"
#include "QDerivative.h"

#include <opencv2/objdetect/objdetect.hpp>
#define SOBEL_MAX 2040.f
#define USE_SOBEL 0

#if USE_SOBEL
#define DERIV_MAX SOBEL_MAX
#else
#define DERIV_MAX 255
#endif

#define SQRT2 1.41421356237f

static const QOcvOrientInfo qOoi;

static inline void QOcvIntegralImage(cv::Mat &src, cv::Mat &dst)
{
	float strSum;
	int rows = src.rows;
	int cols = src.cols;

	dst.create(rows, cols, CV_32F);

	float *imgSrc = src.ptr<float>(0);
	float *imgDst = dst.ptr<float>(0);

	*imgDst++ = *imgSrc++;

	strSum = 0.f;
	for (int j = 1; j < cols; j++) {
		strSum += *imgSrc++;
		*imgDst++ = strSum;
	}

	for (int i = 1; i < rows; i++) {
		*imgDst = imgDst[-cols] + *imgSrc++;
		imgDst++;

		strSum = 0.f;
		for (int j = 1; j < cols; j++) {
			strSum += *imgSrc++;
			*imgDst = imgDst[-cols] + strSum;
			imgDst++;
		}
	}
}

void QOcvIntegralOrientHist(cv::Mat &image, cv::Mat OrientIntegral[NUM_ORIENT_BIN])
{
	int rows = image.rows;
	int cols = image.cols;

	cv::Mat dx;
	cv::Mat dy;

#if USE_SOBEL
	cv::Sobel(image, dx, CV_16S, 1, 0, 3, 1, 0, cv::BORDER_REPLICATE);
	cv::Sobel(image, dy, CV_16S, 0, 1, 3, 1, 0, cv::BORDER_REPLICATE);
#else
	q1::gradient(image, dx, dy, CV_16S, QCV_DIFF_CENTRAL);
#endif

	cv::Mat matAngles[NUM_ORIENT_BIN];
	for (int i = 0; i < NUM_ORIENT_BIN; i++)
		matAngles[i] = cv::Mat::zeros(rows, cols, CV_32F);

	short *x = (short *)dx.data;
	short *y = (short *)dy.data;
	float *orientBins = qOoi.mOrientBins;
	float orientUnit = qOoi.mOrientUnit;

	for (int i = 0; i < rows; i++) {
		for (int j = 0; j < cols; j++) {
			short x_t = *x++;
			short y_t = *y++;

			float norm = sqrtf(float(y_t * y_t) + x_t * x_t) / (DERIV_MAX * SQRT2);
			if (norm == 0)
				continue;

			float ang = cv::fastAtan2(y_t, x_t);
			if (ang >= 180)
				ang -= 180;

			float weight = norm / orientUnit;
			for (int k = NUM_ORIENT_BIN - 1; k >= 0; k--) {
				float angleLevel = orientBins[k];
				if (ang < angleLevel)
					continue;

				float angDiff = ang - angleLevel;

				int upIndex = (k + 1) % NUM_ORIENT_BIN;
				int myIndex = k;

				cv::Mat *upAngMat = matAngles + upIndex;
				cv::Mat *myAngMat = matAngles + myIndex;

				upAngMat->at<float>(i, j) = weight * angDiff;
				myAngMat->at<float>(i, j) = weight * (orientUnit - angDiff);

				break;
			}
		}
	}

	for (int i = 0; i < NUM_ORIENT_BIN; i++)
		QOcvIntegralImage(matAngles[i], OrientIntegral[i]);
}

void QOcvCalPointHoG(VecBNf &ptHoG, cv::Mat IntegralAngles[NUM_ORIENT_BIN],
					 int x1, int y1, int x2, int y2)
{
	int rows = IntegralAngles[0].rows;
	int cols = IntegralAngles[0].cols;

	int ySmaller = y1 - 1;
	int xSmaller = x1 - 1;
	int yBigger = std::min(y2, rows - 1);
	int xBigger = std::min(x2, cols - 1);

	float val;
	if (xSmaller < 0 && ySmaller < 0) {
		for (int m = 0; m < NUM_ORIENT_BIN; m++) {
			val = IntegralAngles[m].at<float>(yBigger, xBigger);
			ptHoG[m] = val;
		}
	} else if (ySmaller < 0) {
		for (int m = 0; m < NUM_ORIENT_BIN; m++) {
			val = IntegralAngles[m].at<float>(yBigger, xBigger)
				- IntegralAngles[m].at<float>(yBigger, xSmaller);
			ptHoG[m] = val;
		}
	} else if (xSmaller < 0) {
		for (int m = 0; m < NUM_ORIENT_BIN; m++) {
			val = IntegralAngles[m].at<float>(yBigger, xBigger)
				- IntegralAngles[m].at<float>(ySmaller, xBigger);
			ptHoG[m] = val;
		}
	} else {
		for (int m = 0; m < NUM_ORIENT_BIN; m++) {
			val = IntegralAngles[m].at<float>(yBigger, xBigger)
				- IntegralAngles[m].at<float>(ySmaller, xBigger)
				+ IntegralAngles[m].at<float>(ySmaller, xSmaller)
				- IntegralAngles[m].at<float>(yBigger, xSmaller);
			ptHoG[m] = val;
		}
	}
}

// NOTE: We support various size of point HoG,  odd numbers 7, 9, 11, 13, ...
void QOcvCalPointHoG(VecBNf &ptHoG, cv::Mat IntegralAngles[NUM_ORIENT_BIN],
					 int x, int y, int ptHogSize)
{
	int ptHogHalf = (ptHogSize - 1) / 2;

	int y1 = y - ptHogHalf;
	int x1 = x - ptHogHalf;
	int y2 = y + ptHogHalf;
	int x2 = x + ptHogHalf;

	QOcvCalPointHoG(ptHoG, IntegralAngles, x1, y1, x2, y2);
}

bool QHoGCascade::process(cv::Mat &src, cv::Mat &dst)
{
	std::string cascadeName = "../../morucv/hogcascadeXml/hogcascade_pedestrians.xml";
	cv::CascadeClassifier detector;
	bool ok = detector.load(cascadeName);
	QASSERT(ok);

	int gr_thr = 6;
	double scale_step = 1.1;
	cv::Size min_obj_sz(48,96);
	cv::Size max_obj_sz(700,1400);

	std::vector<cv::Rect> found;
	detector.detectMultiScale(src, found, scale_step, gr_thr, 0, min_obj_sz, max_obj_sz);

	src.copyTo(dst);

	for(size_t i = 0; i < found.size(); i++)
		rectangle(dst, found[i], cv::Scalar(0, 255, 0), 2);

	return true;
}

#if 0 // Original LSVM DPM
bool QLatenSvm::process(cv::Mat &src, cv::Mat &dst)
{
	std::string model = "../ExtraData/latentsvmXml/person.xml";
	std::vector<std::string> models;

	models.push_back(model);
	cv::LatentSvmDetector detector(models);
	QASSERT(!detector.empty());

	float overlapThreshold = 0.2f;
	int numThreads = 1;

	std::vector<cv::LatentSvmDetector::ObjectDetection> detections;
	detector.detect(src, detections, overlapThreshold, numThreads);

	src.copyTo(dst);

    for (size_t i = 0; i < detections.size(); i++) {
        const cv::LatentSvmDetector::ObjectDetection& od = detections[i];
		if (od.score < 0)
			continue;
        rectangle(dst, od.rect, cv::Scalar(0, 255, 0), 2);
    }

	return true;
}
#else

//#define MORU_DEMO_PERSONNESS
#ifdef MORU_DEMO_PERSONNESS
#include "QImageDPM.h"
#include "DataSetVOC.h"
#include "Objectness.h"

using namespace std;
using namespace cv;

// Objectness
class DemoPreLoad
{
public:
	DemoPreLoad()
	{
		double base = 2;
		double intUionThr = 0.5;
		int W = 8;
		int NSS = 2;

		DataSetVOC voc2007("");
		std::vector<std::string> models;
		std::string model = "../ExtraData/latentsvmXml/person.xml";
		models.push_back(model);
		mDetector = new QUniLsvmDetector(models);
		mObjNess = new Objectness(voc2007, base, intUionThr, W, NSS);
		QASSERT(!mDetector->empty());
		mObjNess->loadTrainedModel("../ExtraData/EstimationModel/PersonNessB2W8MAXBGR");
	}
	~DemoPreLoad()
	{
		delete mDetector;
		delete mObjNess;
	}

	QUniLsvmDetector *mDetector;
	Objectness *mObjNess;
};

DemoPreLoad gDemo;

inline void qPutText(Mat &img, char *str, Point &pt)
{
	putText(img, str, pt, FONT_HERSHEY_SIMPLEX, 0.5, Scalar::all(255), 5, CV_AA);
	putText(img, str, pt, FONT_HERSHEY_SIMPLEX, 0.5, Scalar::all(0), 1, CV_AA);
}
#endif

bool QLatenSvm::process(cv::Mat &src, cv::Mat &dst)
{
#ifdef MORU_DEMO_PERSONNESS
	QTickMeter tmEst, tmSetup, tmDetEval;
	float overlapThreshold = 0.2f;

	ValStructVec<float, Vec4i> boxes;
	QUniLsvmDetector *detector = gDemo.mDetector;
	std::vector<QUniLsvmDetector::ObjectDetection> detections;

	vector<FeatureMapCoord> ftrMapCoords;
#if 0 // Bing
	int numPerSz = 130;
	double timeLimit = 10;
	tmEst.start();
	gDemo.mObjNess->getObjBndBoxes(src, boxes, numPerSz);
	tmEst.stop();

	double localTimeLimitMs = timeLimit;
	if (timeLimit > 0) {
		localTimeLimitMs -= tmEst.getTimeMilli();
		if (localTimeLimitMs < 0.)
			localTimeLimitMs = 0.;
	}

	tmSetup.start();
	detector->setup(src, overlapThreshold, localTimeLimitMs);
	tmSetup.stop();
	vector<QRect> searchBoxes;
	for (int i = 0; i < (int)boxes.size() && i < 500; i++) {
		const Vec4i &bb = boxes[i];
		QRect rt(bb[0], bb[1], bb[2], bb[3]);
		searchBoxes.push_back(rt);
	}
	detector->cvtBox2FtrMapCoord(&searchBoxes, &ftrMapCoords);
#else // Full Search
	detector->setup(src, overlapThreshold);
	detector->genFullFtrMapCoord(&ftrMapCoords);
#endif
	tmDetEval.start();
	vector<vector<FeatureMapCoord> *> fmcss;
	fmcss.push_back(&ftrMapCoords);
	detector->detect(detections, fmcss);
	tmDetEval.stop();

	src.copyTo(dst);

    for (size_t i = 0; i < detections.size(); i++) {
        const QUniLsvmDetector::ObjectDetection& od = detections[i];
		if (od.score < PERSON_THRESHOLD)
			continue;
		rectangle(dst, od.rect, cv::Scalar(0, 255, 0), 2);
    }

	char buf[256];
	Point pt(10, 20);
	sprintf(buf, "estimation: %.2f ms", tmEst.getTimeMilli());
	qPutText(dst, buf, pt);

	pt.y = 40;
	sprintf(buf, "setup: %.2f ms", tmSetup.getTimeMilli());
	qPutText(dst, buf, pt);

	pt.y = 60;
	sprintf(buf, "detection, evaluation: %.2f ms", tmDetEval.getTimeMilli());
	qPutText(dst, buf, pt);
#endif
	return true;
}
#endif
