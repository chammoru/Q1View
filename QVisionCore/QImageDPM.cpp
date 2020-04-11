// Copyright (C) 2014 Kyuwon Kim <chammoru@gmail.com>

#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>

#include "QImageDPM.h"
#include "QImageDpmParse.h"

#include "SThread.h"
#include "QPort.h"

// When we have confidence that a person is at some position in a image
// and are not sure the whole position, we can utilize the fact by disabling
// the next macro. This could be slower because it checks all levels

#ifndef RELAX_VAL
#define RELAX_VAL   3
#endif

#define TIME_COUNT  3

#ifndef Q1_NUM_THREADS
#define Q1_NUM_THREADS 8
#endif

using namespace std;
using namespace cv;

static string extractModelName(const string& filename)
{
    size_t startPos = filename.rfind('/');
    if ( startPos == string::npos )
        startPos = filename.rfind('\\');

    if ( startPos == string::npos )
        startPos = 0;
    else
        startPos++;

    const int extentionSize = 4; //.xml

    int substrLength = (int)(filename.size() - startPos - extentionSize);

    return filename.substr(startPos, substrLength);
}

// Getting feature map for the selected subimage
static LsvmFeatureMap *getFeatureMap(const Mat &image, const int k)
{
	LsvmFeatureMap *featureMap;
    int sizeX, sizeY;
    int p, px, stringSize;
    int height, width, numChannels;
    int i, j, kk, c, ii, jj, d;
    float  * datadx, * datady;

    int   ch;
    float magnitude, x, y, tx, ty;

    height = image.rows;
    width  = image.cols;

	Mat dx(cvSize(width, height), CV_32FC3);
	Mat dy(cvSize(width, height), CV_32FC3);
    int *nearest;
    float *w, a_x, b_x;

    float kernel[3] = {-1.f, 0.f, 1.f};
    Mat kernel_dx(1, 3, CV_32F, kernel);
    Mat kernel_dy(3, 1, CV_32F, kernel);

    float * r;
    int   * alfa;

    float boundary_x[NUM_SECTOR + 1];
    float boundary_y[NUM_SECTOR + 1];
    float max, dotProd;
    int   maxi;

    numChannels = image.channels();

    sizeX = width  / k;
    sizeY = height / k;
    px    = 3 * NUM_SECTOR;
    p     = px;
    stringSize = sizeX * p;
	featureMap = new LsvmFeatureMap(sizeX, sizeY, p);

    filter2D(image, dx, dx.depth(), kernel_dx, Point(-1, 0));
    filter2D(image, dy, dy.depth(), kernel_dy, Point(0, -1));

    float arg_vector;
    for (i = 0; i <= NUM_SECTOR; i++) {
        arg_vector = ( (float) i ) * ( (float)(CV_PI) / (float)(NUM_SECTOR));
        boundary_x[i] = cosf(arg_vector);
        boundary_y[i] = sinf(arg_vector);
    }

    r    = (float *)malloc( sizeof(float) * (width * height));
    alfa = (int   *)malloc( sizeof(int  ) * (width * height * 2));

    for (j = 1; j < height - 1; j++) {
        datadx = (float *)(dx.data + dx.step * j);
        datady = (float *)(dy.data + dy.step * j);
        for (i = 1; i < width - 1; i++) {
            c = 0;
            x = datadx[i * numChannels + c];
            y = datady[i * numChannels + c];

            r[j * width + i] = sqrtf(x * x + y * y);
            for (ch = 1; ch < numChannels; ch++) {
                tx = datadx[i * numChannels + ch];
                ty = datady[i * numChannels + ch];
                magnitude = sqrtf(tx * tx + ty * ty);
                if (magnitude > r[j * width + i]) {
                    r[j * width + i] = magnitude;
                    c = ch;
                    x = tx;
                    y = ty;
                }
            }

            max  = boundary_x[0] * x + boundary_y[0] * y;
            maxi = 0;
            for (kk = 0; kk < NUM_SECTOR; kk++) {
                dotProd = boundary_x[kk] * x + boundary_y[kk] * y;
                if (dotProd > max) {
                    max  = dotProd;
                    maxi = kk;
                } else {
                    if (-dotProd > max) {
                        max  = -dotProd;
                        maxi = kk + NUM_SECTOR;
                    }
                }
            }
            alfa[j * width * 2 + i * 2    ] = maxi % NUM_SECTOR;
            alfa[j * width * 2 + i * 2 + 1] = maxi;
        }
    }

    nearest = (int  *)malloc(sizeof(int  ) *  k);
    w       = (float*)malloc(sizeof(float) * (k * 2));

    for (i = 0; i < k / 2; i++)
        nearest[i] = -1;

    for (i = k / 2; i < k; i++)
        nearest[i] = 1;

    for (j = 0; j < k / 2; j++) {
        b_x = k / 2 + j + 0.5f;
        a_x = k / 2 - j - 0.5f;
        w[j * 2    ] = 1.0f / a_x * ((a_x * b_x) / ( a_x + b_x));
        w[j * 2 + 1] = 1.0f / b_x * ((a_x * b_x) / ( a_x + b_x));
    }

    for (j = k / 2; j < k; j++) {
        a_x = j - k / 2 + 0.5f;
        b_x =-j + k / 2 - 0.5f + k;
        w[j * 2    ] = 1.0f / a_x * ((a_x * b_x) / ( a_x + b_x));
        w[j * 2 + 1] = 1.0f / b_x * ((a_x * b_x) / ( a_x + b_x));
    }

	float *featureMat = featureMap->mFeatureMat;
	int numFeatures = featureMap->mNumFeatures;

    for (i = 0; i < sizeY; i++) {
      for (j = 0; j < sizeX; j++) {
        for (ii = 0; ii < k; ii++) {
          for (jj = 0; jj < k; jj++) {
            if ((i * k + ii > 0) &&
                (i * k + ii < height - 1) &&
                (j * k + jj > 0) &&
                (j * k + jj < width  - 1)) {
              d = (k * i + ii) * width + (j * k + jj);
              featureMat[i * stringSize + j * numFeatures + alfa[d * 2    ]]              += r[d] * w[ii * 2] * w[jj * 2];
              featureMat[i * stringSize + j * numFeatures + alfa[d * 2 + 1] + NUM_SECTOR] += r[d] * w[ii * 2] * w[jj * 2];

              if ((i + nearest[ii] >= 0) &&
                  (i + nearest[ii] <= sizeY - 1)) {
                featureMat[(i + nearest[ii]) * stringSize + j * numFeatures + alfa[d * 2    ]             ] += r[d] * w[ii * 2 + 1] * w[jj * 2 ];
                featureMat[(i + nearest[ii]) * stringSize + j * numFeatures + alfa[d * 2 + 1] + NUM_SECTOR] += r[d] * w[ii * 2 + 1] * w[jj * 2 ];
              }

              if ((j + nearest[jj] >= 0) &&
                  (j + nearest[jj] <= sizeX - 1)) {
                featureMat[i * stringSize + (j + nearest[jj]) * numFeatures + alfa[d * 2    ]             ] += r[d] * w[ii * 2] * w[jj * 2 + 1];
                featureMat[i * stringSize + (j + nearest[jj]) * numFeatures + alfa[d * 2 + 1] + NUM_SECTOR] += r[d] * w[ii * 2] * w[jj * 2 + 1];
              }

              if ((i + nearest[ii] >= 0) &&
                  (i + nearest[ii] <= sizeY - 1) &&
                  (j + nearest[jj] >= 0) &&
                  (j + nearest[jj] <= sizeX - 1)) {
                featureMat[(i + nearest[ii]) * stringSize + (j + nearest[jj]) * numFeatures + alfa[d * 2    ]             ] += r[d] * w[ii * 2 + 1] * w[jj * 2 + 1];
                featureMat[(i + nearest[ii]) * stringSize + (j + nearest[jj]) * numFeatures + alfa[d * 2 + 1] + NUM_SECTOR] += r[d] * w[ii * 2 + 1] * w[jj * 2 + 1];
              }
            }
          }
        }
      }
    }

    free(w);
    free(nearest);

    free(r);
    free(alfa);

    return featureMap;
}

// Feature map Normalization and Truncation
// alfa              - truncation threshold
// featureMap        - truncated and normalized feature map
static int normalizeAndTruncate(LsvmFeatureMap *featureMap, const float alfa)
{
    int i,j, ii;
    int sizeX, sizeY, p, pos, pp, xp, pos1, pos2;
    float * newFeatureMat;
    float   valOfNorm;

    sizeX     = featureMap->mSizeX;
    sizeY     = featureMap->mSizeY;
	vector<float> partOfNorm(sizeX * sizeY); // norm of C(i, j)

    p  = NUM_SECTOR;
    xp = NUM_SECTOR * 3;
    pp = NUM_SECTOR * 12;

    for (i = 0; i < sizeX * sizeY; i++) {
        valOfNorm = 0.0f;
        pos = i * featureMap->mNumFeatures;
        for (j = 0; j < p; j++)
            valOfNorm += featureMap->mFeatureMat[pos + j] * featureMap->mFeatureMat[pos + j];
        partOfNorm[i] = valOfNorm;
    }

    sizeX -= 2;
    sizeY -= 2;

	newFeatureMat = new float[sizeX * sizeY * pp];
    //normalization
    for (i = 1; i <= sizeY; i++) {
        for (j = 1; j <= sizeX; j++) {
            valOfNorm = sqrtf(
                partOfNorm[(i    )*(sizeX + 2) + (j    )] +
                partOfNorm[(i    )*(sizeX + 2) + (j + 1)] +
                partOfNorm[(i + 1)*(sizeX + 2) + (j    )] +
                partOfNorm[(i + 1)*(sizeX + 2) + (j + 1)]) + FLT_EPSILON;
            pos1 = (i  ) * (sizeX + 2) * xp + (j  ) * xp;
            pos2 = (i-1) * (sizeX    ) * pp + (j-1) * pp;
            for (ii = 0; ii < p; ii++)
                newFeatureMat[pos2 + ii        ] = featureMap->mFeatureMat[pos1 + ii    ] / valOfNorm;
            for (ii = 0; ii < 2 * p; ii++)
                newFeatureMat[pos2 + ii + p * 4] = featureMap->mFeatureMat[pos1 + ii + p] / valOfNorm;

            valOfNorm = sqrtf(
                partOfNorm[(i    )*(sizeX + 2) + (j    )] +
                partOfNorm[(i    )*(sizeX + 2) + (j + 1)] +
                partOfNorm[(i - 1)*(sizeX + 2) + (j    )] +
                partOfNorm[(i - 1)*(sizeX + 2) + (j + 1)]) + FLT_EPSILON;
            for (ii = 0; ii < p; ii++)
                newFeatureMat[pos2 + ii + p    ] = featureMap->mFeatureMat[pos1 + ii    ] / valOfNorm;
            for (ii = 0; ii < 2 * p; ii++)
                newFeatureMat[pos2 + ii + p * 6] = featureMap->mFeatureMat[pos1 + ii + p] / valOfNorm;

			valOfNorm = sqrtf(
                partOfNorm[(i    )*(sizeX + 2) + (j    )] +
                partOfNorm[(i    )*(sizeX + 2) + (j - 1)] +
                partOfNorm[(i + 1)*(sizeX + 2) + (j    )] +
                partOfNorm[(i + 1)*(sizeX + 2) + (j - 1)]) + FLT_EPSILON;
            for (ii = 0; ii < p; ii++)
                newFeatureMat[pos2 + ii + p * 2] = featureMap->mFeatureMat[pos1 + ii    ] / valOfNorm;
            for (ii = 0; ii < 2 * p; ii++)
                newFeatureMat[pos2 + ii + p * 8] = featureMap->mFeatureMat[pos1 + ii + p] / valOfNorm;

			valOfNorm = sqrtf(
                partOfNorm[(i    )*(sizeX + 2) + (j    )] +
                partOfNorm[(i    )*(sizeX + 2) + (j - 1)] +
                partOfNorm[(i - 1)*(sizeX + 2) + (j    )] +
                partOfNorm[(i - 1)*(sizeX + 2) + (j - 1)]) + FLT_EPSILON;
            for (ii = 0; ii < p; ii++)
                newFeatureMat[pos2 + ii + p * 3 ] = featureMap->mFeatureMat[pos1 + ii    ] / valOfNorm;
            for (ii = 0; ii < 2 * p; ii++)
                newFeatureMat[pos2 + ii + p * 10] = featureMap->mFeatureMat[pos1 + ii + p] / valOfNorm;
        }
    }

	//truncation
    for (i = 0; i < sizeX * sizeY * pp; i++) {
        if (newFeatureMat[i] > alfa)
			newFeatureMat [i] = alfa;
	}

	//swap data
    featureMap->mNumFeatures  = pp;
    featureMap->mSizeX = sizeX;
    featureMap->mSizeY = sizeY;

    delete [] featureMap->mFeatureMat;

    featureMap->mFeatureMat = newFeatureMat;

    return LATENT_SVM_OK;
}

// Feature map reduction
static int PCAFeatureMap(LsvmFeatureMap *featureMap)
{
    int i,j, ii, jj, k;
    int sizeX, sizeY, p,  pp, xp, yp, pos1, pos2;
    float *newFeatureMat;
    float val;
    float nx, ny;

    sizeX = featureMap->mSizeX;
    sizeY = featureMap->mSizeY;
    p     = featureMap->mNumFeatures;
    pp    = NUM_SECTOR * 3 + 4;
    yp    = 4;
    xp    = NUM_SECTOR;

    nx    = 1.0f / sqrtf((float)(xp * 2));
    ny    = 1.0f / sqrtf((float)(yp    ));

	newFeatureMat = new float[sizeX * sizeY * pp];

    for (i = 0; i < sizeY; i++) {
        for (j = 0; j < sizeX; j++) {
            pos1 = ((i)*sizeX + j)*p;
            pos2 = ((i)*sizeX + j)*pp;
            k = 0;
            for (jj = 0; jj < xp * 2; jj++) {
                val = 0;
                for (ii = 0; ii < yp; ii++)
                    val += featureMap->mFeatureMat[pos1 + yp * xp + ii * xp * 2 + jj];
                newFeatureMat[pos2 + k] = val * ny;
                k++;
            }

            for (jj = 0; jj < xp; jj++) {
                val = 0;
                for (ii = 0; ii < yp; ii++)
                    val += featureMap->mFeatureMat[pos1 + ii * xp + jj];
                newFeatureMat[pos2 + k] = val * ny;
                k++;
            }

            for (ii = 0; ii < yp; ii++) {
                val = 0;
                for (jj = 0; jj < 2 * xp; jj++)
                    val += featureMap->mFeatureMat[pos1 + yp * xp + ii * xp * 2 + jj];
                newFeatureMat[pos2 + k] = val * nx;
                k++;
            }
        }
    }

    //swap data
    featureMap->mNumFeatures = pp;
    delete [] featureMap->mFeatureMat;
    featureMap->mFeatureMat = newFeatureMat;

    return LATENT_SVM_OK;
}

class FeaturePyramidThread : public SThread
{
public:
	FeaturePyramidThread(long *stepPtr, long numStep, float step,
		Mat &image, int sideLength, int startIndex, LsvmFeaturePyramid *pyramid)
	: mStepPtr(stepPtr)
	, mNumStep(numStep)
	, mStep(step)
	, mImage(image)
	, mSideLength(sideLength)
	, mStartIndex(startIndex)
	, mPyramid(pyramid)
	{}

	inline long getNextStep() const { return qcmn_atomic_inc(mStepPtr); }

	virtual bool threadLoop()
	{
		long level = getNextStep();

		if (level >= mNumStep)
			return false;

		int w = mImage.cols;
		int h = mImage.rows;

		int tW, tH;
		Mat scaleTmp;
		float scale = 1.0f / powf(mStep, (float)level);
		tW = (int)(w * scale + 0.5);
		tH = (int)(h * scale + 0.5);
		resize(mImage, scaleTmp, Size(tW, tH), 0., 0., CV_INTER_AREA);
		LsvmFeatureMap *featureMap = getFeatureMap(scaleTmp, mSideLength);
		normalizeAndTruncate(featureMap, VAL_OF_TRUNCATE);
		PCAFeatureMap(featureMap);
		mPyramid->mFeatureMaps[mStartIndex + level] = featureMap;

		return true;
	}

private:
	QLsvmDetector *mDetector;
	long *mStepPtr;
	int mNumStep;
	float mStep;
	Mat &mImage;
	int mSideLength;
	int mStartIndex;
	LsvmFeaturePyramid *mPyramid;
};

static int getPathOfFeaturePyramid(Mat &image,
                            float step, int numStep, int startIndex,
                            int sideLength, LsvmFeaturePyramid *pyramid)
{
#if Q1_NUM_THREADS > 1
	long level = -1;

	cv::Ptr<FeaturePyramidThread> fpt[Q1_NUM_THREADS];
	for (int i = 0; i < Q1_NUM_THREADS; i++) {
		fpt[i] = new FeaturePyramidThread(&level, numStep, step, image, sideLength, startIndex, pyramid);
		fpt[i]->run();
	}
	for (int i = 0; i < Q1_NUM_THREADS; i++)
		fpt[i]->join();
#else
	int w = image.cols;
	int h = image.rows;

	for (int i = 0; i < numStep; i++) {
		Mat scaleTmp;
		float scale = 1.0f / powf(step, (float)i);
		int tW = (int)(w * scale + 0.5);
		int tH = (int)(h * scale + 0.5);
		resize(image, scaleTmp, Size(tW, tH), 0., 0., CV_INTER_AREA);
		LsvmFeatureMap *featureMap = getFeatureMap(scaleTmp, sideLength);
		normalizeAndTruncate(featureMap, VAL_OF_TRUNCATE);
		PCAFeatureMap(featureMap);
		pyramid->mFeatureMaps[startIndex + i] = featureMap;
	}
#endif

    return LATENT_SVM_OK;
}

// Getting feature pyramid
int QLsvmDetector::getFeaturePyramid(const Mat &image)
{
    Mat imgResize;
    int numStep;
    int maxNumCells;
    int w, h;

    if (image.depth() == CV_32FC3)
        imgResize = image;
    else
		image.convertTo(imgResize, CV_32FC3);

    w = imgResize.cols;
    h = imgResize.rows;

    mStep = powf(2.0f, 1.0f / (float)LAMBDA);
    maxNumCells = min(w / SIDE_LENGTH, h / SIDE_LENGTH);
    numStep = (int)(logf(maxNumCells / 5.0f) / logf(mStep)) + 1;

	mPyramid = new LsvmFeaturePyramid(numStep + LAMBDA);

    getPathOfFeaturePyramid(imgResize, mStep, LAMBDA,  0,      SIDE_LENGTH / 2, mPyramid);
    getPathOfFeaturePyramid(imgResize, mStep, numStep, LAMBDA, SIDE_LENGTH,     mPyramid);

    return LATENT_SVM_OK;
}

// Function for convolution computation
// OUTPUT
// f                 - the convolution
static int convolution(const LsvmFilter *filter, const LsvmFeatureMap *featureMap, float *f)
{
    int ftrMapY, ftrMapX, filterY, filterX, p, diffY, diffX;
    int i1, i2, j1, j2, k;
    float tmp_f1, tmp_f2, tmp_f3, tmp_f4;
    float *pMat = NULL;
    float *pH = NULL;

    ftrMapY = featureMap->mSizeY;
    ftrMapX = featureMap->mSizeX;
    filterY = filter->sizeY;
    filterX = filter->sizeX;
    p = featureMap->mNumFeatures;

    diffY = ftrMapY - filterY + 1;
    diffX = ftrMapX - filterX + 1;

	for (i1 = diffY - 1; i1 >= 0; i1--) {
		for (j1 = diffX - 1; j1 >= 0; j1--) {
            tmp_f1 = 0.0f;
            tmp_f2 = 0.0f;
            tmp_f3 = 0.0f;
            tmp_f4 = 0.0f;
            for (i2 = 0; i2 < filterY; i2++) {
                for (j2 = 0; j2 < filterX; j2++) {
                    pMat = featureMap->mFeatureMat + (i1 + i2) * ftrMapX * p + (j1 + j2) * p;//sm2
                    pH = filter->H + (i2 * filterX + j2) * p;//sm2
                    for (k = 0; k < p/4; k++) {
                        tmp_f1 += pMat[4*k]   * pH[4*k];//sm2
                        tmp_f2 += pMat[4*k+1] * pH[4*k+1];
                        tmp_f3 += pMat[4*k+2] * pH[4*k+2];
                        tmp_f4 += pMat[4*k+3] * pH[4*k+3];
                    }

                    if (p % 4 == 1)
                        tmp_f1 += pH[p-1] * pMat[p-1];
                    else if (p % 4 == 2)
                        tmp_f1 += pH[p-2] * pMat[p-2] + pH[p-1] * pMat[p-1];
                    else if (p % 4 == 3)
                        tmp_f1 += pH[p-3] * pMat[p-3] + pH[p-2] * pMat[p-2] + pH[p-1] * pMat[p-1];
                }
            }
            f[i1 * diffX + j1] = tmp_f1 + tmp_f2 + tmp_f3 + tmp_f4;//sm1
        }
    }
    return LATENT_SVM_OK;
}

// Function for convolution computation
// OUTPUT
// f                 - the convolution
static int convolution2(const LsvmFilter *filter, Mat &done,
					   const LsvmFeatureMap *featureMap, float *f,
					   const Point &ftrCoord, QSafeRect *limit)
{
    int ftrMapY, ftrMapX, filterY, filterX, p, diffY, diffX;
    int i1, i2, j1, j2, k;
    float tmp_f1, tmp_f2, tmp_f3, tmp_f4;
    float *pMat = NULL;
    float *pH = NULL;

    ftrMapY = featureMap->mSizeY;
    ftrMapX = featureMap->mSizeX;
    filterY = filter->sizeY;
    filterX = filter->sizeX;
    p = featureMap->mNumFeatures;

    diffY = ftrMapY - filterY + 1;
    diffX = ftrMapX - filterX + 1;

	int xFrom = max(ftrCoord.x - RELAX_VAL + 1, 0);
	int xTo = min(ftrCoord.x + RELAX_VAL, diffX);
	int yFrom = max(ftrCoord.y - RELAX_VAL + 1, 0);
	int yTo = min(ftrCoord.y + RELAX_VAL, diffY);
	if (yFrom >= yTo || xFrom >= xTo)
		return LATENT_SVM_OK;

	for (i1 = yTo - 1; i1 >= yFrom; i1--) {
		for (j1 = xTo - 1; j1 >= xFrom; j1--) {
			if (done.at<uchar>(i1, j1))
				continue;
			done.at<uchar>(i1, j1)++;

			tmp_f1 = 0.0f;
            tmp_f2 = 0.0f;
            tmp_f3 = 0.0f;
            tmp_f4 = 0.0f;
            for (i2 = 0; i2 < filterY; i2++) {
                for (j2 = 0; j2 < filterX; j2++) {
                    pMat = featureMap->mFeatureMat + (i1 + i2) * ftrMapX * p + (j1 + j2) * p;//sm2
                    pH = filter->H + (i2 * filterX + j2) * p;//sm2
                    for (k = 0; k < p/4; k++) {
                        tmp_f1 += pMat[4*k]   * pH[4*k];//sm2
                        tmp_f2 += pMat[4*k+1] * pH[4*k+1];
                        tmp_f3 += pMat[4*k+2] * pH[4*k+2];
                        tmp_f4 += pMat[4*k+3] * pH[4*k+3];
                    }

                    if (p % 4 == 1)
                        tmp_f1 += pH[p-1] * pMat[p-1];
                    else if (p % 4 == 2)
                        tmp_f1 += pH[p-2] * pMat[p-2] + pH[p-1] * pMat[p-1];
                    else if (p % 4 == 3)
                        tmp_f1 += pH[p-3] * pMat[p-3] + pH[p-2] * pMat[p-2] + pH[p-1] * pMat[p-1];
                }
            }
            f[i1 * diffX + j1] = tmp_f1 + tmp_f2 + tmp_f3 + tmp_f4;//sm1
        }
    }

	limit->maximize(xFrom, yFrom, xTo, yTo);

    return LATENT_SVM_OK;
}

// Function for convolution computation
// OUTPUT
// f                 - the convolution
static int convolutionInv(const LsvmFilter *filter, const LsvmFeatureMap *featureMap, float *f)
{
    int ftrMapY, ftrMapX, filterY, filterX, p, diffY, diffX;
    int i1, i2, j1, j2, k;
    float tmp_f1, tmp_f2, tmp_f3, tmp_f4;
    float *pMat = NULL;
    float *pH = NULL;

    ftrMapY = featureMap->mSizeY;
    ftrMapX = featureMap->mSizeX;
    filterY = filter->sizeY;
    filterX = filter->sizeX;
    p = featureMap->mNumFeatures;

    diffY = ftrMapY - filterY + 1;
    diffX = ftrMapX - filterX + 1;

	for (i1 = diffY - 1; i1 >= 0; i1--) {
		for (j1 = diffX - 1; j1 >= 0; j1--) {
            tmp_f1 = 0.0f;
            tmp_f2 = 0.0f;
            tmp_f3 = 0.0f;
            tmp_f4 = 0.0f;
            for (i2 = 0; i2 < filterY; i2++) {
                for (j2 = 0; j2 < filterX; j2++) {
                    pMat = featureMap->mFeatureMat + (i1 + i2) * ftrMapX * p + (j1 + j2) * p;//sm2
                    pH = filter->H + (i2 * filterX + j2) * p;//sm2
                    for (k = 0; k < p/4; k++) {
                        tmp_f1 += pMat[4*k]   * pH[4*k];//sm2
                        tmp_f2 += pMat[4*k+1] * pH[4*k+1];
                        tmp_f3 += pMat[4*k+2] * pH[4*k+2];
                        tmp_f4 += pMat[4*k+3] * pH[4*k+3];
                    }

                    if (p % 4 == 1)
                        tmp_f1 += pH[p-1] * pMat[p-1];
                    else if (p % 4 == 2)
                        tmp_f1 += pH[p-2] * pMat[p-2] + pH[p-1] * pMat[p-1];
                    else if (p % 4 == 3)
                        tmp_f1 += pH[p-3] * pMat[p-3] + pH[p-2] * pMat[p-2] + pH[p-1] * pMat[p-1];
                }
            }
            f[i1 * diffX + j1] = -(tmp_f1 + tmp_f2 + tmp_f3 + tmp_f4);
        }
    }
    return LATENT_SVM_OK;
}

static int convolutionInv2(const LsvmFilter *filter, Mat &done,
					   const LsvmFeatureMap *featureMap, float *f,
					   const Point &ftrCoord, QSafeRect *limit)
{
    int ftrMapY, ftrMapX, filterY, filterX, p, diffY, diffX;
    int i1, i2, j1, j2, k;
    float tmp_f1, tmp_f2, tmp_f3, tmp_f4;
    float *pMat = NULL;
    float *pH = NULL;

    ftrMapY = featureMap->mSizeY;
    ftrMapX = featureMap->mSizeX;
    filterY = filter->sizeY;
    filterX = filter->sizeX;
    p = featureMap->mNumFeatures;

    diffY = ftrMapY - filterY + 1;
    diffX = ftrMapX - filterX + 1;

	int xFrom = max(ftrCoord.x - RELAX_VAL * 2 + 1, 0);
	int xTo = min(ftrCoord.x + RELAX_VAL * 2, diffX);
	int yFrom = max(ftrCoord.y - RELAX_VAL * 2 + 1, 0);
	int yTo = min(ftrCoord.y + RELAX_VAL * 2, diffY);
	yFrom = min(yFrom, yTo);

	if (xFrom >= xTo || yFrom >= yTo)
		return LATENT_SVM_OK; // This case happens for part filters

	for (i1 = yTo - 1; i1 >= yFrom; i1--) {
		for (j1 = xTo - 1; j1 >= xFrom; j1--) {
			if (done.at<uchar>(i1, j1))
				continue;
			done.at<uchar>(i1, j1)++;
            tmp_f1 = 0.0f;
            tmp_f2 = 0.0f;
            tmp_f3 = 0.0f;
            tmp_f4 = 0.0f;
            for (i2 = 0; i2 < filterY; i2++) {
                for (j2 = 0; j2 < filterX; j2++) {
                    pMat = featureMap->mFeatureMat + (i1 + i2) * ftrMapX * p + (j1 + j2) * p;//sm2
                    pH = filter->H + (i2 * filterX + j2) * p;//sm2
                    for (k = 0; k < p/4; k++) {
                        tmp_f1 += pMat[4*k]   * pH[4*k];//sm2
                        tmp_f2 += pMat[4*k+1] * pH[4*k+1];
                        tmp_f3 += pMat[4*k+2] * pH[4*k+2];
                        tmp_f4 += pMat[4*k+3] * pH[4*k+3];
                    }

                    if (p % 4 == 1)
                        tmp_f1 += pH[p-1] * pMat[p-1];
                    else if (p % 4 == 2)
                        tmp_f1 += pH[p-2] * pMat[p-2] + pH[p-1] * pMat[p-1];
                    else if (p % 4 == 3)
                        tmp_f1 += pH[p-3] * pMat[p-3] + pH[p-2] * pMat[p-2] + pH[p-1] * pMat[p-1];
                }
            }
            f[i1 * diffX + j1] = -(tmp_f1 + tmp_f2 + tmp_f3 + tmp_f4);
        }
    }

	limit->maximize(xFrom, yFrom, xTo, yTo);

    return LATENT_SVM_OK;
}

// Computation the point of intersection functions
// (parabolas on the variable y)
//      a(y - q1) + b(q1 - y)(q1 - y) + f[q1]
//      a(y - q2) + b(q2 - y)(q2 - y) + f[q2]
//
// INPUT
//   f                - function on the regular grid
//   a                - coefficient of the function
//   b                - coefficient of the function
//   q1               - parameter of the function
//   q2               - parameter of the function
// OUTPUT
//   point            - point of intersection
static int GetPointOfIntersection(const float *f,
                           const float a, const float b,
                           int q1, int q2, float *point)
{
    if (q1 == q2)
        return DISTANCE_TRANSFORM_EQUAL_POINTS;

	*point = ((f[q2] - a * q2 + b *q2 * q2) - (f[q1] - a * q1 + b * q1 * q1)) / (2 * b * (q2 - q1));

    return DISTANCE_TRANSFORM_OK;
}

// Decision of one dimensional problem generalized distance transform
// on the regular grid at all points
//      min (a(y' - y) + b(y' - y)(y' - y) + f(y')) (on y')
//
// INPUT
// f                 - function on the regular grid
// dim               - grid dimension
// a                 - coefficient of optimizable function
// b                 - coefficient of optimizable function
// OUTPUT
// distanceTransform - values of generalized distance transform
// points            - arguments that corresponds to the optimal value of function
static int DistanceTransformOneDimensionalProblem(const float *f, const int dim,
                                           const float a, const float b,
                                           float *distanceTransform,
                                           int *points,
										   int from, int to)
{
    int i, k;
    int tmp;
    int diff;
    float pointIntersection;
    k = from;

    // Allocation memory (must be free in this function)
	vector<int> v(dim);
	vector<float> z(dim + 1);

    v[from] = from;
    z[from] = (float)F_MIN; // left border of envelope
    z[from + 1] = (float)F_MAX; // right border of envelope

    for (i = from + 1; i < to; i++) {
        tmp = GetPointOfIntersection(f, a, b, v[k], i, &pointIntersection);
        if (tmp != DISTANCE_TRANSFORM_OK)
            return DISTANCE_TRANSFORM_GET_INTERSECTION_ERROR;

        if (pointIntersection <= z[k]) {
            // Envelope doesn't contain current parabola
            do {
                k--;
                tmp = GetPointOfIntersection(f, a, b, v[k], i, &pointIntersection);
                if (tmp != DISTANCE_TRANSFORM_OK)
                    return DISTANCE_TRANSFORM_GET_INTERSECTION_ERROR;
            } while (pointIntersection <= z[k]);
        }

        // Addition parabola to the envelope
        k++;
        v[k] = i;
        z[k] = pointIntersection;
        z[k + 1] = (float)F_MAX;
    }

    // Computation values of generalized distance transform at all grid points
    k = from;
    for (i = from; i < to; i++) {
        while (z[k + 1] < i)
            k++;

        points[i] = v[k];
        diff = i - v[k];
        distanceTransform[i] = a * diff + b * diff * diff + f[v[k]];
    }

    return DISTANCE_TRANSFORM_OK;
}

// Decision of two dimensional problem generalized distance transform
// on the regular grid at all points
//      min{d2(y' - y) + d4(y' - y)(y' - y) +
//          min(d1(x' - x) + d3(x' - x)(x' - x) + f(x',y'))} (on x', y')
//
// INPUT
// f                 - function on the regular grid
// diffY             - number of rows
// diffX             - number of columns
// coeff             - coefficients of optimizable function
//                     coeff[0] = d1, coeff[1] = d2,
//                     coeff[2] = d3, coeff[3] = d4
// OUTPUT
// distanceTransform - values of generalized distance transform
// pointsX           - arguments x' that correspond to the optimal value
// pointsY           - arguments y' that correspond to the optimal value
// RESULT
// Error status
static int DistanceTransformTwoDimensionalProblem(const float *f,
											const int diffY, const int diffX,
											const float coeff[4],
											LsvmFilterDisposition *desposition,
											QRect *limit)
{
    int i, j;
    int resOneDimProblem;
	Mat matDistTrans(diffY, diffX, CV_32F);
	Mat matPointsX(diffY, diffX, CV_32S);
	int *pointsX = (int *)desposition->Xs->data;

	float *internalDistTrans = (float *)matDistTrans.data;
	int *internalPointsX = (int *)matPointsX.data;

    for (i = limit->t; i < limit->b; i++) {
        resOneDimProblem = DistanceTransformOneDimensionalProblem(
                                    f + i * diffX, diffX,
                                    coeff[0], coeff[2],
                                    internalDistTrans + i * diffX,
                                    internalPointsX + i * diffX,
									limit->l, limit->r);
        if (resOneDimProblem != DISTANCE_TRANSFORM_OK)
            return DISTANCE_TRANSFORM_ERROR;
    }
	matDistTrans = matDistTrans.t();
	internalDistTrans = (float *)matDistTrans.data;

	Mat matCosts(diffX, diffY, CV_32F, Scalar(0.f));
	Mat matPointsY(diffX, diffY, CV_32S);
    float *costs = (float *)matCosts.data;
	int *pointsY = (int *)matPointsY.data;

    for (j = limit->l; j < limit->r; j++) {
        resOneDimProblem = DistanceTransformOneDimensionalProblem(
                                    &internalDistTrans[j * diffY], diffY,
                                    coeff[1], coeff[3],
                                    costs + j * diffY,
                                    pointsY + j * diffY,
									limit->t, limit->b);
        if (resOneDimProblem != DISTANCE_TRANSFORM_OK)
            return DISTANCE_TRANSFORM_ERROR;
    }
	desposition->costs = new Mat(matCosts.t());
	desposition->Ys = new Mat(matPointsY.t());

	pointsY = (int *)desposition->Ys->data;
    for (i = limit->t; i < limit->b; i++) {
        for (j = limit->l; j < limit->r; j++) {
            int tmpY = pointsY[i * diffX + j];
            pointsX[i * diffX + j] = internalPointsX[tmpY * diffX + j];
        }
    }

    return DISTANCE_TRANSFORM_OK;
}

// Computation objective function D according the original paper
//
// INPUT
// filter            - filter object (weights and coefficients of penalty
//                     function that are used in this routine)
// featureMap        - feature pyramid
// level             - level number
// OUTPUT
// scoreFi           - values of distance transform on the level at all positions
// (pointsX, pointsY)- positions that correspond to the maximum value
//                     of distance transform at all grid nodes
static int filterDispositionLevel(const LsvmFilter *filter, int level,
								  const LsvmFeatureMap *featureMap,
								  LsvmFilterDisposition *desposition)
{
    int ftrMapY, ftrMapX, filterY, filterX, diffY, diffX;

    ftrMapY = featureMap->mSizeY;
    ftrMapX = featureMap->mSizeX;
    filterY = filter->sizeY;
    filterX = filter->sizeX;

    // Processing the situation when part filter goes
    // beyond the boundaries of the block set
    if (ftrMapY < filterY || ftrMapX < filterX)
        return FILTER_OUT_OF_BOUNDARIES;

    // Computation number of positions for the filter
    diffY = ftrMapY - filterY + 1;
    diffX = ftrMapX - filterX + 1;

	float *f = (float *)filter->mF[level].data;
    convolutionInv(filter, featureMap, f);

	QRect limit(0, 0, diffX, diffY);
	// Decision of the general distance transform task
    DistanceTransformTwoDimensionalProblem(f, diffY, diffX, filter->fineFunction, desposition, &limit);

    return LATENT_SVM_OK;
}

static int filterDispositionLevel2(const LsvmFilter *filter, int level,
								  const LsvmFeatureMap *featureMap,
								  LsvmFilterDisposition *desposition,
								  QRect *limit)
{
    int ftrMapY, ftrMapX, filterY, filterX, diffY, diffX;

    ftrMapY = featureMap->mSizeY;
    ftrMapX = featureMap->mSizeX;
    filterY = filter->sizeY;
    filterX = filter->sizeX;

    // Computation number of positions for the filter
    diffY = ftrMapY - filterY + 1;
    diffX = ftrMapX - filterX + 1;

	float *f = (float *)filter->mF[level].data;

	// Decision of the general distance transform task
    DistanceTransformTwoDimensionalProblem(f, diffY, diffX, filter->fineFunction, desposition, limit);

    return LATENT_SVM_OK;
}

// Transformation filter displacement from the block space
// to the space of pixels at the initial image
//
// INPUT
// countLevel        - the number of levels in the feature pyramid
// points            - the set of root filter positions (in the block space)
// levels            - the set of levels
// partsDisplacement - displacement of part filters (in the block space)
// kPoints           - number of root filter positions
// n                 - number of part filters
// initialImageLevel - level that contains features for initial image
// maxXBorder        - the largest root filter size (X-direction)
// maxYBorder        - the largest root filter size (Y-direction)
// OUTPUT
// points            - the set of root filter positions (in the space of pixels)
// partsDisplacement - displacement of part filters (in the space of pixels)
// RESULT
// Error status
int QLsvmDetector::convertPoints(int lambda, vector<MatchPos *> &matchPoses)
{
    float scale;

	vector<MatchPos *>::const_iterator it = matchPoses.begin();
    for (;it != matchPoses.end(); it++) {
		int level = (*it)->level;

		// scaling factor for root filter
        scale = mScales[level];
		Point &pt = (*it)->pt;
        pt.x = (int)((pt.x - mXBorder + 1) * scale);
        pt.y = (int)((pt.y - mYBorder + 1) * scale);

        // scaling factor for part filters
		 vector<Point> *pd = (*it)->partsDisplacement;
        scale = SIDE_LENGTH * powf(mStep, float(level - lambda));
        for (size_t j = 0; j < pd->size(); j++) {
            (*pd)[j].x = (int)(((*pd)[j].x - 2 * mXBorder + 1) * scale);
            (*pd)[j].y = (int)(((*pd)[j].y - 2 * mYBorder + 1) * scale);
        }
    }
    return LATENT_SVM_OK;
}

// Computation score function at the level that exceed threshold
//
// INPUT
// compFilters       - the set of filters (the first element is root filter,
//                     the other - part filters)
// n                 - the number of part filters
// H                 - feature pyramid
// level             - feature pyramid level for computation maximum score
// b                 - linear term of the score function
// maxXBorder        - the largest root filter size (X-direction)
// maxYBorder        - the largest root filter size (Y-direction)
// scoreThreshold    - score threshold
// OUTPUT
// score             - score function at the level that exceed threshold
// points            - the set of root filter positions (in the block space)
// levels            - the set of levels
// kPoints           - number of root filter positions
// partsDisplacement - displacement of part filters (in the block space)
int QLsvmDetector::thresholdFunctionalScoreFixedLevel(LsvmFilter **compFilters, int numPartFilter,
                                       int level, float b, vector<MatchPos *> &matchPoses)
{
    int i, j, k, ftrMapX, ftrMapY, filterY, filterX;
    int index, last;
    float sumDispositionCost;
    LsvmFeatureMap *featureMap;

	// FeatureMap matrix dimension on the level
    ftrMapX = mPyramid->mFeatureMaps[level]->mSizeX;
    ftrMapY = mPyramid->mFeatureMaps[level]->mSizeY;

    // Getting dimension of root filter
    filterY = compFilters[0]->sizeY;
    filterX = compFilters[0]->sizeX;

	// Processing the situation when root filter goes
    // beyond the boundaries of the block set
    if (filterY > ftrMapY || filterX > ftrMapX)
        return LATENT_SVM_FAILED_SUPERPOSITION;

    float *f = (float *)compFilters[0]->mF[level].data;

    // A dot product vectors of featureMap and weights of root filter
    convolution(compFilters[0], mPyramid->mFeatureMaps[level], f);

    // Computation values of function D for each part filter
    // on the level (level - LAMBDA)
    featureMap = mPartPyramid->mFeatureMaps[level];

    for (k = 1; k <= numPartFilter; k++) {
        filterDispositionLevel(compFilters[k], level,
			featureMap, compFilters[k]->mDisposition + level);
    }

    // Computation the maximum of score function

    // Construction of the set of positions for root filter
    // that correspond score function on the level that exceed threshold
    last = 0;
    int diffY = ftrMapY - filterY + 1;
    int diffX = ftrMapX - filterX + 1;

	for (i = 0; i < diffY; i++) {
		for (j = 0; j < diffX; j++) {
			sumDispositionCost = 0.0;
			vector<Point> meaningfulPts;

			for (k = 1; k <= numPartFilter; k++) {
				LsvmFilterDisposition *disposition = compFilters[k]->mDisposition + level;

				// This condition takes on a value true
				// when filter goes beyond the boundaries of block set
				int partFilterY = 2 * i + compFilters[k]->V.y;
				int partFilterX = 2 * j + compFilters[k]->V.x;
				int sizeY = featureMap->mSizeY - compFilters[k]->sizeY + 1;
				int sizeX = featureMap->mSizeX - compFilters[k]->sizeX + 1;

				float *costs = (float *)disposition->costs->data;
				int *Xs = (int *)disposition->Xs->data;
				int *Ys = (int *)disposition->Ys->data;

				if (partFilterY < sizeY && partFilterX < sizeX) {
					index = partFilterY * sizeX + partFilterX;
					sumDispositionCost += costs[index];
					Point partsPts(Xs[index], Ys[index]);
					meaningfulPts.push_back(partsPts);
				}
			}

			float score = f[i * diffX + j] - sumDispositionCost + b;
			if (score <= mScoreThreshold)
				continue;

			vector<Point> *pd = new vector<Point>;
			for (size_t l = 0; l < meaningfulPts.size(); l++)
				pd->push_back(meaningfulPts[l]);

			MatchPos *mp = new MatchPos(score, Point(j, i), pd, level);
			matchPoses.push_back(mp);
			last++;
		}
	}

	return LATENT_SVM_OK;
}

int QLsvmDetector::calConvolutions(LsvmFilter **compFilters, int numPartFilter,
								   int level, const Point *ftrRtCoord, vector<QSafeRect> *limits)
{
    int ftrMapX, ftrMapY, filterY, filterX;

	QSafeRect *limit = &limits[0][level];

	// FeatureMap matrix dimension on the level
    ftrMapX = mPyramid->mFeatureMaps[level]->mSizeX;
    ftrMapY = mPyramid->mFeatureMaps[level]->mSizeY;

    // Getting dimension of root filter
    filterY = compFilters[0]->sizeY;
    filterX = compFilters[0]->sizeX;

	// Processing the situation when root filter goes
    // beyond the boundaries of the block set
    if (filterY > ftrMapY || filterX > ftrMapX)
        return LATENT_SVM_FAILED_SUPERPOSITION;

    float *f = (float *)compFilters[0]->mF[level].data;

	// Since the ftrCoord is the center coordinates of a object windows,
	// we subtract the half size of convolution filter from the ftrCoord.
	int halfConvY = filterY >> 1;
	int halfConvX = filterX >> 1;

	Point rtCoord;
	rtCoord.x = ftrRtCoord->x - halfConvX;
	rtCoord.y = ftrRtCoord->y - halfConvY;

    // A dot product vectors of featureMap and weights of root filter
    convolution2(compFilters[0], compFilters[0]->mConvDone[level],
		mPyramid->mFeatureMaps[level], f, rtCoord, limit);

    // Computation values of function D for each part filter
    // on the level (level - LAMBDA)
    LsvmFeatureMap *featureMap = mPartPyramid->mFeatureMaps[level];

    for (int k = 1; k <= numPartFilter; k++) {
		Point ftrPtCoord;

		ftrPtCoord.y = 2 * rtCoord.y + compFilters[k]->V.y;
		ftrPtCoord.x = 2 * rtCoord.x + compFilters[k]->V.x;

		float *f = (float *)compFilters[k]->mF[level].data;

		convolutionInv2(compFilters[k], compFilters[k]->mConvDone[level],
			featureMap, f, ftrPtCoord, &limits[k][level]);
    }

    return LATENT_SVM_OK;
}

int QLsvmDetector::calDistanceAndScore(LsvmFilter **compFilters, int numPartFilter, float b,
									int level, vector<MatchPos *> &matchPoses, vector<QSafeRect> *limits)
{
	int ftrMapX, filterX;

	QRect *limit = &limits[0][level];
	// FeatureMap matrix dimension on the level
    ftrMapX = mPyramid->mFeatureMaps[level]->mSizeX;

    // Getting dimension of root filter
    filterX = compFilters[0]->sizeX;

	float *f = (float *)compFilters[0]->mF[level].data;

    // Computation the maximum of score function

    // Construction of the set of positions for root filter
    // that correspond score function on the level that exceed threshold
    int last = 0;
    int diffX = ftrMapX - filterX + 1;

	LsvmFeatureMap *featureMap = mPartPyramid->mFeatureMaps[level];

    for (int k = 1; k <= numPartFilter; k++) {
        filterDispositionLevel2(compFilters[k], level, featureMap,
			compFilters[k]->mDisposition + level, &limits[k][level]);
	}

	for (int i = limit->t; i < limit->b; i++) {
        for (int j = limit->l; j < limit->r; j++) {
            float sumDispositionCost = 0.0;
			vector<Point> meaningfulPts;

            for (int k = 1; k <= numPartFilter; k++) {
				LsvmFilterDisposition *partDisposition = compFilters[k]->mDisposition + level;

                // This condition takes on a value true
                // when filter goes beyond the boundaries of block set
				int partFilterY = 2 * i + compFilters[k]->V.y;
				int partFilterX = 2 * j + compFilters[k]->V.x;
				int sizeY = featureMap->mSizeY - compFilters[k]->sizeY + 1;
				int sizeX = featureMap->mSizeX - compFilters[k]->sizeX + 1;

				float *costs = (float *)partDisposition->costs->data;
				int *Xs = (int *)partDisposition->Xs->data;
				int *Ys = (int *)partDisposition->Ys->data;

                if (partFilterY < sizeY && partFilterX < sizeX) {
                    int index = partFilterY * sizeX + partFilterX;
                    sumDispositionCost += costs[index];
					Point partsPts(Xs[index], Ys[index]);
					meaningfulPts.push_back(partsPts);
                }
            }

            float score = f[i * diffX + j] - sumDispositionCost + b;
            if (score <= mScoreThreshold)
				continue;

			vector<Point> *pd = new vector<Point>;
            for (size_t k = 0; k < meaningfulPts.size(); k++)
				pd->push_back(meaningfulPts[k]);

			MatchPos *mp = new MatchPos(score, Point(j, i), pd, level);
			matchPoses.push_back(mp);
            last++;
        }
    }

	return LATENT_SVM_OK;
}

// Computation score function that exceed threshold
//
// INPUT
// compFilters       - the set of filters (the first element is root filter,
//                     the other - part filters)
// numPartFilter     - the number of part filters
// H                 - feature pyramid
// b                 - linear term of the score function
// maxXBorder        - the largest root filter size (X-direction)
// maxYBorder        - the largest root filter size (Y-direction)
// scoreThreshold    - score threshold
// OUTPUT
// score             - score function values that exceed threshold
// points            - the set of root filter positions (in the block space)
// levels            - the set of levels
// kPoints           - number of root filter positions
// partsDisplacement - displacement of part filters (in the block space)
int QLsvmDetector::thresholdFunctionalScore(LsvmFilter **compFilters, int numPartFilter,
                             float b, vector<MatchPos *> &matchPoses)
{
    // Computation the number of levels for seaching object,
    // first lambda-levels are used for computation values
    // of score function for each position of root filter

    // Computation maxima of score function on each level
    // and getting the maximum on all levels
    for (int level = 0; level < mPyramid->mNumLevels; level++) {
        int res = thresholdFunctionalScoreFixedLevel(compFilters, numPartFilter, level, b, matchPoses);
		if (res != LATENT_SVM_OK)
            return res;
    }

    return LATENT_SVM_OK;
}

// Computation of the root filter displacement and values of score function
//
// INPUT
// H                 - feature pyramid
// compFilters       - the set of filters (the first element is root filter,
//                     other elements - part filters)
// n                 - the number of part filters
// b                 - linear term of the score function
// maxXBorder        - the largest root filter size (X-direction)
// maxYBorder        - the largest root filter size (Y-direction)
// scoreThreshold    - score threshold
// OUTPUT
// points            - positions (x, y) of the upper-left corner
//                     of root filter frame
// levels            - levels that correspond to each position
// kPoints           - number of positions
// score             - values of the score function
// partsDisplacement - part filters displacement for each position
//                     of the root filter
int QLsvmDetector::searchObjectThreshold(LsvmFilter **compFilters, int numPartFilter,
                          float b, vector<MatchPos *> &matchPoses)
{
    // Matching
    int opResult = thresholdFunctionalScore(compFilters, numPartFilter, b, matchPoses);
    if (opResult != LATENT_SVM_OK)
        return LATENT_SVM_SEARCH_OBJECT_FAILED;

    // Transformation filter displacement from the block space
    // to the space of pixels at the initial image
    // that settles at the level number LAMBDA
    convertPoints(LAMBDA, matchPoses);

    return LATENT_SVM_OK;
}

// Compute opposite point for filter box
//
// INPUT
// point             - coordinates of filter top left corner
//                     (in the space of pixels)
// (sizeX, sizeY)    - filter dimension in the block space
// step              - scaling factor
// degree            - degree of the scaling factor
// OUTPUT
// oppositePoint     - coordinates of filter bottom corner
//                     (in the space of pixels)
// RESULT
// Error status
static int getOppositePoint(Point &point, float scale,
							int sizeX, int sizeY, Point *oppositePoint)
{
    oppositePoint->x = int(point.x + sizeX * scale);
    oppositePoint->y = int(point.y + sizeY * scale);

    return LATENT_SVM_OK;
}

// Computation right bottom corners coordinates of bounding boxes
//
// INPUT
// points            - left top corners coordinates of bounding boxes
// levels            - levels of feature pyramid where points were found
// (sizeX, sizeY)    - size of root filter
// OUTPUT
// oppositePoins     - right bottom corners coordinates of bounding boxes
int QLsvmDetector::estimateBoxes(vector<MatchPos *> &matchPoses, Point * &oppositePoints,
								 int sizeX, int sizeY)
{
	size_t numPoint = matchPoses.size();
    oppositePoints = (Point *)malloc(sizeof(Point) * numPoint);
    for (size_t i = 0; i < numPoint; i++) {
		float scale = mScales[matchPoses[i]->level];
        getOppositePoint(matchPoses[i]->pt, scale, sizeX, sizeY, &oppositePoints[i]);
	}

    return LATENT_SVM_OK;
}

// Addition nullable border to the feature map
//
// INPUT
// map               - feature map
// bx                - border size (X-direction)
// by                - border size (Y-direction)
static int addNullableBorder(LsvmFeatureMap *featureMap, int bx, int by)
{
    int sizeX, sizeY;
    float *newFeatureMap;
    sizeX = featureMap->mSizeX + 2 * bx;
    sizeY = featureMap->mSizeY + 2 * by;
	int numFeature = featureMap->mNumFeatures;
	newFeatureMap = new float[sizeX * sizeY * numFeature];
    for (int i = 0; i < sizeX * sizeY * numFeature; i++)
        newFeatureMap[i] = 0.0;

    for (int i = by; i < featureMap->mSizeY + by; i++) {
        for (int j = bx; j < featureMap->mSizeX + bx; j++) {
            for (int k = 0; k < numFeature; k++) {
                newFeatureMap[(i * sizeX + j) * numFeature + k] =
                    featureMap->mFeatureMat[((i - by) * featureMap->mSizeX + j - bx) * numFeature + k];
            }
        }
    }
    featureMap->mSizeX = sizeX;
    featureMap->mSizeY = sizeY;
    delete [] featureMap->mFeatureMat;
    featureMap->mFeatureMat = newFeatureMap;
    return LATENT_SVM_OK;
}

LsvmFeatureMap *QLsvmDetector::featureMapBorderPartFilter(LsvmFeatureMap *featureMap)
{
    int bx = mXBorder, by = mYBorder;
    int sizeX, sizeY;
    LsvmFeatureMap *newFeatureMaps;
	float *newFeatureMap;
	int numFeature = featureMap->mNumFeatures;

    sizeX = featureMap->mSizeX + 2 * bx;
    sizeY = featureMap->mSizeY + 2 * by;
	newFeatureMaps = new LsvmFeatureMap(sizeX, sizeY, numFeature);
	newFeatureMap = newFeatureMaps->mFeatureMat;

    for (int i = by; i < featureMap->mSizeY + by; i++) {
        for (int j = bx; j < featureMap->mSizeX + bx; j++) {
            for (int k = 0; k < numFeature; k++) {
                newFeatureMap[(i * sizeX + j) * numFeature + k] =
                    featureMap->mFeatureMat[((i - by) * featureMap->mSizeX + j - bx) * numFeature + k];
            }
        }
    }

    return newFeatureMaps;
}

// Creation feature pyramid with nullable border
//
// INPUT
// image             - initial image
// maxXBorder        - the largest root filter size (X-direction)
// maxYBorder        - the largest root filter size (Y-direction)
// RESULT
// Feature pyramid with nullable border
void QLsvmDetector::createFeaturePyramidWithBorder(const Mat &image)
{
    int opResult;

    // Obtaining feature pyramid
    opResult = getFeaturePyramid(image);
    if (opResult != LATENT_SVM_OK) {
        delete mPyramid;
        return;
    }

    // Addition nullable border for each feature map
    // the size of the border for root filters
    mXBorder = (int)ceilf(mMaxXBorder / 2.0f + 1.0f);
    mYBorder = (int)ceilf(mMaxYBorder / 2.0f + 1.0f);

    for (int level = 0; level < mPyramid->mNumLevels; level++)
        addNullableBorder(mPyramid->mFeatureMaps[level], mXBorder, mYBorder);

	int numPartLevel = mPyramid->mNumLevels - LAMBDA;
	mPartPyramid = new LsvmFeaturePyramid(numPartLevel);
	for (int partsLevel = 0; partsLevel < numPartLevel; partsLevel++) {
		mPartPyramid->mFeatureMaps[partsLevel] =
			featureMapBorderPartFilter(mPyramid->mFeatureMaps[partsLevel]);
	}
}

void QLsvmDetector::packAndPrepareBuffers()
{
	// pack
	for (int level = 0; level < LAMBDA; level++)
		delete mPyramid->mFeatureMaps[level];

	for (int level = LAMBDA; level < mPyramid->mNumLevels; level++) {
		int partsLevel = level - LAMBDA;
		mPyramid->mFeatureMaps[partsLevel] = mPyramid->mFeatureMaps[level];
	}
	mPyramid->mNumLevels = mPyramid->mNumLevels - LAMBDA;
	int numLevel = mPyramid->mNumLevels;

	mScales.resize(numLevel);
	for (int level = 0; level < numLevel; level++)
		mScales[level] = SIDE_LENGTH * powf(mStep, float(level));

	// prepare mConvDone, mF, mDisposition
	int rootIndex = 0;
	int diffY, diffX;

	for (int filterIdx = 0; filterIdx < mNumFilters; filterIdx++) {
		LsvmFilter *filter = mFilters[filterIdx];

		filter->mProbeDone = new Mat[numLevel];
		filter->mConvDone = new Mat[numLevel];
		filter->mF = new Mat[numLevel];
		filter->mDisposition = new LsvmFilterDisposition[numLevel];
	}

	mRootFilters.resize(mNumComponents);
	mRootFilterIdxs.resize(mNumComponents);

	for (int componentIdx = 0; componentIdx < mNumComponents; componentIdx++) {
		LsvmFilter **compFilters = &mFilters[rootIndex];
		LsvmFilter *rootFilter = *compFilters;
		mRootFilters[componentIdx] = rootFilter;
		mRootFilterIdxs[componentIdx] = rootIndex;

		int filterY = rootFilter->sizeY;
		int filterX = rootFilter->sizeX;

		rootFilter->mRealH.resize(numLevel);
		rootFilter->mRealW.resize(numLevel);

		for (int level = 0; level < numLevel; level++) {
			LsvmFeatureMap *featureMap = mPyramid->mFeatureMaps[level];
			int rtFtrMapY = featureMap->mSizeY;
			int rtFtrMapX = featureMap->mSizeX;

			diffY = rtFtrMapY - filterY + 1;
			diffX = rtFtrMapX - filterX + 1;

			rootFilter->mConvDone[level] = Mat::zeros(diffY, diffX, CV_8U);
			rootFilter->mF[level] = Mat::zeros(diffY, diffX, CV_32F);
			// note that ProbeDone's rows and cols are different from others
			rootFilter->mProbeDone[level] = Mat::zeros(rtFtrMapY, rtFtrMapX, CV_8U);

			float scale = mScales[level];
			rootFilter->mRealH[level] = static_cast<int>(filterY * scale + 0.5);
			rootFilter->mRealW[level] = static_cast<int>(filterX * scale + 0.5);

			LsvmFeatureMap *partFeatureMap = mPartPyramid->mFeatureMaps[level];
			int ftrMapY = partFeatureMap->mSizeY;
			int ftrMapX = partFeatureMap->mSizeX;
			for (int partIdx = 1; partIdx <= mNumPartFilters[componentIdx]; partIdx++) {
				LsvmFilter *partFilter = compFilters[partIdx];

				// part filter
				int filterY = partFilter->sizeY;
				int filterX = partFilter->sizeX;

				diffY = ftrMapY - filterY + 1;
				diffX = ftrMapX - filterX + 1;

				partFilter->mConvDone[level] = Mat::zeros(diffY, diffX, CV_8U);
				if (mClassName == "person") // no penalty (since threshold is still high)
					partFilter->mF[level] = Mat::zeros(diffY, diffX, CV_32F);
				else // penalty (since threshold is low)
					partFilter->mF[level] = Mat(diffY, diffX, CV_32F, Scalar(-mScoreThreshold + 0.15f));

				LsvmFilterDisposition *desposition = &partFilter->mDisposition[level];

				// Allocation memory for arrays for saving decisions
				// The member 'costs and Ys' are allocted in DistanceTransformTwoDimensionalProblem()
				desposition->Xs = new Mat(diffY, diffX, CV_32S);
			}
		}

		rootIndex += mNumPartFilters[componentIdx] + 1;
	}
}

// Computation root filters displacement and values of score function
//
// INPUT
// H                 - feature pyramid
// filters           - filters (root filter then it's part filters, etc.)
// kComponents       - root filters number
// kPartFilters      - array of part filters number for each componentIdx
// b                 - array of linear terms
// scoreThreshold    - score threshold
// OUTPUT
// points            - root filters displacement (top left corners)
// oppPoints         - root filters displacement (bottom right corners)
// score             - array of score values
// kPoints           - number of boxes
int QLsvmDetector::searchObjectThresholdSomeComponents(vector<DetectionResult> &dr)
{
    int i, compIdx;

	// Allocation memory
	vector<Point *> oppPointsArr(mNumComponents);
	vector<vector<MatchPos *> > matchPoseSet(mNumComponents);

    compIdx = 0;

    // For each componentIdx perform searching
    for (i = 0; i < mNumComponents; i++) {
		LsvmFilter **compFilters = &mFilters[compIdx];

        int error = searchObjectThreshold(compFilters, mNumPartFilters[i], mB[i], matchPoseSet[i]);
        if (error != LATENT_SVM_OK)
            return LATENT_SVM_SEARCH_OBJECT_FAILED;

		LsvmFilter *rootFilter = *compFilters;
        estimateBoxes(matchPoseSet[i], oppPointsArr[i], rootFilter->sizeX, rootFilter->sizeY);
        compIdx += mNumPartFilters[i] + 1;
    }

    for (i = 0; i < mNumComponents; i++) {
		vector<MatchPos *> &mps = matchPoseSet[i];
		dr[i].create(int(mps.size()));

		Point *points = dr[i].points;
		Point *oppPoints = dr[i].oppPoints;
		float *scores = dr[i].scores;

		vector<MatchPos *>::const_iterator it = mps.begin();
		for (int k = 0; it != mps.end(); it++, k++) {
			points[k] = (*it)->pt;
			oppPoints[k] = oppPointsArr[i][k];
			scores[k] = (*it)->score;
		}
    }

    // Release allocated memory
    for (i = 0; i < mNumComponents; i++)
        free(oppPointsArr[i]);

	for (int i = 0; i < mNumComponents; i++) {
		vector<MatchPos *>::const_iterator it = matchPoseSet[i].begin();
		for (; it != matchPoseSet[i].end(); it++)
			delete *it;
	}

    return LATENT_SVM_OK;
}

// find the next larger index in the level array
static int binaryLevelSearch(int key, int *levels, int numLevel)
{
	int l, h, m = 0;

	l = 0;
	h = numLevel - 1;

	while (h >= l) {
		int compar;

		m = (l + h) >> 1;
		compar = key - levels[m];

		if (compar < 0) {
			h = m - 1;
			if (key > levels[h])
				return m;
		} else if (compar > 0) {
			l = m + 1;
		} else {
			return m;
		}
	}

	return m;
}

class ConvThread : public SThread
{
public:
	ConvThread(QLsvmDetector *detector,
		vector<bool> &searchLevels,
		vector<FeatureMapCoord> *ftrMapCoords,
		long *fmcIdPtr,
		vector<vector<QSafeRect> > &limits)
	: mDetector(detector)
	, mSearchLevels(searchLevels)
	, mFtrMapCoords(ftrMapCoords)
	, mFmcIdPtr(fmcIdPtr)
	, mLimits(limits) {}

	inline long getNextFmcID() const { return qcmn_atomic_inc(mFmcIdPtr); }

	virtual bool threadLoop()
	{
		long id = getNextFmcID();
		if (unsigned(id) >= mFtrMapCoords->size())
			return false;

		const FeatureMapCoord *fmc = &(*mFtrMapCoords)[id];

		vector<int> &rootFilterIdxs = mDetector->mRootFilterIdxs;
		LsvmFilter ** &filters = mDetector->mFilters;
		int *numPartFilters = mDetector->mNumPartFilters;

		int level = fmc->level;
		int compIdx = fmc->compIdx;
		int filterIdx = rootFilterIdxs[compIdx];

		mSearchLevels[level] = true;
		int error = mDetector->calConvolutions(&filters[filterIdx], numPartFilters[compIdx],
			level, &fmc->pt, &mLimits[filterIdx]);
		if (error != LATENT_SVM_OK)
			return false;

		double timeLimit = mDetector->mTimeLimitMs;
		if (timeLimit >= 0 && ++mDetector->mTimeCount > TIME_COUNT) {
			QThinMeter *tm = &mDetector->mTm;
			mDetector->mTimeCount = 0;
			double timeLapsed = tm->getCurMilli();
			if (timeLapsed > timeLimit)
				return false;
		}

		return true;
	}

private:
	QLsvmDetector *mDetector;
	vector<bool> &mSearchLevels;
	vector<FeatureMapCoord> *mFtrMapCoords;
	long *mFmcIdPtr;
	vector<vector<QSafeRect> > &mLimits;
};

int QLsvmDetector::searchObjectComponentsWithOrder(vector<DetectionResult> &dr,
												   vector<FeatureMapCoord> *ftrMapCoords)
{
    int i, compIdx;
	int numLevel = mPyramid->mNumLevels;
	vector<bool> searchLevels(numLevel, false);

	if (mTimeLimitMs >= 0.) {
		mTm.reset();
		mTm.start();
	}

	vector<vector<QSafeRect> > limits(mNumFilters);
	for (i = 0; i < mNumFilters; i++) {
		limits[i].resize(numLevel);
		for (int j = 0; j < numLevel; j++)
			limits[i][j].init();
	}

#if Q1_NUM_THREADS > 1
	long fmcIdx = -1;

	cv::Ptr<ConvThread> ct[Q1_NUM_THREADS];
	for (i = 0; i < Q1_NUM_THREADS; i++) {
		ct[i] = new ConvThread(this, searchLevels, ftrMapCoords, &fmcIdx, limits);
		ct[i]->run();
	}
	for (i = 0; i < Q1_NUM_THREADS; i++)
		ct[i]->join();
#else
    // For each componentIdx perform searching
	vector<FeatureMapCoord>::const_iterator it = ftrMapCoords->begin();
	for (; it != ftrMapCoords->end(); it++) {
		const FeatureMapCoord *fmc = &(*it);

		int level = fmc->level;
		int compIdx = fmc->compIdx;
		int filterIdx = mRootFilterIdxs[compIdx];

		searchLevels[level] = true;
		int error = calConvolutions(&mFilters[filterIdx], mNumPartFilters[compIdx],
			level, &fmc->pt, &limits[filterIdx]);
		if (error != LATENT_SVM_OK)
			return error;

		if (mTimeLimitMs >= 0) {
			double timeLapsed = mTm.getCurMilli();
			if (timeLapsed > mTimeLimitMs)
				break;
		}
	}
#endif

	if (mTimeLimitMs >= 0.)
		mTm.stop();

	vector<Point *> oppPointsArr(mNumComponents);
	vector<vector<MatchPos *> > matchPoseSet(mNumComponents);
	compIdx = 0;
	for (i = 0; i < mNumComponents; i++) {
		LsvmFilter **compFilters = &mFilters[compIdx];

		for (int level = 0; level < numLevel; level++) {
			if (!searchLevels[level])
				continue;

			calDistanceAndScore(compFilters, mNumPartFilters[i],
				mB[i], level, matchPoseSet[i], &limits[compIdx]);
		}

		// Transformation filter displacement from the block space
		// to the space of pixels at the initial image
		// that settles at the level number LAMBDA
		convertPoints(LAMBDA, matchPoseSet[i]);

		LsvmFilter *rootFilter = *compFilters;
        estimateBoxes(matchPoseSet[i], oppPointsArr[i], rootFilter->sizeX, rootFilter->sizeY);
        compIdx += mNumPartFilters[i] + 1;
	}

    for (i = 0; i < mNumComponents; i++) {
		vector<MatchPos *> &mps = matchPoseSet[i];
		dr[i].create(int(mps.size()));

		Point *points = dr[i].points;
		Point *oppPoints = dr[i].oppPoints;
		float *scores = dr[i].scores;

		vector<MatchPos *>::const_iterator it = mps.begin();
		for (int k = 0; it != mps.end(); it++, k++) {
			points[k] = (*it)->pt;
			oppPoints[k] = oppPointsArr[i][k];
			scores[k] = (*it)->score;
		}
    }

    // Release allocated memory
    for (i = 0; i < mNumComponents; i++)
        free(oppPointsArr[i]);

	for (int i = 0; i < mNumComponents; i++) {
		vector<MatchPos *>::const_iterator it = matchPoseSet[i].begin();
		for (; it != matchPoseSet[i].end(); it++)
			delete *it;
	}
    return LATENT_SVM_OK;
}

// Elimination boxes that are outside the image boudaries
//
// INPUT
// width             - image wediht
// height            - image heigth
// points            - a set of points (coordinates of top left or
//                     bottom right corners)
// kPoints           - points number
// OUTPUT
// points            - updated points (if coordinates less than zero then
//                     set zero coordinate, if coordinates more than image
//                     size then set coordinates equal image size)
static int clippingBoxes(int width, int height, Point *points, int kPoints)
{
    int i;
    for (i = 0; i < kPoints; i++) {
        if (points[i].x > width - 1)
            points[i].x = width - 1;
		else if (points[i].x < 0)
            points[i].x = 0;

        if (points[i].y > height - 1)
            points[i].y = height - 1;
		else if (points[i].y < 0)
            points[i].y = 0;
    }
    return LATENT_SVM_OK;
}

static void sort(int n, const float* x, int* indices)
{
    int i, j;
    for (i = 0; i < n; i++) {
        for (j = i + 1; j < n; j++) {
            if (x[indices[j]] > x[indices[i]])
				swap(indices[i], indices[j]);
        }
	}
}

// Perform non-maximum suppression algorithm (described in original paper)
// to remove "similar" bounding boxes
//
// INPUT
// numBoxes          - number of bounding boxes
// points            - array of left top corner coordinates
// oppositePoints    - array of right bottom corner coordinates
// score             - array of detection scores
// overlapThreshold  - threshold: bounding box is removed if overlap part
//                     is greater than passed value
// OUTPUT
// numBoxesOut       - the number of bounding boxes algorithm returns
// pointsOut         - array of left top corner coordinates
// oppositePointsOut - array of right bottom corner coordinates
// scoreOut          - array of detection scores
// RESULT
// Error status
static int nonMaximumSuppression(DetectionResult *drIn, DetectionResult *drOut, float overlapThreshold,
								int stableCount)
{
    int i, j, index;
	int numBoxes = drIn->count;
	Point *points = drIn->points;
	Point *oppPoints = drIn->oppPoints;
	float *scores = drIn->scores;

	vector<float> box_areas(numBoxes);
	vector<int> indices(numBoxes);
	vector<int> is_suppressed(numBoxes);

    for (i = 0; i < numBoxes; i++) {
        indices[i] = i;
        is_suppressed[i] = 0;
        box_areas[i] = (float)((oppPoints[i].x - points[i].x + 1) *
                              (oppPoints[i].y - points[i].y + 1));
    }

	if (numBoxes - stableCount > 0)
		sort(numBoxes - stableCount, scores, &indices[stableCount]);

    for (i = 0; i < numBoxes; i++) {
        if (is_suppressed[indices[i]])
			continue;

        for (j = i + 1; j < numBoxes; j++) {
            if (is_suppressed[indices[j]])
				continue;

            int x1max = max(points[indices[i]].x, points[indices[j]].x);
            int x2min = min(oppPoints[indices[i]].x, oppPoints[indices[j]].x);
            int y1max = max(points[indices[i]].y, points[indices[j]].y);
            int y2min = min(oppPoints[indices[i]].y, oppPoints[indices[j]].y);
            int overlapWidth = x2min - x1max + 1;
            int overlapHeight = y2min - y1max + 1;
            if (overlapWidth <= 0 || overlapHeight <= 0)
				continue;

            float overlapPart = (overlapWidth * overlapHeight) / box_areas[indices[j]];
            if (overlapPart > overlapThreshold || scores[indices[j]] < 0)
                is_suppressed[indices[j]] = 1;
        }
    }

    int numBoxesOut = 0;
    for (i = 0; i < numBoxes; i++) {
        if (!is_suppressed[i])
			numBoxesOut++;
    }

	drOut->create(numBoxesOut);

	Point *pointsOut = drOut->points;
	Point *oppPointsOut = drOut->oppPoints;
	float *scoresOut = drOut->scores;

    index = 0;
    for (i = 0; i < numBoxes; i++) {
        if (!is_suppressed[indices[i]]) {
            pointsOut[index].x = points[indices[i]].x;
            pointsOut[index].y = points[indices[i]].y;
            oppPointsOut[index].x = oppPoints[indices[i]].x;
            oppPointsOut[index].y = oppPoints[indices[i]].y;
            scoresOut[index] = scores[indices[i]];
            index++;
        }
    }

    return LATENT_SVM_OK;
}

// load trained detector from a file
//
// INPUT
// filename             - path to the file containing the parameters of
//                      - trained Latent SVM detector
// OUTPUT
// trained Latent SVM detector in internal representation
static QLsvmDetector *loadLatentSvmDetector(const char* filename)
{
    LsvmFilter **filters = 0; // models or filter objects
    int kFilters = 0;
    int kComponents = 0;
    int *kPartFilters = 0;
    float *b = 0;
    float scoreThreshold = 0.f;
    float ratioThreshold = 1.54f; // to better use proposals
    int err;

    err = QloadModel(filename, filters, &kFilters, &kComponents, &kPartFilters, b, &scoreThreshold, &ratioThreshold);
    if (err != LATENT_SVM_OK)
		return NULL;

	QLsvmDetector *detector = new QLsvmDetector(kFilters, kComponents, kPartFilters, filters, b, scoreThreshold, ratioThreshold);

	int compIdx = 0;
	for (int filterIdx = 0; filterIdx < kFilters; filterIdx += kPartFilters[compIdx] + 1) {
		LsvmFilter *rootFilter = filters[filterIdx];

		rootFilter->mRealRatio = float(rootFilter->sizeY) / rootFilter->sizeX;
	}

    return detector;
}

// Computation maximum filter size for each dimension
//
// INPUT
// filters           - a set of filters (at first root filter, then part filters
//                     and etc. for all components)
// kComponents       - number of components
// kPartFilters      - number of part filters for each componentIdx
// OUTPUT
// maxXBorder        - maximum of filter size at the horizontal dimension
// maxYBorder        - maximum of filter size at the vertical dimension
int QLsvmDetector::calMaxFilterDims()
{
	int compIdx = mNumPartFilters[0] + 1;

	mMaxXBorder = mFilters[0]->sizeX;
	mMaxYBorder = mFilters[0]->sizeY;

	for (int i = 1; i < mNumComponents; i++) {
		if ((unsigned)mFilters[compIdx]->sizeX > mMaxXBorder)
			mMaxXBorder = mFilters[compIdx]->sizeX;

		if ((unsigned)mFilters[compIdx]->sizeY > mMaxYBorder)
			mMaxYBorder = mFilters[compIdx]->sizeY;

		compIdx += mNumPartFilters[i] + 1;
	}

	return LATENT_SVM_OK;
}

// find rectangular regions in the given image that are likely
// to contain objects and corresponding confidence levels
//
// INPUT
// image                - image to detect objects in
// detector             - Latent SVM detector in internal representation
// storage              - memory storage to store the resultant sequence
//                        of the object candidate rectangles
// overlap_threshold    - threshold for the non-maximum suppression algorithm
//                        [here will be the reference to original paper]
// OUTPUT
// sequence of detected objects
// (bounding boxes and confidence levels stored in QCvObjectDetection structures)
void QLsvmDetector::detect(vector<QCvObjectDetection> &detections,
						int w, int h, float overlapThreshold,
						vector<FeatureMapCoord> *ftrMapCoords)
{
	vector<DetectionResult> dr1(mNumComponents), dr2(mNumComponents);
	DetectionResult dr3, dr4;

    // Search object
	int error;
	if (ftrMapCoords && ftrMapCoords->size())
		error = searchObjectComponentsWithOrder(dr1, ftrMapCoords);
	else
		error = searchObjectThresholdSomeComponents(dr1);

    if (error != LATENT_SVM_OK)
        return;

	int count = 0;
	for (int i = 0; i < mNumComponents; i++) {
		// Clipping boxes
		clippingBoxes(w, h, dr1[i].points, dr1[i].count);
		clippingBoxes(w, h, dr1[i].oppPoints, dr1[i].count);

		// NMS procedure
		nonMaximumSuppression(&dr1[i], &dr2[i], overlapThreshold, 0);
		count += dr2[i].count;
	}

	dr3.create(count);

	int stableCount = 0;
	int k = 0;
	for (int i = mNumComponents - 1; i >= 0; i--) { // bigger components first
		for (size_t j = 0; j < size_t(dr2[i].count); j++, k++) {
			dr3.scores[k] = dr2[i].scores[j];
			dr3.points[k] = dr2[i].points[j];
			dr3.oppPoints[k] = dr2[i].oppPoints[j];
			// assume the dr3.scores is sorted in descending order
			if (mClassName == "person" &&
				mNumComponents - 1 == i &&
				dr3.scores[k] > STABLE_THRESHOLD)
				stableCount++;
		}
	}

	// To turn back to previous NMS, set stableCount zero
	nonMaximumSuppression(&dr3, &dr4, overlapThreshold, stableCount);

	Point *pointsOut = dr4.points;
	Point *oppPointsOut = dr4.oppPoints;
	float *scoreOut = dr4.scores;

	for (int i = 0; i < dr4.count; i++) {
		QCvObjectDetection detection;
		CvRect *bounding_box = &detection.rect;
		detection.score = scoreOut[i];
		bounding_box->x = pointsOut[i].x;
		bounding_box->y = pointsOut[i].y;
		bounding_box->width = oppPointsOut[i].x - pointsOut[i].x;
		bounding_box->height = oppPointsOut[i].y - pointsOut[i].y;
		detections.push_back(detection);
	}
}

QLsvmDetector::QLsvmDetector(int numFilters, int numComponents, int *numPartFilter,
	LsvmFilter **filters, float *b, float scoreThreshold, float ratioThreshold)
: mNumFilters(numFilters)
, mNumComponents(numComponents)
, mB(b)
, mScoreThreshold(scoreThreshold)
, mPyramid(NULL)
, mPartPyramid(NULL)
, mRatioThreshold(ratioThreshold) // again, to better use proposals
, mNumPartFilters(numPartFilter)
, mFilters(filters)
, mTimeLimitMs(-1.)
, mTimeCount(0)
{ }

QLsvmDetector::~QLsvmDetector()
{
	free(mB);
	free(mNumPartFilters);

	clear();

	for (int i = 0; i < mNumFilters; i++) {
		free(mFilters[i]->H);
		if (mFilters[i])
			delete mFilters[i];
	}
	free(mFilters);
}

void QLsvmDetector::cvtBox2FtrMapCoord(vector<QRect> *searchBoxes, vector<FeatureMapCoord> *ftrMapCoords)
{
	// look for the right level
	int numLevel = mPyramid->mNumLevels;

	vector<QRect>::const_iterator it = searchBoxes->begin();
	for (; it != searchBoxes->end(); it++) {
		const QRect *region = &(*it);

		for (int i = 0; i < mNumComponents; i++) {
			LsvmFilter *rootFilter = mRootFilters[i];

			int objH = region->b - region->t + 1;
			int objW = region->r - region->l + 1;

			float hwr = float(objH) / objW;

			float l = max(rootFilter->mRealRatio, hwr);
			float s = min(rootFilter->mRealRatio, hwr);
			if (l / s >= mRatioThreshold)
				continue;

			int l1 = binaryLevelSearch(objW, &rootFilter->mRealW[0], numLevel);
			int l2 = binaryLevelSearch(objH, &rootFilter->mRealH[0], numLevel);

			int level1, level2;
#ifdef LAYER1
			const int additional = 0;
#else
			const int additional = 1;
#endif
			if (l1 > l2) {
				level1 = max(l2 - additional, 0);
				level2 = min(l1 + additional, numLevel - 1);
			} else {
				level1 = max(l1 - additional, 0);
				level2 = min(l2 + additional, numLevel - 1);
			}

			Point pt;
			pt.y = (region->b + region->t) / 2 - 1;
			pt.x = (region->r + region->l) / 2 - 1;

			for (int level = level2; level >= level1; level--) {
				float scale = mScales[level];
				int y = int(mYBorder + pt.y / scale - 1);
				int x = int(mXBorder + pt.x / scale - 1);
				if (rootFilter->mProbeDone[level].at<uchar>(y, x))
					continue;
				rootFilter->mProbeDone[level].at<uchar>(y, x)++;

				FeatureMapCoord cc;
				cc.pt.x = x;
				cc.pt.y = y;
				cc.compIdx = i;
				cc.level = level;

				ftrMapCoords->push_back(cc);
			}
		}
	}
}

void QLsvmDetector::genFullFtrMapCoord(vector<FeatureMapCoord> *ftrMapCoords)
{
	const int hop = RELAX_VAL * 2 - 1;

	for (int compIdx = 0; compIdx < mNumComponents; compIdx++) {
		LsvmFilter *rootFilter = mRootFilters[compIdx];
		for (int level = mPyramid->mNumLevels - 1; level >= 0; level--) {
			int h = rootFilter->mProbeDone[level].rows;
			int w = rootFilter->mProbeDone[level].cols;

			for (int y = RELAX_VAL - 1; y < h; y += hop) {
				for (int x = RELAX_VAL - 1; x < w; x += hop) {
					if (rootFilter->mProbeDone[level].at<uchar>(y, x))
						continue;
					rootFilter->mProbeDone[level].at<uchar>(y, x)++;

					FeatureMapCoord cc;
					cc.pt.x = x;
					cc.pt.y = y;
					cc.compIdx = compIdx;
					cc.level = level;

					ftrMapCoords->push_back(cc);
				}
			}
		}
	}
}

void QLsvmDetector::genRandFtrMapCoord(vector<FeatureMapCoord> *ftrMapCoords)
{
	const int hop = RELAX_VAL * 2 - 1;
	const int relax1 = RELAX_VAL - 1;
	int trials = 0;

	while (trials < 100) {
		int compIdx = rand() % mNumComponents;
		LsvmFilter *rootFilter = mRootFilters[compIdx];
		int level = rand() % mPyramid->mNumLevels;
		int h = rootFilter->mProbeDone[level].rows;
		int w = rootFilter->mProbeDone[level].cols;

		int y = (rand() % ((h - relax1 - 1) / hop)) * hop + relax1;
		int x = (rand() % ((w - relax1 - 1) / hop)) * hop + relax1;

		if (rootFilter->mProbeDone[level].at<uchar>(y, x)) {
			trials++;
			continue;
		}

		rootFilter->mProbeDone[level].at<uchar>(y, x)++;

		FeatureMapCoord cc;
		cc.pt.x = x;
		cc.pt.y = y;
		cc.compIdx = compIdx;
		cc.level = level;

		ftrMapCoords->push_back(cc);

		trials = 0;
	}

	genFullFtrMapCoord(ftrMapCoords);
}

static inline void pushCoord(LsvmFilter *rootFilter, vector<FeatureMapCoord> *ftrMapCoords,
							int x, int y, int compIdx, int level)
{
	int h = rootFilter->mProbeDone[level].rows;
	int w = rootFilter->mProbeDone[level].cols;

	if (x < 0 || y < 0 || x >= w || y >= h)
		return;

	if (rootFilter->mProbeDone[level].at<uchar>(y, x))
		return;

	rootFilter->mProbeDone[level].at<uchar>(y, x)++;

	FeatureMapCoord cc;
	cc.pt.x = x;
	cc.pt.y = y;
	cc.compIdx = compIdx;
	cc.level = level;

	ftrMapCoords->push_back(cc);
}

void QLsvmDetector::genCenteredFtrMapCoord(vector<FeatureMapCoord> *ftrMapCoords)
{
	const int hop = RELAX_VAL * 2 - 1;

	for (int level = mPyramid->mNumLevels - 1; level >= 0; level--) {
		for (int compIdx = 0; compIdx < mNumComponents; compIdx++) {
			LsvmFilter *rootFilter = mRootFilters[compIdx];
			int h = rootFilter->mProbeDone[level].rows;
			int w = rootFilter->mProbeDone[level].cols;

			int xHalf, yHalf;
			if (h > w) {
				int ratio = int(float(h) / w + 0.5);
				xHalf = 0;
				yHalf = ratio;
			} else {
				int ratio = int(float(w) / h + 0.5);
				xHalf = 0;
				yHalf = ratio;
			}
			int cH = (h - 1) / 2;
			int cW = (w - 1) / 2;

			while (cH - yHalf >= 0 || cH + yHalf < h || cW - xHalf >= 0 || cW + xHalf < w) {
				int y, x;

				x = cW - xHalf;
				for (y = cH + yHalf; y >= cH - yHalf; y -= hop)
					pushCoord(rootFilter, ftrMapCoords, x, y, compIdx, level);

				y = cH - yHalf;
				for (x = cW - xHalf; x <= cW + xHalf; x += hop)
					pushCoord(rootFilter, ftrMapCoords, x, y, compIdx, level);

				x = cW + xHalf;
				for (y = cH - yHalf; y <= cH + yHalf; y += hop)
					pushCoord(rootFilter, ftrMapCoords, x, y, compIdx, level);

				y = cH + yHalf;
				for (x = cW + xHalf; x >= cW - xHalf; x -= hop)
					pushCoord(rootFilter, ftrMapCoords, x, y, compIdx, level);

				yHalf += hop;
				xHalf += hop;
			}
		}
	}
}

typedef pair<float, Point> P;

struct CandidateSort {
	bool operator()(const P &left, const P &right) {
		return left.first > right.first;
	}
};

void QLsvmDetector::clear()
{
	for (int i = 0; i < mNumFilters; i++)
		mFilters[i]->clear();

	if (mPyramid) {
		delete mPyramid;
		mPyramid = NULL;
	}
	if (mPartPyramid) {
		delete mPartPyramid;
		mPartPyramid = NULL;
	}
}

void QUniLsvmDetector::clear()
{
    for (size_t i = 0; i < detectors.size(); i++) {
        delete detectors[i];
		detectors[i] = NULL;
	}

    detectors.clear();
    classNames.clear();
}

void QUniLsvmDetector::finish()
{
	for (size_t classID = 0; classID < detectors.size(); classID++)
		detectors[classID]->clear();
}

bool QUniLsvmDetector::load(const vector<string> &filenames, const vector<string> &_classNames)
{
    clear();

    CV_Assert(_classNames.empty() || _classNames.size() == filenames.size());

    for (size_t i = 0; i < filenames.size(); i++) {
        const string filename = filenames[i];
        if (filename.length() < 5 || filename.substr(filename.length() - 4, 4) != ".xml")
            continue;

        QLsvmDetector *detector = loadLatentSvmDetector(filename.c_str());
        if (detector) {
			string className;
            if (_classNames.empty())
                className = extractModelName(filenames[i]);
            else
                className = _classNames[i];

            classNames.push_back(className);
			detector->setClassName(className);
            detectors.push_back(detector);
        }
    }

    return !empty();
}

void QUniLsvmDetector::setup(const Mat &image, float overlapThreshold, double timeLimitMs)
{
	mW = image.cols;
	mH = image.rows;
	mOverlapThreshold = overlapThreshold;
	CV_Assert(image.ptr() != NULL);

	for (size_t classID = 0; classID < detectors.size(); classID++) {
		QLsvmDetector *detector = detectors[classID];

		// Getting maximum filter dimensions
		detector->calMaxFilterDims();

		// Create feature pyramid with nullable border
		detector->createFeaturePyramidWithBorder(image);

		detector->packAndPrepareBuffers();
		detector->setTimeLimit(timeLimitMs);
	}
}

void QUniLsvmDetector::cvtBox2FtrMapCoord(vector<QRect> *searchBoxes, vector<FeatureMapCoord> *ftrMapCoords)
{
	for (size_t classID = 0; classID < detectors.size(); classID++)
		detectors[classID]->cvtBox2FtrMapCoord(searchBoxes, ftrMapCoords);
}

void QUniLsvmDetector::genFullFtrMapCoord(vector<FeatureMapCoord> *ftrMapCoords)
{
	for (size_t classID = 0; classID < detectors.size(); classID++)
		detectors[classID]->genFullFtrMapCoord(ftrMapCoords);
}

void QUniLsvmDetector::genRandFtrMapCoord(vector<FeatureMapCoord> *ftrMapCoords)
{
	for (size_t classID = 0; classID < detectors.size(); classID++)
		detectors[classID]->genRandFtrMapCoord(ftrMapCoords);
}

void QUniLsvmDetector::genCenteredFtrMapCoord(vector<FeatureMapCoord> *ftrMapCoords)
{
	for (size_t classID = 0; classID < detectors.size(); classID++)
		detectors[classID]->genCenteredFtrMapCoord(ftrMapCoords);
}

void QUniLsvmDetector::setTimeLimit(double timeLimit)
{
	for (size_t classID = 0; classID < detectors.size(); classID++)
		detectors[classID]->setTimeLimit(timeLimit);
}

void QUniLsvmDetector::setRatioThreshold(float ratioThreshold)
{
	for (size_t classID = 0; classID < detectors.size(); classID++)
		detectors[classID]->setRatioThreshold(ratioThreshold);
}

void QUniLsvmDetector::detect(vector<ObjectDetection> &objectDetections,
							  vector<vector<FeatureMapCoord> *> &fmcss)
{
    objectDetections.clear();

    for (size_t classID = 0; classID < detectors.size(); classID++) {
		vector<QCvObjectDetection> detections;
		vector<FeatureMapCoord> *ftrMapCoords;
		if (classID < fmcss.size())
			ftrMapCoords = fmcss[classID];
		else
			ftrMapCoords = NULL;

        detectors[classID]->detect(detections, mW, mH, mOverlapThreshold, ftrMapCoords);

		// convert results
		vector<QCvObjectDetection>::const_iterator it = detections.begin();
		for (; it != detections.end(); it++)
			objectDetections.push_back(ObjectDetection(Rect(it->rect), it->score, (int)classID));
    }

	finish();
}
