#pragma once

#include <opencv2/core/core.hpp>

namespace q1 {

class FeatureExtractor {
	const int mShrink;      // amount to shrink channels
	const int mNumOrients;  // number of orientations per gradient scale
	const float mGrdSmooth; // radius for image gradient smoothing (using convTri)
	const float mChnSmooth; // radius for reg channel smoothing (using convTri)
	const float mSimSmooth; // radius for sim channel smoothing (using convTri)
	const int mNumGrad;
	const int mNumSrcChns;
	const bool mUseI;       // use intensity (i.e. color) or not
	const float mNormRad;   // gradient normalization radius
	const int mNumChns;     // number of channels of the feature

	const float mMaxi;
	const float mMinu;
	const float mMinv;
	const float mUn;
	const float mVn;
	float mX[3], mY[3], mZ[3];
	float m2LuvTable[1064];

	float mAcMult;
	float *mAcosTable, *mAcosTableC;

	cv::Mat rgb2luv(const cv::Mat &src) const;

public:
	FeatureExtractor(int shrink = 2, int nOrients = 4,
		float grdSmooth = 0.f, float chnSmooth = 2.f, float simSmooth = 8.f,
		int nGrad = 2, int nSrcChns = 3, bool useI = true);
	~FeatureExtractor() {
		delete [] mAcosTable;
	}
	inline int channels() const { return mNumChns; }
	void getFeatures(const cv::Mat &img, std::vector<cv::Mat> &chns) const;
	void getSmoothFtrs(const std::vector<cv::Mat> &chns,
		std::vector<cv::Mat> &chnsReg, std::vector<cv::Mat> &chnsSim) const;
	void getSmoothFtrs(const std::vector<cv::Mat> &chns,
		std::vector<cv::Mat> &chnsSim) const;
	void gradientMagOrient(const cv::Mat &src, cv::Mat &mag, cv::Mat &orient) const;
};

}
