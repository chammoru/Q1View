#include <cmath>
#include <QCommon.h>
#include <QDebug.h>

#include "QStructuralEdge.h"
#include "QCvUtil.h"
#include "QDerivative.h"

using namespace std;
using namespace cv;

namespace q1 {

template <typename T>
static int sgn(T val)
{
    return (T(0) < val) - (val < T(0));
}

void StructuralEdge::loadModel(const string &path)
{
	FILE *fp = fopen(path.c_str(), "rb");
	if (fp == NULL) {
		LOGERR("path: %s", path.c_str());
		CV_Error(Error::StsAssert, "unable to open model file");
	}

	fread(&mImWidth, sizeof(int), 1, fp);
	fread(&mGtWidth, sizeof(int), 1, fp);

	fread(&mNumTrees, sizeof(int), 1, fp);
	fread(&mNumTreeNodes, sizeof(int), 1, fp);

	fread(&mNumOrients, sizeof(int), 1, fp);
	fread(&mGrdSmooth, sizeof(float), 1, fp);
	fread(&mChnSmooth, sizeof(float), 1, fp);
	fread(&mSimSmooth, sizeof(float), 1, fp);
	fread(&mNormRad, sizeof(float), 1, fp);
	fread(&mShrink, sizeof(int), 1, fp);
	fread(&mNumCells, sizeof(int), 1, fp);
	fread(&mRgbd, sizeof(int), 1, fp);

	fread(&mStride, sizeof(int), 1, fp);
	fread(&mMultiscale, sizeof(int), 1, fp);
	fread(&mSharpen, sizeof(int), 1, fp);
	fread(&mNumTreesEval, sizeof(int), 1, fp);
	fread(&mNumThreads, sizeof(int), 1, fp);
	fread(&mNms, sizeof(int), 1, fp);

	fread(&mNumChns, sizeof(int), 1, fp);
	fread(&mNumChnFtrs, sizeof(uint), 1, fp);
	fread(&mNumSimFtrs, sizeof(int), 1, fp);

	const int nTotalNodes = mNumTrees * mNumTreeNodes;
	mThrs = new float[nTotalNodes];
	mFids.resize(nTotalNodes);
	mChild.resize(nTotalNodes);
	mSegs.resize(mGtWidth * mGtWidth * nTotalNodes);
	mNumSegs.resize(nTotalNodes);
	fread(mThrs, sizeof(float), nTotalNodes, fp);
	fread(&mFids[0], sizeof(uint), nTotalNodes, fp);
	fread(&mChild[0], sizeof(uint), nTotalNodes, fp);
	fread(&mSegs[0], sizeof(uchar), mGtWidth * mGtWidth * nTotalNodes, fp);
	fread(&mNumSegs[0], sizeof(uchar), nTotalNodes, fp);

	fread(&mElemBinsSize, sizeof(int), 1, fp);
	mElemBins.resize(mElemBinsSize);
	fread(&mElemBins[0], sizeof(ushort), mElemBinsSize, fp);

	fread(&mElemBndsSize, sizeof(int), 1, fp);
	mElemBnds.resize(mElemBndsSize);
	fread(&mElemBnds[0], sizeof(uint), mElemBndsSize, fp);

	mStride = std::max(mStride, mShrink);
	mNumTreesEval = std::min(mNumTreesEval, mNumTrees);
	mNumBnds = (mElemBndsSize - 1) / (nTotalNodes);
	mSharpen = std::min(mSharpen, mNumBnds - 1);
}

void StructuralEdge::setup(const string &modelPath)
{
	loadModel(modelPath);
	mFe = new FeatureExtractor(mShrink, mNumOrients, mGrdSmooth, mChnSmooth, mSimSmooth);
}

static void buildLookup(int H, int W, int C, int w, vector<uint> &ids)
{
	ids.resize(w * w * C);
	int total = H * W;
	int n = 0;
	for (int c = 0; c < C; c++)
		for (int x = 0; x < w; x++)
			for (int y = 0; y < w; y++)
				ids[n++] = c * total + y * W + x;
}

static void buildLookupSs(int H, int W, int C, int w, int m,
						  vector<uint> &cids1, vector<uint> &cids2)
{
	vector<int> locs(m);
	int s = int(w / m / 2.0 + .5);
	for (int i = 0; i < m; i++)
		locs[i] = uint((i + 1) * (w + 2 * s - 1) / (m + 1.0) - s + .5);
	int m2 = m * m;
	int size = m2 * (m2 - 1) / 2 * C;
	cids1.resize(size);
	cids2.resize(size);
	int n = 0;
	int total = H * W;
	for (int z = 0; z < C; z++) {
		for(int i = 0; i < m2; i++) {
			for(int j = i + 1; j < m2; j++) {
				int z1 = z * total;
				n++;
				int y = i % m;
				int x = (i - y) / m;
				cids1[n - 1] = z1 + locs[y] * W + locs[x];
				y = j % m;
				x = (j - y) / m;
				cids2[n - 1] = z1 + locs[y] * W + locs[x];
			}
		}
	}
}

Mat StructuralEdge::detect(const Mat &I, vector<Mat> &chns) const
{
	const int H = I.rows, W = I.cols, C = I.channels();
	const int H1 = (int)ceil(double(H - mImWidth) / mStride);
	const int W1 = (int)ceil(double(W - mImWidth) / mStride);
	const int H2 = H1 * mStride + mGtWidth;
	const int W2 = W1 * mStride + mGtWidth;
	const int H3 = H / mShrink;
	const int W3 = W / mShrink;
	const int sImWidth = mImWidth / mShrink;
	const int sStride = mStride / mShrink;
	const int total1 = H1 * W1;
	const int total3 = H3 * W3;

	vector<float> _chnsReg(total3 * mNumChns);
	vector<float> _chnsSim(total3 * mNumChns);
	vector<Mat> chnsReg(mNumChns), chnsSim(mNumChns);
	for (int i = 0; i < mNumChns; i++) { // for continuous memory
		chnsReg[i] = Mat(H3, W3, CV_32F, &_chnsReg[total3 * i]);
		chnsSim[i] = Mat(H3, W3, CV_32F, &_chnsSim[total3 * i]);
	}
	mFe->getSmoothFtrs(chns, chnsReg, chnsSim);

	// construct lookup tables
	vector<uint> iids, eids, cids, cids1, cids2;
	buildLookup(H,  W ,        C, mGtWidth, iids);
	buildLookup(H2, W2,        1, mGtWidth, eids);
	buildLookup(H3, W3, mNumChns, sImWidth, cids);
	buildLookupSs(H3, W3, mNumChns, sImWidth, mNumCells, cids1, cids2);

	vector<uint> ind(total1 * mNumTreesEval);
	for (int t = 0; t < mNumTreesEval; t++) {
		uint *ind_t = &ind[total1 * t];
		for (int y0 = 0; y0 < 2; y0++) {
			for (int y = y0; y < H1; y += 2) {
				const uint *ind_2 = ind_t + y * W1;
				for (int x = 0; x < W1; x++) {
					int o = (y * sStride) * W3 + (x * sStride);
					// select tree to evaluate
					int t1 = ((y + x) % 2 * mNumTreesEval + t) % mNumTrees;
					uint k = t1 * mNumTreeNodes;
					while (mChild[k]) {
						// compute feature (either channel or self-similarity feature)
						uint f = mFids[k];
						float ftr;
						if (f < mNumChnFtrs)
							ftr = _chnsReg[cids[f] + o];
						else
							ftr = _chnsSim[cids1[f - mNumChnFtrs] + o] - _chnsSim[cids2[f - mNumChnFtrs] + o];
						// compare ftr to threshold and move left or right accordingly
						if (ftr < mThrs[k])
							k = mChild[k] - 1;
						else
							k = mChild[k];
						k += t1 * mNumTreeNodes;
					}
					// store leaf index and update edge maps
					ind_t[y * W1 + x] = k;
				}
			}
		}
	}

	// compute edge maps
	Mat E(H2, W2, CV_32F, Scalar(0));
	if (!mSharpen) {
		const int sGtWidth = mGtWidth / mStride;
		for (int t = 0; t < mNumTreesEval; t++) {
			uint *ind_t = &ind[t * total1];
			for (int y = 0; y < H1; y++) {
				for (int x0 = 0; x0 < sGtWidth; x0++) {
					for (int x = x0; x < W1; x += sGtWidth) {
						int x1 = x * mStride;
						uint k = ind_t[y * W1 + x];
						float *E1 = E.ptr<float>() + (y * mStride) * W2 + x1;
						int b0 = mElemBnds[k * mNumBnds];
						int b1 = mElemBnds[k * mNumBnds + 1];
						if (b0 == b1)
							continue;
						for (int b = b0; b < b1; b++)
							E1[eids[mElemBins[b]]]++;
					}
				}
			}
		}
	} else { // computed sharpened edge maps, snapping to local color values
		// compute neighbors array
		const int g = mGtWidth;
		const int g2 = g * g;
		vector<ushort> N(g2 * 4);
		for (int y = 0; y < g; y++) {
			for (int x = 0; x < g; x++) {
				int i = x * g + y;
				ushort *N1 = &N[i * 4];
				N1[0] = x > 0     ? i - g : i;
				N1[1] = x < g - 1 ? i + g : i;
				N1[2] = y > 0     ? i - 1 : i;
				N1[3] = y < g - 1 ? i + 1 : i;
			}
		}
		vector<uchar> S(g2 * sizeof(uchar));
		vector<float> vs(C);
		const int pad = (mImWidth - g) / 2;
		for (int t = 0; t < mNumTreesEval; t++) {
			uint *ind_t = &ind[t * total1];
			for (int y = 0; y < H1; y++) {
				for (int x = 0; x < W1; x++) {
					// get current segment and copy into S
					uint k = ind_t[y * W1 + x];
					int m = mNumSegs[k];
					if (m == 1)
						continue;
					memcpy(&S[0], &mSegs[k * g2], g2 * sizeof(uchar));
					// compute color model for each segment using every other pixel
					vector<float> ns(m, 0.f), mus(m * C, 0.f);
					const float *I1 = I.ptr<float>() + ((y * mStride + pad) * W + (x * mStride + pad)) * C;
					for (int yi = 0; yi < g; yi += 2) {
						for (int xi = 0; xi < g; xi += 2) {
							int s = S[xi * g + yi];
							ns[s]++;
							for (int z = 0; z < C; z++)
								mus[s * C + z] += I1[(yi * W + xi) * C + (C - z - 1)];
						}
					}
					for (int s = 0; s < m; s++)
						for (int z = 0; z < C; z++)
							mus[s * C + z] /= ns[s];
					// update segment S according to local color values
					int b0 = mElemBnds[k * mNumBnds];
					int b1 = mElemBnds[k * mNumBnds + mSharpen];
					for (int b = b0; b < b1; b++) {
						float d, e, eBest = 1e10f;
						int sBest=-1, ss[4];
						for (int i = 0; i < 4; i++)
							ss[i] = S[N[mElemBins[b] * 4 + i]];
						for (int z = 0; z < C; z++)
							vs[z]=I1[iids[mElemBins[b]] * C + (C - z - 1)];
						for (int i = 0; i < 4; i++) {
							int s = ss[i];
							if (s == sBest)
								continue;
							e = 0;
							for (int z = 0; z < C; z++) {
								d = mus[s * C + z] - vs[z];
								e += d*d;
							}
							if (e < eBest) {
								eBest=e;
								sBest=s;
							}
						}
						S[mElemBins[b]] = sBest;
					}
					// convert mask to edge maps (examining expanded set of pixels)
					float *E1 = E.ptr<float>() + y * mStride * W2 + x * mStride;
					b1 = mElemBnds[k * mNumBnds + mSharpen + 1];
					for (int b = b0; b < b1; b++) {
						int i = mElemBins[b];
						uchar s = S[i];
						ushort *N1 = &N[i * 4];
						if (s != S[N1[0]] || s != S[N1[1]] || s != S[N1[2]] || s != S[N1[3]])
							E1[eids[i]]++;
					}
				}
			}
		}
	}

	return E;
}

// return I[x, y] via bilinear interpolation
static inline float interp(float *I, int h, int w, float x, float y)
{
	x = x < 0 ? 0 : (x > w - 1.001f ? w - 1.001f : x);
	y = y < 0 ? 0 : (y > h - 1.001f ? h - 1.001f : y);
	int x0 = int(x), y0 = int(y), x1 = x0 + 1, y1 = y0 + 1;
	float dx0 = x - x0, dy0 = y - y0, dx1 = 1 - dx0, dy1 = 1 - dy0;
	return I[y0 * w + x0] * dx1 * dy1 +
		I[y0 * w + x1] * dx0 * dy1 +
		I[y1 * w + x0] * dx1 * dy0 +
		I[y1 * w + x1] * dx0 * dy0;
}

static void _edgeNms(Mat &E, const Mat &O, int r, int s, float m)
{
	Mat E0 = E.clone();
	int h = E.rows, w = E.cols;

	// supress edges where edge is stronger in orthogonal direction
	for (int y = 0; y < h; y++) {
		for (int x = 0; x < w; x++) {
			float e = E.at<float>(y, x);
			if (!e)
				continue;
			e *= m;
			float coso = cos(O.at<float>(y, x));
			float sino = sin(O.at<float>(y, x));
			for (int d = -r; d <= r; d++) {
				if (!d)
					continue;
				float e0 = interp(E0.ptr<float>(), h, w, x + d * coso, y + d * sino);
				if (e < e0) {
					E.at<float>(y, x) = 0;
					break;
				}
			}
		}
	}
	// supress noisy edge estimates near boundaries
	s = std::min(s, std::min(w / 2, h / 2));
	float sf = float(s);
	for (int x = 0; x < s; x++) {
		for (int y = 0; y < h; y++) {
			E.at<float>(y, x) *= x / sf;
			E.at<float>(y, w - 1 - x) *= x / sf;
		}
	}
	for (int x = 0; x < w; x++) {
		for (int y = 0; y < s; y++) {
			E.at<float>(y, x) *= y / sf;
			E.at<float>(h - 1 - y, x) *= y / sf;
		}
	}
}

void StructuralEdge::edgeNms(cv::Mat &E, int r, int s, float m) const
{
	// compute approximate orientation O from edges E
	Mat E4O, Ox, Oy, Oxx, Oxy, Oyy;
	convTri(E, E4O, 4);
	gradient(E4O, Ox , Oy , CV_32F, QCV_DIFF_CENTRAL);
	gradient(Ox , Oxx, Oxy, CV_32F, QCV_DIFF_CENTRAL);
	gradient(Oy , Oxy, Oyy, CV_32F, QCV_DIFF_CENTRAL);

	Mat O(E.size(), CV_32F);
	for (int y = 0; y < E.rows; y++) {
		for (int x = 0; x < E.cols; x++) {
			float val = atan(Oyy.at<float>(y, x) * sgn(-Oxy.at<float>(y, x))
				/ (Oxx.at<float>(y, x) + 0.00001f));
			if (val < 0)
				val += float(CV_PI); // Since y=atain() ranges [-pi/2, pi/2]
			O.at<float>(y, x) = val;
		}
	}
	_edgeNms(E, O, r, s, m);
}

Mat StructuralEdge::detectEdges(const Mat &img) const
{
	Mat padded, padded3f;
	const int h = img.rows;
	const int w = img.cols;
	const int p = mImWidth / 2;
	const int t = p;
	const int b = ROUNDUP_DWORD(h + mImWidth) - p - h;
	const int l = p;
	const int r = ROUNDUP_DWORD(w + mImWidth) - p - w;
	copyMakeBorder(img, padded, t, b, l, r, BORDER_REFLECT);
	vector<Mat> chns;
	padded.convertTo(padded3f, DataType<float>::type, 1/255.f);
	mFe->getFeatures(padded3f, chns);
	Mat I;
	if (mSharpen)
		convTri(padded3f, I, 1);
	else
		I = padded3f;
	Mat E = detect(I, chns);
	E = E(Rect(mGtWidth / 2, mGtWidth / 2, w, h)).clone(); // clone for contiguous

	 // normalize and finalize edge maps
	float s = 1.f * mStride * mStride / (mGtWidth * mGtWidth) / mNumTreesEval;
	switch (mSharpen) {
	case 0:
		s *= 2.f;
		break;
	case 1:
		s *= 1.8f;
		break;
	default:
		s *= 1.66f;
	}
	convTri(E.mul(s), E, 1);

	if (mNms)
		edgeNms(E, 1, 5, 1.01f);
	
	// TODO: use 'edgeNms(E, 2, 0, 1);' for Edge-Boxes

	return E;
}

}
