#include <opencv2/imgproc/imgproc.hpp>

#include <QCommon.h>
#include <QCvUtil.h>

#include "QFeatureExtraction.h"
#include "QDerivative.h"

using namespace cv;
using namespace std;

namespace q1 {

FeatureExtractor::FeatureExtractor(int shrink, int nOrients,
	float grdSmooth, float chnSmooth, float simSmooth,
	int nGrad, int nSrcChns, bool useI)
: mShrink(shrink)
, mNumOrients(nOrients)
, mGrdSmooth(grdSmooth)
, mChnSmooth(chnSmooth)
, mSimSmooth(simSmooth)
, mNumGrad(nGrad)
, mNumSrcChns(nSrcChns)
, mUseI(useI)
, mNumChns((mUseI ? mNumSrcChns : 0) /*LUV*/ + mNumGrad * (1 /*mag*/ + mNumOrients))
, mNormRad(4.f)
, mMaxi(1.f / 270)
, mMinu(-88 * mMaxi)
, mMinv(-134 * mMaxi)
, mUn(0.197833f)
, mVn(0.468331f)

, mAcMult(10000.f)
{
	const float a  = QCUBE(29.0f) / 27;
	const float y0 = QCUBE(6.0f) / QCUBE(29);
	mX[0] = 0.430574f; mX[1] = 0.341550f; mX[2] = 0.178325f;
	mY[0] = 0.222015f; mY[1] = 0.706655f; mY[2] = 0.071330f;
	mZ[0] = 0.020183f; mZ[1] = 0.129553f; mZ[2] = 0.939180f;

	// build (padded) lookup table for y->l conversion assuming y in [0,1]
	int i = 0;
	for (; i < 1025; i++) {
		float y = i / 1024.f;
		float l = y > y0 ? 116*(float)pow((double)y, 1.0/3.0) - 16 : y*a;
		m2LuvTable[i] = l * mMaxi;
	}
	const float last = m2LuvTable[i - 1];
	for (; i < ARRAY_SIZE(m2LuvTable); i++)
		m2LuvTable[i] = last;

	// arc cosine for gradient orientation
	const int n = int(mAcMult), b = 10;
	mAcosTable = new float[n * 2 + b * 2];
	mAcosTableC = mAcosTable + n + b;
	for (i = -n - b; i < -n; i++)
		mAcosTableC[i] = float(CV_PI) - 1e-6f;
	for (i = -n; i < n; i++)
		mAcosTableC[i] = QMIN(acos(i / float(n)), float(CV_PI) - 1e-6f);
	for (i = n; i < n + b; i++ )
		mAcosTableC[i] = 0;
}

/*!
 * From OpenCV (structued_edge_detection.cpp)
 * The function implements rgb to luv conversion in a way similar
 * to UCSD computer vision toolbox
 *
 * \param src : source image (RGB, float, in [0;1]) to convert
 * \return converted image in luv colorspace
 */
Mat FeatureExtractor::rgb2luv(const Mat &src) const
{
	Mat dst(src.size(), src.type());

	for (int i = 0; i < src.rows; ++i) {
		const float *pSrc = src.ptr<float>(i);
		float *pDst = dst.ptr<float>(i);
		for (int j = 0; j < src.cols*3; j += 3) {
			const float rgb[] = {pSrc[j + 2], pSrc[j + 1], pSrc[j + 0]};
			const float xyz[] = {mX[0]*rgb[0] + mX[1]*rgb[1] + mX[2]*rgb[2],
								 mY[0]*rgb[0] + mY[1]*rgb[1] + mY[2]*rgb[2],
								 mZ[0]*rgb[0] + mZ[1]*rgb[1] + mZ[2]*rgb[2]};
			const float nz = 1.0f / float(xyz[0] + 15*xyz[1] + 3*xyz[2] + 1e-35);
			const float l = pDst[j] = m2LuvTable[int(1024*xyz[1])];
			pDst[j + 1] = l * (13*4*xyz[0]*nz - 13*mUn) - mMinu;;
			pDst[j + 2] = l * (13*9*xyz[1]*nz - 13*mVn) - mMinv;
		}
	}

	return dst;
}

void FeatureExtractor::gradientMagOrient(const Mat &src, Mat &mag, Mat &orient) const
{
	Mat Gx, Gy;
	const int h = src.rows;
	const int w = src.cols;
	const int c = src.channels();
	CV_Assert(c == 3 || c == 1);
	const int imgSize = h * w;

	gradient(src, Gx, Gy, CV_32F, QCV_DIFF_CENTRAL);

	// compute gradients (Gx, Gy) with maximum squared magnitude (mag)
	mag.create(src.size(), CV_32F);
	orient.create(src.size(), CV_32F);
	if (c == 3) {
		for (int i = 0; i < imgSize; i++) {
			float maxMag = 0;
			int maxK = 0;
			for (int k = 0; k < 3; k++) {
				float magF = QSQR(Gx.at<Vec3f>(i)[k]) + QSQR(Gy.at<Vec3f>(i)[k]);
				if (magF > maxMag) {
					maxMag = magF;
					maxK = k;
				}
			}
			// compute gradient magnitude (mag) and normalize Gx
			float m = QMIN(1/sqrt(maxMag), 1e10f);
			mag.at<float>(i) = 1 / m;
			float o = Gx.at<Vec3f>(i)[maxK] * m * mAcMult;
			if (Gy.at<Vec3f>(i)[maxK] < 0)
				o = -o;
			// compute and store gradient orientation (O) via table lookup
			orient.at<float>(i) = mAcosTableC[int(o)];
		}
	} else {
		for (int i = 0; i < imgSize; i++) {
			float magF = QSQR(Gx.at<float>(i)) + QSQR(Gy.at<float>(i));
			// compute gradient magnitude (mag) and normalize Gx
			float m = QMIN(1/sqrt(magF), 1e10f);
			mag.at<float>(i) = 1 / m;
			float o = Gx.at<float>(i) * m * mAcMult;
			if (Gy.at<float>(i) < 0)
				o = -o;
			// compute and store gradient orientation (O) via table lookup
			orient.at<float>(i) = mAcosTableC[int(o)];
		}
	}

	if (mNormRad == 0)
		return;

	Mat magBlurred;
	convTri(mag, magBlurred, mNormRad);
	for (int i = 0; i < imgSize; i++)
		mag.at<float>(i) /= magBlurred.at<float>(i) + 0.01f /*norm contrast*/;
}

static void gradientHist(const Mat &mag, const Mat &orient, const int binSize, Mat *hists, int nOrients)
{
	const int h = mag.rows;
	const int w = mag.cols;
	const int hb = mag.rows / binSize;
	const int wb = mag.cols / binSize;
	const float norm = 1.f / binSize / binSize;
	const float oMult = float(nOrients / CV_PI);

	for (int i = 0; i < nOrients; i++) {
		hists[i].create(hb, wb, CV_32F);
		hists[i].setTo(0.f);
	}

	for (int i = 0; i < h; i++) {
		for (int j = 0; j < w; j++) {
			float o = orient.at<float>(i, j) * oMult;
			int o0 = int(o);
			float od = o - o0;
			o0 = o0 % nOrients;
			int o1 = (o0 + 1) % nOrients;
			float m = mag.at<float>(i, j) * norm;
			float residual = od * m;

			const int ib = i / binSize;
			const int jb = j / binSize;
			hists[o0].at<float>(ib, jb) += m - residual;
			hists[o1].at<float>(ib, jb) += residual;
		}
	}
}

void FeatureExtractor::getFeatures(const Mat &img, vector<Mat> &chns) const
{
	Mat src, srcShrinked;
	CV_Assert(CV_MAT_DEPTH(img.type()) == DataType<float>::type);
	if (mNumSrcChns == 3)
		src = rgb2luv(img);
	else
		src = img;

	// Feature Extraction
	Mat ftrSrc, orient;
	resize(src, srcShrinked, Size(), 1.f/mShrink, 1.f/mShrink);
	chns.resize(mNumChns);
	size_t cIdx = 0;
	if (mUseI) {
		split(srcShrinked, &chns[0]);
		cIdx = mNumSrcChns;
	}

	Mat blurred;
	for (int i = 1, s = 1; i <= mNumGrad; i++, s <<= 1) {
		ftrSrc = (s == mShrink) ? srcShrinked : src;
		convTri(ftrSrc, blurred, mGrdSmooth);

		// Compute gradient magnitude and orientation at each location
		Mat mag;
		gradientMagOrient(blurred, mag, orient);
		resize(mag, chns[cIdx++], Size(), double(s)/mShrink, double(s)/mShrink);

		// Compute nOrients gradient histograms per binSize x binSize block of pixels:
		gradientHist(mag, orient, QMAX(1, mShrink / s), &chns[cIdx], mNumOrients);
		cIdx += mNumOrients;
	}
}

void FeatureExtractor::getSmoothFtrs(const vector<Mat> &chns,
									 vector<Mat> &chnsReg, vector<Mat> &chnsSim) const
{
	chnsReg.resize(mNumChns);
	chnsSim.resize(mNumChns);
	for (int i = 0; i < mNumChns; i++) {
		convTri(chns[i], chnsReg[i], mChnSmooth / mShrink);
		convTri(chns[i], chnsSim[i], mSimSmooth / mShrink);
	}
}

void FeatureExtractor::getSmoothFtrs(const vector<Mat> &chns, vector<Mat> &chnsSim) const
{
	chnsSim.resize(mNumChns);
	for (int i = 0; i < mNumChns; i++)
		convTri(chns[i], chnsSim[i], mSimSmooth / mShrink);
}

} // namespace q1
