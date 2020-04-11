// Copyright (C) 2014 Kyuwon Kim <chammoru@gmail.com>

#include "QImageMatcher.h"

using namespace std;
using namespace cv;

Mat qGetFundamentalMat(Mat& image1, Mat& image2)
{
	RobustMatcher rmatcher;

	rmatcher.setConfidenceLevel(0.995);
	rmatcher.setMinDistanceToEpipolar(1.0);
	rmatcher.setRatio(0.65f);

	// Or you can use the SiftFeatureDetector class for the SIFT feature
#if CV_MAJOR_VERSION <= 2
	cv::Ptr<cv::FeatureDetector> pfd = new cv::SurfFeatureDetector(10); 
	rmatcher.setFeatureDetector(pfd);
#endif
	// Match the two images
	Mat fundamental = rmatcher.match(image1, image2);

	return fundamental;
}

void qComputeCorrespondEpilines(Point2f &point, int pointImageID, Mat &fmatrix, Vec3f &line)
{
	Mat F = fmatrix.clone();
	double *f = (double *)F.data;

	// transpose
	if (pointImageID == 2)
		F = F.t();

	double x = point.x;
	double y = point.y;
	double z = 1;

	double a = f[0] * x + f[1] * y + f[2] * z;
	double b = f[3] * x + f[4] * y + f[5] * z;
	double c = f[6] * x + f[7] * y + f[8] * z;
	double numerator = a * a + b * b;

	if (numerator) {
		numerator = sqrt(numerator);

		a /= numerator;
		b /= numerator;
		c /= numerator;
	}

	line[0] = static_cast<float>(a);
	line[1] = static_cast<float>(b);
	line[2] = static_cast<float>(c);
}

void qGetEpilines(vector<Point2f> &pts, bool isLeft,
				  Mat &fundamental, vector<Vec3f> &lines)
{
#ifdef MORU_IMPL_FMAT
	int id = isLeft ? 1 : 2;
	Vec3f line;
	qComputeCorrespondEpilines(pts[0], id, fundamental, line);
	lines.push_back(line);
#else
	if (isLeft)
		cv::computeCorrespondEpilines(Mat(pts), 1, fundamental, lines);
	else
		cv::computeCorrespondEpilines(Mat(pts), 2, fundamental, lines);
#endif
}

void qDecomposeEssentialMat(Mat &E, Mat &R1, Mat &R2, Mat &t)
{
	Mat D, U, Vt;
	SVD::compute(E, D, U, Vt);

	if (determinant(U) < 0)
		U *= -1.;
	if (determinant(Vt) < 0)
		Vt *= -1.;

	Mat W = (Mat_<double>(3, 3) << 0, 1, 0, -1, 0, 0, 0, 0, 1);

	R1 = U * W * Vt;
	R2 = U * W.t() * Vt;
	t = U.col(2) * 1.0;
}

void qTriangulatePoints(Mat &P1, Mat &P2, Mat &point1, Mat &point2,
						Mat &points4D)
{
	point1 = point1.reshape(1, static_cast<int>(point1.total())).t();
	point2 = point2.reshape(1, static_cast<int>(point2.total())).t();

	int numPoints = point1.cols;

	points4D.create(4, point1.cols, point1.type());

	Mat A(6, 4, CV_64F);
	Mat W(6, 4, CV_64F);
	Mat V(4, 4, CV_64F);

	Mat *pPoints[2];
	Mat *pMats[2];

	pPoints[0] = &point1;
	pPoints[1] = &point2;
	pMats[0] = &P1;
	pMats[1] = &P2;

	/* Solve system for each point */
	for (int i = 0; i < numPoints; i++) { /* For each point */
		/* Fill matrix for current point */
		for (int j = 0; j < 2; j++) { /* For each view */
			double x = pPoints[j]->at<double>(0, i);
			double y = pPoints[j]->at<double>(1, i);
			for (int k = 0; k < 4; k++) {
				A.at<double>(j * 3 + 0, k) = x * pMats[j]->at<double>(2, k) -     pMats[j]->at<double>(0, k);
				A.at<double>(j * 3 + 1, k) = y * pMats[j]->at<double>(2, k) -     pMats[j]->at<double>(1, k);
				A.at<double>(j * 3 + 2, k) = x * pMats[j]->at<double>(1, k) - y * pMats[j]->at<double>(0, k);
			}
		}

		/* Solve system for current point */
		Mat U;
		SVD::compute(A, W, U, V);

		/* Copy computed point */
		points4D.at<double>(0, i) = V.at<double>(3, 0); // X
		points4D.at<double>(1, i) = V.at<double>(3, 1); // Y
		points4D.at<double>(2, i) = V.at<double>(3, 2); // Z
		points4D.at<double>(3, i) = V.at<double>(3, 3); // W
	}
}

Mat qGetProjectionMat(Mat &R1, Mat &R2, Mat &t,
				   vector<Point2d> &points1, vector<Point2d> &points2)
{
	Mat P0 = Mat::eye(3, 4, R1.type());
	vector<Mat> P1(4);

	for (int i = 0; i < 4; i++)
		P1[i].create(3, 4, R1.type());

	P1[0](Range::all(), Range(0, 3)) = R1 * 1.0;
	P1[0].col(3) = t * 1.0;
	P1[1](Range::all(), Range(0, 3)) = R2 * 1.0;
	P1[1].col(3) = t * 1.0;
	P1[2](Range::all(), Range(0, 3)) = R1 * 1.0;
	P1[2].col(3) = -t * 1.0;
	P1[3](Range::all(), Range(0, 3)) = R2 * 1.0;
	P1[3].col(3) = -t * 1.0;

	// Do the chirality check.
	// Notice here a threshold dist is used to filter
	// out far away points (i.e. infinite points) since
	// there depth may vary between postive and negtive.
	double dist = 50.0;

	int maxGood = -1;
	int maxGoodIdx = -1;
	for (int i = 0; i < 4; i++) {
		Mat Q;
		Mat ptMat1 = Mat(points1);
		Mat ptMat2 = Mat(points2);
		qTriangulatePoints(P0, P1[i], ptMat1, ptMat2, Q);
		Mat mask = Q.row(2).mul(Q.row(3)) > 0;
		Q.row(0) /= Q.row(3);
		Q.row(1) /= Q.row(3);
		Q.row(2) /= Q.row(3);
		Q.row(3) /= Q.row(3);
		mask = (Q.row(2) < dist) & mask;
		Q = P1[i] * Q;
		mask = (Q.row(2) > 0) & mask;
		mask = (Q.row(2) < dist) & mask;

		mask = mask.t();
		int good = countNonZero(mask);
		if (good > maxGood) {
			maxGood = good;
			maxGoodIdx = i;
		}
	}

	return P1[maxGoodIdx];
}

int qRun8Point(const Mat &_m1, const Mat &_m2, Mat &F)
{
	double a[9 * 9] = {0, };
	Mat A(9, 9, CV_64F, a);

	Point2d m1c(0, 0), m2c(0, 0);
	double scale0 = 0, scale1 = 0;

	const Point2d *m1 = (const Point2d*)_m1.data;
	const Point2d *m2 = (const Point2d*)_m2.data;

	int i, count = _m1.cols *_m1.rows;

	// compute centers and average distances for each of the two point sets
	for (i = 0; i < count; i++) {
		double x = m1[i].x;
		double y = m1[i].y;
		m1c.x += x;
		m1c.y += y;
		x = m2[i].x;
		y = m2[i].y;
		m2c.x += x;
		m2c.y += y;
	}

	// calculate the normalizing transformations for each of the point sets:
	// after the transformation each set will have the mass center at the coordinate origin
	// and the average distance from the origin will be ~sqrt(2).
	m1c.x /= count;
	m1c.y /= count;
	m2c.x /= count;
	m2c.y /= count;

	// matrix norm
	for (i = 0; i < count; i++) {
		double x = m1[i].x - m1c.x;
		double y = m1[i].y - m1c.y;
		scale0 += sqrt(x * x + y * y);
		x = m2[i].x - m2c.x;
		y = m2[i].y - m2c.y;
		scale1 += sqrt(x * x + y * y);
	}

	scale0 /= count;
	scale1 /= count;

	if (scale0 < FLT_EPSILON || scale1 < FLT_EPSILON)
		return 0;

	// make the mean squared distance between the origin and the data points
	// be 2 pixels
	scale0 = sqrt(2.) / scale0;
	scale1 = sqrt(2.) / scale1;

	// form a linear system Ax=0: for each selected pair of points m1 & m2,
	// the row of A(=a) represents the coefficients of equation: (m2, 1)'*F*(m1, 1) = 0
	// to save computation time, we compute (At*A) instead of A and then solve (At*A)x=0.
	for (i = 0; i < count; i++) {
		double x0 = (m1[i].x - m1c.x) * scale0;
		double y0 = (m1[i].y - m1c.y) * scale0;
		double x1 = (m2[i].x - m2c.x) * scale1;
		double y1 = (m2[i].y - m2c.y) * scale1;
		// the form of A's transpose
		double r[9] = {x1*x0, x1*y0, x1, y1*x0, y1*y0, y1, x0, y0, 1};
		for(int j = 0; j < 9; j++) {
			for(int k = 0; k < 9; k++)
				a[j * 9 + k] += r[j] * r[k];
		}
	}

	// minimize sigma((x^t)*F*x')^2
	double w[9], v[9 * 9];
	Mat V(9, 9, CV_64F, v);
	Mat W(9, 1, CV_64F, w);
#if CV_MAJOR_VERSION <= 2
	cv::eigen(A, true, W, V); // Eigen Value Decomposition
#else
	cv::eigen(A, W, V);
#endif

	// should be greater than or equal 8
	for (i = 0; i < 8; i++) {
		if (fabs(w[i]) < DBL_EPSILON)
			return 0;
	}

	// The initial version of fundamental matrix
	Mat F0(3, 3, CV_64F, v + 9 * 8); // take the last column of v as a solution of Af = 0

	// make F0 singular (of rank 2) by decomposing it with SVD,
	// zeroing the last diagonal element of W and then composing the matrices back.
	// (enforce the rank-2 constraint)
	Mat W1(3, 1, CV_64F);
	V.create(3, 3, CV_64F);
	Mat U(3, 3, CV_64F);
	SVD::compute(F0, W1, U, V, SVD::MODIFY_A);

	W = Mat::zeros(3, 3, CV_64F);
	W.at<double>(0, 0) = W1.at<double>(0);
	W.at<double>(1, 1) = W1.at<double>(1);

	Mat TF(3, 3, CV_64F);
	// Performs generalized matrix multiplication.
	cv::gemm(U, W, 1., noArray(), 0., TF);
	cv::gemm(TF, V, 1., noArray(), 0., F0);

	// apply the transformation that is inverse
	// to what we used to normalize the point coordinates
	double tt0[] = {scale0, 0, -scale0*m1c.x, 0, scale0, -scale0*m1c.y, 0, 0, 1};
	double tt1[] = {scale1, 0, -scale1*m2c.x, 0, scale1, -scale1*m2c.y, 0, 0, 1};
	Mat T0(F0.size(), F0.type(), tt0);
	Mat T1(F0.size(), F0.type(), tt1);

	// F <- T1'*F0*T0
	cv::gemm(T1, F0, 1., noArray(), 0., TF, GEMM_1_T);
	cv::gemm(TF, T0, 1., noArray(), 0., F);

	// make fmatrix(3,3) = 1
	if (fabs(F.at<double>(2, 2)) > FLT_EPSILON)
		F = F * 1. / F.at<double>(2, 2);

	return 1;
}

Mat qFindFundamentalMat8Point(Mat &points1, Mat &points2)
{
	// change to the double precision system
	Mat m1 = points1.t();
	m1.convertTo(m1, CV_64FC2);
	Mat m2 = points2.t();
	m2.convertTo(m2, CV_64FC2);

	Mat F(3, 3, CV_64F, Scalar(0.));
	qRun8Point(m1, m2, F);

	return F;
}
