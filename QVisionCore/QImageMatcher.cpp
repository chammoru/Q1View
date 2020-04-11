// Copyright (C) 2014 Kyuwon Kim <chammoru@gmail.com>

#include "QImageMatcher.h"
#include "QImageEpipol.h"
#include <iostream>

using namespace std;
using namespace cv;

// Clear matches for which NN ratio is > than threshold
// return the number of removed points
// (corresponding entries being cleared, i.e. size will be 0)
int RobustMatcher::ratioTest(vector<vector<cv::DMatch> >& matches)
{
	int removed = 0;

	// for all matches
	vector<vector<cv::DMatch> >::iterator matchIt = matches.begin();
	for (; matchIt != matches.end(); ++matchIt) {
		// if 2 NN has been identified
		if (matchIt->size() > 1) {
			vector<cv::DMatch> &matches = *matchIt;
			// check distance ratio
			// 1st should be much more distinctive than 2nd
			if (matches[0].distance / matches[1].distance > mRatio) {
				matchIt->clear(); // remove match
				removed++;
			}
		} else { // does not have 2 neighbours
			matchIt->clear(); // remove match
			removed++;
		}
	}

	return removed;
}

// Insert symmetrical matches in symMatches vector
void RobustMatcher::symmetryTest(const vector<vector<cv::DMatch> >& matches1,
		const vector<vector<cv::DMatch> >& matches2,
		vector<cv::DMatch>& symMatches)
{
	// for all matches image 1 -> image 2
	vector<vector<cv::DMatch> >::const_iterator matchIt1 = matches1.begin();
	for (; matchIt1 != matches1.end(); ++matchIt1) {
		if (matchIt1->size() < 2) // ignore deleted matches 
			continue;

		// for all matches image 2 -> image 1
		vector<vector<cv::DMatch> >::const_iterator matchIt2 = matches2.begin();
		for (; matchIt2 != matches2.end(); ++matchIt2) {
			if (matchIt2->size() < 2) // ignore deleted matches 
				continue;

			const vector<cv::DMatch> &matches1 = *matchIt1;
			const vector<cv::DMatch> &matches2 = *matchIt2;

			// Match symmetry test
			if (matches1[0].queryIdx == matches2[0].trainIdx  &&
				matches2[0].queryIdx == matches1[0].trainIdx) {
				// add symmetrical match
				symMatches.push_back(cv::DMatch(matches1[0].queryIdx,
							matches1[0].trainIdx,
							matches1[0].distance));
				break; // next match in image 1 -> image 2
			}
		}
	}
}

#ifdef MORU_IMPL_FMAT
const int modelPoints = 8;
cv::RNG rng(-1);

static bool checkSubset(const Mat &m, int count)
{
	// need at least 3 points
	if (count <= 2)
		return true;

	cv::Point2d *ptr = (cv::Point2d *)m.data;

	// check that the i-th selected point does not belong
	// to a line connecting some previously selected points
	int last = count - 1;
	cv::Point2d *lastPt = ptr + last;
	int j, k;
	for (j = 0; j < last; j++) {
		double dx1 = ptr[j].x - lastPt->x;
		double dy1 = ptr[j].y - lastPt->y;
		for (k = 0; k < j; k++) {
			double dx2 = ptr[k].x - lastPt->x;
			double dy2 = ptr[k].y - lastPt->y;

			// 'dx1 : dy1 = dx2 : dy2' means there are in the same line
			double tiny = FLT_EPSILON * (fabs(dx1) + fabs(dy1) + fabs(dx2) + fabs(dy2));
			if (fabs(dx2 * dy1 - dy2 * dx1) <= tiny)
				break;
		}
		if (k < j)
			break;
	}
	if (j < last)
		return false;

	return true;
}

static bool getSubset(const Mat &m1, const Mat &m2, Mat &ms1, Mat &ms2, int maxAttempts)
{
	vector<int> _idx(modelPoints);
	int *idx = &_idx[0];

	size_t elemSize = m1.elemSize() / sizeof(int);
	const int *m1ptr = (const int *)m1.data;
	const int *m2ptr = (const int *)m2.data;
	int *ms1ptr = (int *)ms1.data;
	int *ms2ptr = (int *)ms2.data;
	int count = m1.cols * m1.rows;
	int i, iters;

	for (iters = 0; iters < maxAttempts; iters++) {
		for(i = 0; i < modelPoints && iters < maxAttempts;) {
			int idx_i = idx[i] = unsigned int(rng) % count;
			int j = 0;
			for (; j < i; j++) {
				if (idx_i == idx[j])
					break;
			}
			if (j < i)
				continue;
			for (int k = 0; k < elemSize; k++) {
				ms1ptr[i * elemSize + k] = m1ptr[idx_i * elemSize + k];
				ms2ptr[i * elemSize + k] = m2ptr[idx_i * elemSize + k];
			}

			if (!checkSubset(ms1, i + 1) || !checkSubset(ms2, i + 1)) {
				iters++;
				continue;
			}
			i++;
		}
		break;
	}

	return i == modelPoints && iters < maxAttempts;
}

static void computeReprojError(const Mat &_m1, const Mat &_m2, const Mat &model, Mat &_err)
{
	int count = _m1.rows * _m1.cols;
	const cv::Point2d *m1 = (const cv::Point2d *)_m1.data;
	const cv::Point2d *m2 = (const cv::Point2d *)_m2.data;
	const double *F = (const double *)model.data;
	float *err = (float *)_err.data;

	// calculate Point-Line Distances
	for (int i = 0; i < count; i++) {
		double a, b, c, d1, d2, s1, s2;

		a = F[0] * m1[i].x + F[1] * m1[i].y + F[2];
		b = F[3] * m1[i].x + F[4] * m1[i].y + F[5];
		c = F[6] * m1[i].x + F[7] * m1[i].y + F[8];

		s2 = a * a + b * b;
		d2 = m2[i].x * a + m2[i].y * b + c;

		a = F[0] * m2[i].x + F[3] * m2[i].y + F[6];
		b = F[1] * m2[i].x + F[4] * m2[i].y + F[7];
		c = F[2] * m2[i].x + F[5] * m2[i].y + F[8];

		s1 = a * a + b * b;
		d1 = m1[i].x * a + m1[i].y * b + c;

		err[i] = (float)std::max(d1 * d1 / s1, d2 * d2 / s2);
	}
}

static int findInliers(Mat &m1, Mat &m2, Mat &model, Mat &_mask, double threshold)
{
	int goodCount = 0;
	int count = m1.rows * m1.cols;
	Mat _err(1, count, CV_32FC1);
	const float* err = (float *)_err.data;
	uchar *mask = _mask.data;

	computeReprojError(m1, m2, model, _err);
	threshold *= threshold;
	for (int i = 0; i < count; i++)
		goodCount += mask[i] = err[i] <= threshold;

	return goodCount;
}

static int RANSACUpdateNumIters(double p, double ep, int model_points, int max_iters)
{
	p = MAX(p, 0.);
	p = MIN(p, 1.);
	ep = MAX(ep, 0.);
	ep = MIN(ep, 1.);

	// avoid inf's & nan's
	double num = MAX(1. - p, DBL_MIN);
	double denom = 1. - pow(1. - ep, model_points);
	if (denom < DBL_MIN)
		return 0;

	num = log(num);
	denom = log(denom);

	if (denom >= 0)
		return 1;
	else if (-num >= max_iters * -denom)
		return max_iters;
	else
		return cvRound(num/denom);
}

// reprojThreshold == distance
static bool runRANSAC(Mat &m1, Mat &m2, Mat &F, Mat &swapMask1, double reprojThreshold,
		double confidence, int maxIters = 2000)
{
	const int maxBasicSolutions = 1;
	const cv::Size modelSize(3, 3);
	int count = m1.rows * m1.cols;

	Mat model(modelSize.height * maxBasicSolutions, modelSize.width, CV_64FC1);
	Mat swapMask2(swapMask1.size(), swapMask1.type());

	Mat ms1(1, modelPoints, m1.type());
	Mat ms2(1, modelPoints, m2.type());

	int niters = maxIters;
	int maxGoodCount = 0;
	for (int iter = 0; iter < niters; iter++) {
		bool found = getSubset(m1, m2, ms1, ms2, 300);
		if (!found) {
			if (iter == 0)
				return false;
			break;
		}

		int result = qRun8Point(ms1, ms2, model);
		if (result <= 0)
			continue;

		int goodCount = findInliers(m1, m2, model, swapMask2, reprojThreshold);
		if (goodCount > maxGoodCount) {
			std::swap(swapMask2, swapMask1);
			F = model.clone();
			maxGoodCount = goodCount;
			double badCount = double(count - goodCount);
			niters = RANSACUpdateNumIters(confidence, badCount / count, modelPoints, niters);
		}
	}

	if (maxGoodCount <= 0)
		return false;

	return true;
}

static Mat findFundamentalMatRansac(Mat &points1, Mat &points2, vector<uchar> &inliers,
		double distance, double confidence)
{
	Mat F(3, 3, CV_64F, Scalar(0.));
	int count = cv::max(points1.cols, points1.rows);

	if (count < 15)
		return F;

	// change to the double precision system
	Mat m1 = points1.t();
	m1.convertTo(m1, CV_64FC2);
	Mat m2 = points2.t();
	m2.convertTo(m2, CV_64FC2);

	Mat mask(inliers);

	Mat swapMask(mask.size(), mask.type());
	runRANSAC(m1, m2, F, swapMask, distance, confidence);

	swapMask.copyTo(mask);

	return F;
}
#endif

// Identify good matches using RANSAC
// Return fundamental matrix
Mat RobustMatcher::ransacTest(const vector<cv::DMatch>& matches,
		const vector<cv::KeyPoint>& keypoints1,
		const vector<cv::KeyPoint>& keypoints2,
		vector<cv::DMatch>& outMatches)
{
	// Convert keypoints into Point2f	
	vector<Point2f> points1, points2;
	vector<cv::DMatch>::const_iterator it = matches.begin();
	for (; it != matches.end(); ++it) {
		// Get the position of left keypoints
		float x = keypoints1[it->queryIdx].pt.x;
		float y = keypoints1[it->queryIdx].pt.y;
		points1.push_back(Point2f(x, y));
		// Get the position of right keypoints
		x = keypoints2[it->trainIdx].pt.x;
		y = keypoints2[it->trainIdx].pt.y;
		points2.push_back(Point2f(x, y));
	}

	// Compute F matrix using RANSAC
	vector<uchar> inliers(points1.size(), 0);
#ifdef MORU_IMPL_FMAT
	Mat fundamental(3, 3, CV_64F, Scalar(0.));
	if (points1.size() < 8)
		return fundamental;

	fundamental = findFundamentalMatRansac(Mat(points1), Mat(points2),
		inliers, mDistance, mConfidence);
#else
	Mat fundamental = cv::findFundamentalMat(
			Mat(points1), Mat(points2), // matching points
			inliers,      // match status (inlier or outlier)
			FM_RANSAC, // RANSAC method
			mDistance,     // distance to epipolar line (for RANSAC)
			mConfidence);  // confidence probability (for RANSAC)
#endif
	// extract the surviving (inliers) matches
	vector<uchar>::const_iterator itIn = inliers.begin();
	vector<cv::DMatch>::const_iterator itM = matches.begin();
	// for all matches
	for (; itIn != inliers.end(); ++itIn, ++itM) {
		if (*itIn) // it is a valid match
			outMatches.push_back(*itM);
	}

	cout << "Number of matched points (after RANSAC cleaning): " << outMatches.size() << endl;

	return fundamental;
}

// Match feature points using symmetry test and RANSAC
// returns fundamental matrix
Mat RobustMatcher::match(Mat& image1, Mat& image2) // input images
{
	Mat fundamental(3, 3, CV_64F, Scalar(0.));
	vector<cv::KeyPoint> keypoints1, keypoints2;

	// 1a. Detection of the SURF features
	mDetector->detect(image1, keypoints1);
	mDetector->detect(image2, keypoints2);

	cout << "Number of SURF points (1): " << keypoints1.size() << endl;
	cout << "Number of SURF points (2): " << keypoints2.size() << endl;

	// 1b. Extraction of the SURF descriptors
	Mat descriptors1, descriptors2;
	mExtractor->compute(image1, keypoints1, descriptors1);
	mExtractor->compute(image2, keypoints2, descriptors2);

	// 2. Match the two image descriptors

	// Construction of the matcher 
	cv::BFMatcher matcher(cv::NORM_L2, false);

	// from image 1 to image 2
	// based on k nearest neighbours (with k=2)
	vector<vector<cv::DMatch> > matches1;
	matcher.knnMatch(descriptors1, descriptors2,
			matches1, // vector of matches (up to 2 per entry) 
			2);		  // return 2 nearest neighbours

	// from image 2 to image 1
	// based on k nearest neighbours (with k=2)
	vector<vector<cv::DMatch> > matches2;
	matcher.knnMatch(descriptors2, descriptors1,
			matches2, // vector of matches (up to 2 per entry) 
			2);		  // return 2 nearest neighbours

	cout << "Number of matched points 1->2: " << matches1.size() << endl;
	cout << "Number of matched points 2->1: " << matches2.size() << endl;

	// 3. Remove matches for which NN ratio is > than threshold

	// clean image 1 -> image 2 matches
	int removed = ratioTest(matches1);
	cout << "Number of matched points 1->2 (ratio test) : " << matches1.size() - removed << endl;
	// clean image 2 -> image 1 matches
	removed = ratioTest(matches2);
	cout << "Number of matched points 1->2 (ratio test) : " << matches2.size() - removed << endl;

	// 4. Remove non-symmetrical matches
	vector<cv::DMatch> symMatches;
	symmetryTest(matches1, matches2, symMatches);

	cout << "Number of matched points (symmetry test): " << symMatches.size() << endl;

	// draw the matches
	cv::Mat imageMatches;
	cv::drawMatches(image1, keypoints1,  // 1st image and its keypoints
					image2, keypoints2,  // 2nd image and its keypoints
					symMatches,			// the matches
					imageMatches,		// the image produced
					cv::Scalar(255,255,255)); // color of the lines
	cv::namedWindow("Matches");
	cv::imshow("Matches", imageMatches);

	if (symMatches.size() < 7)
		return fundamental;

	// 5. Validate matches using RANSAC
	vector<cv::DMatch> matches; // output matches and keypoints
	fundamental = ransacTest(symMatches, keypoints1, keypoints2, matches);

	if (matches.size() < 7)
		return fundamental;

	if (mRefineF) {
		// The F matrix will be recomputed with all accepted matches

		// Convert keypoints into Point2f for final F computation
		vector<Point2f> points1, points2;

		vector<cv::DMatch>::const_iterator it = matches.begin();
		for (; it != matches.end(); ++it) {
			// Get the position of left keypoints
			float x = keypoints1[it->queryIdx].pt.x;
			float y = keypoints1[it->queryIdx].pt.y;
			points1.push_back(Point2f(x, y));
			// Get the position of right keypoints
			x = keypoints2[it->trainIdx].pt.x;
			y = keypoints2[it->trainIdx].pt.y;
			points2.push_back(Point2f(x, y));
		}

#ifdef MORU_IMPL_FMAT
		if (points1.size() < 8)
			return fundamental;

		fundamental = qFindFundamentalMat8Point(Mat(points1), Mat(points2));
#else
		// Compute 7 or 8-point F from all accepted matches
		fundamental = cv::findFundamentalMat(
				Mat(points1), Mat(points2), // matching points
				FM_8POINT);
#endif
	}

	return fundamental;
}
