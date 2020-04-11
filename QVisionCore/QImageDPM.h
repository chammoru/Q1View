// Copyright (C) 2014 Kyuwon Kim <chammoru@gmail.com>

#ifndef __QIMAGE_DPM__
#define __QIMAGE_DPM__

#include <vector>
#include <list>
#include <opencv2/core/core.hpp>
#include <opencv2/core/core_c.h>
#include <opencv2/objdetect/objdetect.hpp>
#include "SMutex.h"

// The number of elements in bin
// The number of sectors in gradient histogram building
#define NUM_SECTOR 9

// The number of levels in image resize procedure
// We need Lambda levels to resize image twice
#define LAMBDA 10

// Block size. Used in feature pyramid building procedure
#define SIDE_LENGTH 8

#define VAL_OF_TRUNCATE 0.2f

#define F_MAX FLT_MAX
#define F_MIN -FLT_MAX

#define LATENT_SVM_OK 0
#define LATENT_SVM_MEM_NULL 2
#define DISTANCE_TRANSFORM_OK 1
#define DISTANCE_TRANSFORM_GET_INTERSECTION_ERROR -1
#define DISTANCE_TRANSFORM_ERROR -2
#define DISTANCE_TRANSFORM_EQUAL_POINTS -3
#define LATENT_SVM_GET_FEATURE_PYRAMID_FAILED -4
#define LATENT_SVM_SEARCH_OBJECT_FAILED -5
#define LATENT_SVM_FAILED_SUPERPOSITION -6
#define FILTER_OUT_OF_BOUNDARIES -7
#define LATENT_SVM_TBB_SCHEDULE_CREATION_FAILED -8
#define LATENT_SVM_TBB_NUMTHREADS_NOT_CORRECT -9
#define FFT_OK 2
#define FFT_ERROR -10
#define LSVM_PARSER_FILE_NOT_FOUND -11

#define PERSON_THRESHOLD 0.15f
#define STABLE_THRESHOLD 1.33f

struct QRect
{
	QRect()
	: l(INT_MAX), t(INT_MAX), r(INT_MIN), b(INT_MIN) {}
	QRect(int _l, int _t, int _r, int _b)
	: l(_l), t(_t), r(_r), b(_b) {}

	int l, t, r, b;
};

class QSafeRect : public QRect
{
public:
	QSafeRect() : QRect(), mLock(NULL) {}
	~QSafeRect() {
		if (mLock)
			delete mLock;
	};

	inline void init() {
		mLock = new SMutex;
	}

	inline void maximize(int _l, int _t, int _r, int _b) {
		SMutex::Autolock autoLock(*mLock);

		l = std::min(_l, l);
		t = std::min(_t, t);
		r = std::max(_r, r);
		b = std::max(_b, b);
	}
private:
	SMutex *mLock;
};

// DataType: STRUCT filterDisposition
// The structure stores preliminary results in optimization process
// with objective function D
//
// x            - array with X coordinates of optimization problems solutions
// y            - array with Y coordinates of optimization problems solutions
// score        - array with optimal objective values
struct LsvmFilterDisposition
{
	LsvmFilterDisposition() : costs(NULL), Xs(NULL), Ys(NULL) {}
	~LsvmFilterDisposition()
	{
		if (costs) delete costs;
		if (Xs) delete Xs;
		if (Ys) delete Ys;
	}

    cv::Mat *costs;
    cv::Mat *Xs;
    cv::Mat *Ys;
};

struct QCvLSVMFilterPosition
{
    int x;
    int y;
    int l;
};

// DataType: STRUCT filterObject
// Description of the filter, which corresponds to the part of the object
// V               - ideal (penalty = 0) position of the partial filter
//                   from the root filter position (V_i in the paper)
// penaltyFunction - std::vector describes penalty function (d_i in the paper)
//                   pf[0] * x + pf[1] * y + pf[2] * x^2 + pf[3] * y^2
// FILTER DESCRIPTION
//   Rectangular map (sizeX x sizeY),
//   every cell stores feature std::vector (dimension = p)
// H               - matrix of feature vectors
//                   to set and get feature vectors (i,j)
//                   used formula H[(j * sizeX + i) * p + k], where
//                   k - component of feature std::vector in cell (i, j)
// END OF FILTER DESCRIPTION
struct LsvmFilter
{
	LsvmFilter() : mRealRatio(0.f), mConvDone(NULL), mProbeDone(NULL),
		mF(NULL), mDisposition(NULL) {}
	~LsvmFilter() { clear(); }
	void clear()
	{
		if (mConvDone) {
			delete [] mConvDone;
			mConvDone = NULL;
		}
		if (mF) {
			delete [] mF;
			mF = NULL;
		}
		if (mDisposition) {
			delete [] mDisposition;
			mDisposition = NULL;
		}
		if (mProbeDone) {
			delete [] mProbeDone;
			mProbeDone = NULL;
		}

		mRealH.clear();
		mRealW.clear();
	}

    QCvLSVMFilterPosition V;
    float fineFunction[4];
    int sizeX, sizeY;
	float mRealRatio;
    int numFeatures;
    float *H;

	cv::Mat *mConvDone;
	cv::Mat *mProbeDone;

    // save a dot product vectors of featureMap and weights of filter
	cv::Mat *mF; // filter response
	std::vector<int> mRealW, mRealH;

	// only for partFilter
	LsvmFilterDisposition *mDisposition;
};

// DataType: STRUCT featureMat
// FEATURE MAP DESCRIPTION
//   Rectangular map (sizeX x sizeY),
//   every cell stores feature std::vector (dimension = mNumFeatures)
// map             - matrix of feature vectors
//                   to set and get feature vectors (i,j)
//                   used formula featureMat[(j * sizeX + i) * p + k], where
//                   k - component of feature std::vector in cell (i, j)
struct LsvmFeatureMap
{
	LsvmFeatureMap(const int sizeX, const int sizeY, const int numFeatures)
	: mSizeX(sizeX), mSizeY(sizeY), mNumFeatures(numFeatures)
	{
		int numElements = mSizeX * mSizeY  * mNumFeatures;
		mFeatureMat = new float[numElements];

		memset(mFeatureMat, 0, sizeof(float) * numElements);
	}

	~LsvmFeatureMap() { delete [] mFeatureMat; }

	int mSizeX;
	int mSizeY;
	int mNumFeatures;
	float *mFeatureMat;
};

// DataType: STRUCT featurePyramid
//
// numLevels    - number of levels in the feature pyramid
// mFeatureMaps  - array of pointers to feature map at different levels
struct LsvmFeaturePyramid
{
	LsvmFeaturePyramid(const int numLevels)
	: mNumLevels(numLevels)
	, mFeatureMaps(new LsvmFeatureMap *[mNumLevels]) {}

	~LsvmFeaturePyramid()
	{
		for (int i = 0; i < mNumLevels; i++) {
			if (mFeatureMaps[i])
				delete mFeatureMaps[i];
		}
		delete [] mFeatureMaps;
	}

    int mNumLevels;
    LsvmFeatureMap **mFeatureMaps;
};

struct MatchPos
{
	MatchPos(float _score, const cv::Point &_pt, std::vector<cv::Point> *pd, int _level)
	: score(_score), pt(_pt), partsDisplacement(pd), level(_level) {}
	~MatchPos()
	{
		if (partsDisplacement)
			delete partsDisplacement;
	}

	float score;
	cv::Point pt;
	std::vector<cv::Point> *partsDisplacement;
	int level;
};

struct DetectionResult
{
	DetectionResult() : points(NULL), oppPoints(NULL), scores(NULL), count(0) {}
	~DetectionResult() { clear(); }

	void clear() {
		if (points) delete [] points;
		if (oppPoints) delete [] oppPoints;
		if (scores) delete [] scores;
	}

	void create(int _count) {
		clear();
		count = _count;
		points = new cv::Point[_count];
		oppPoints = new cv::Point[_count];
		scores = new float[_count];
	}

	cv::Point *points;
	cv::Point *oppPoints;
	float *scores;
	int count;
};

struct FeatureMapCoord
{
	int compIdx;
	int level;
	cv::Point pt; // Root Coordinates
};

// Idea from the OpenCV TickMeter
class QThinMeter
{
public:
	QThinMeter() { reset(); }
	void start() { mStartTime = cvGetTickCount(); }
	void stop()
	{
		if (mStartTime == 0)
			return;

		mStartTime = 0;
	}

	inline double getCurMilli() const
	{
		int64 time = cvGetTickCount();
		double passTime = double(time - mStartTime);
		return (passTime / cvGetTickFrequency()) * 1e-3;
	}

	void reset() { mStartTime = 0; }

private:
	int64 mStartTime;
};

struct QCvObjectDetection
{
    CvRect rect;
    float score;
};

// data type: STRUCT QLsvmDetector
// structure contains internal representation of trained Latent SVM detector
// mNumFilters          - total number of filters (root plus part) in model
// mNumComponents       - number of components in model
// mNumPartFilters     - array containing number of part filters for each component
// mFilters              - root and part filters for all model components
// b                    - biases for all model components
// mScoreThreshold      - confidence level threshold
class QLsvmDetector
{
public:
	QLsvmDetector(int numFilters, int numComponents, int *numPartFilters,
	LsvmFilter **filters, float *b, float scoreThreshold, float ratioThreshold);

	~QLsvmDetector();

	void clear();
	void detect(std::vector<QCvObjectDetection> &detections, int w, int h,
		float overlapThreshold, std::vector<FeatureMapCoord> *ftrMapCoords);

	int searchObjectThresholdSomeComponents(std::vector<DetectionResult> &dr);
	int searchObjectComponentsWithOrder(std::vector<DetectionResult> &dr, std::vector<FeatureMapCoord> *ftrMapCoords);

	int searchObjectThreshold(LsvmFilter **compFilters, int numPartFilter, float b,
		std::vector<MatchPos *> &matchPoses);

	int thresholdFunctionalScore(LsvmFilter **compFilters, int numPartFilter,
								 float b, std::vector<MatchPos *> &matchPoses);

	int thresholdFunctionalScoreFixedLevel(LsvmFilter **compFilters, int numPartFilter,
										   int level, float b, std::vector<MatchPos *> &matchPoses);
	int calConvolutions(LsvmFilter **compFilters, int numPartFilter,
                        int level, const cv::Point *coord, std::vector<QSafeRect> *limits);

	int calDistanceAndScore(LsvmFilter **compFilters, int numPartFilter, float b, int level,
		std::vector<MatchPos *> &matchPoses, std::vector<QSafeRect> *limits);

	LsvmFeatureMap *featureMapBorderPartFilter(LsvmFeatureMap *featureMap);

	int convertPoints(int lambda, std::vector<MatchPos *> &matchPoses);

	void packAndPrepareBuffers();

	int calMaxFilterDims();
	void createFeaturePyramidWithBorder(const cv::Mat &image);
	int getFeaturePyramid(const cv::Mat &image);
	int estimateBoxes(std::vector<MatchPos *> &matchPoses, cv::Point * &oppositePoints,
		int sizeX, int sizeY);
	void cvtBox2FtrMapCoord(std::vector<QRect> *searchBoxes, std::vector<FeatureMapCoord> *ftrMapCoords);
	void genFullFtrMapCoord(std::vector<FeatureMapCoord> *ftrMapCoords);
	void genRandFtrMapCoord(std::vector<FeatureMapCoord> *ftrMapCoords);
	void genCenteredFtrMapCoord(std::vector<FeatureMapCoord> *ftrMapCoords);

	inline void setTimeLimit(double timeLimit) { mTimeLimitMs = timeLimit; }
	inline void setRatioThreshold(float ratioThreshold) { mRatioThreshold = ratioThreshold; }
	inline void setClassName(const std::string &className) { mClassName = className; }

private:
    int mNumFilters;
    int mNumComponents;
	std::vector<LsvmFilter *> mRootFilters;
    float *mB;
    float mScoreThreshold;

	LsvmFeaturePyramid *mPyramid;
	LsvmFeaturePyramid *mPartPyramid;

	unsigned int mMaxXBorder;
	unsigned int mMaxYBorder;

	int mXBorder;
	int mYBorder;

	float mRatioThreshold;
	float mStep;
	std::vector<float> mScales;
	std::string mClassName;

public:
	int *mNumPartFilters;
	LsvmFilter **mFilters;
	std::vector<int> mRootFilterIdxs;

	double mTimeLimitMs; // time-limit in milli seconds
	QThinMeter mTm;
	long mTimeCount;
};

/*
 * This is a class wrapping up the structure QLsvmDetector and functions working with it.
 * The class goals are:
 * 1) provide c++ interface;
 * 2) make it possible to load and detect more than one class (model) unlike QLsvmDetector.
 */
class QUniLsvmDetector
{
public:
    struct ObjectDetection
    {
		ObjectDetection() : score(0.f), classID(-1) {}
		ObjectDetection(const cv::Rect& _rect, float _score, int _classID = -1)
			: rect(_rect), score(_score), classID(_classID) {}
        cv::Rect rect;
        float score;
        int classID;
    };

	QUniLsvmDetector() {};
    QUniLsvmDetector(const std::vector<std::string>& filenames,
		const std::vector<std::string> &_classNames = std::vector<std::string>())
	{
		load(filenames, _classNames);
	}
	~QUniLsvmDetector() { clear(); };

    void clear();
	bool empty() const { return detectors.empty(); }
    bool load(const std::vector<std::string>& filenames, const std::vector<std::string>& classNames=std::vector<std::string>());

	void setup(const cv::Mat &image, float overlapThreshold = 0.5f, double timeLimitMs = -1.);
    void detect(std::vector<ObjectDetection> &objectDetections,
		std::vector<std::vector<FeatureMapCoord> *> &fmcss);
	const std::vector<std::string>& getClassNames() const { return classNames; }
	size_t getClassCount() const { return classNames.size(); }
	void cvtBox2FtrMapCoord(std::vector<QRect> *searchBoxes, std::vector<FeatureMapCoord> *ftrMapCoords);
	void genFullFtrMapCoord(std::vector<FeatureMapCoord> *ftrMapCoords);
	void genRandFtrMapCoord(std::vector<FeatureMapCoord> *ftrMapCoords);
	void genCenteredFtrMapCoord(std::vector<FeatureMapCoord> *ftrMapCoords);
	void setTimeLimit(double timeLimit);
	void setRatioThreshold(float ratioThreshold);

private:
	void finish();

    std::vector<QLsvmDetector *> detectors;
    std::vector<std::string> classNames;
	int mW;
	int mH;
	float mOverlapThreshold;
};

#endif /* __QIMAGE_DPM__ */
