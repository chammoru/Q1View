#pragma once

#include "QFeatureExtraction.h"

namespace q1 {

class StructuralEdge {
	// (1) model parameters
	int mImWidth;      // width of image patches
	int mGtWidth;      // width of ground truth patches

	// (2) tree parameters
	int mNumTrees;     // number of trees in forest to train
	int mNumTreeNodes; // maximum number of tree nodes in each tree

	// (3) feature parameters
	int mNumOrients;   // number of orientations per gradient scale
	float mGrdSmooth;  // radius for image gradient smoothing (using convTri)
	float mChnSmooth;  // radius for reg channel smoothing (using convTri)
	float mSimSmooth;  // radius for sim channel smoothing (using convTri)
	float mNormRad;    // gradient normalization radius (see gradientMag)
	int mShrink;       // amount to shrink channels
	int mNumCells;     // number of self similarity cells
	int mRgbd;         // 0:RGB, 1:depth, 2:RBG+depth (for NYU data only)

	// (4) detection parameters (can be altered after training)
	int mStride;       // stride at which to compute edges
	int mMultiscale;   // if true run multiscale edge detector
	int mSharpen;      // sharpening amount (can only decrease after training)
	int mNumTreesEval; // number of trees to evaluate per location
	int mNumThreads;   // number of threads for evaluation of trees
	bool mNms;         // if true apply non-maximum suppression to edges

	// (5) other parameters:
	int mNumChns;      // number of channels of the feature
	uint mNumChnFtrs;
	int mNumSimFtrs;
	int mElemBinsSize;  // number of elements in eBins
	int mElemBndsSize;  // number of elements in eBnds
	int mNumBnds;

	float *mThrs;       // [nTreeNodes x nTrees]
	std::vector<uint> mFids;// [nTreeNodes x nTrees]
	std::vector<uint> mChild;// [nTreeNodes x nTrees]
	std::vector<uchar> mSegs;// [gtWidth x gtWidth x nTreeNodes x nTrees]
	std::vector<uchar> mNumSegs;// [nTreeNodes x nTrees]
	std::vector<ushort> mElemBins;
	std::vector<uint> mElemBnds;

	cv::Ptr<FeatureExtractor> mFe;

	void loadModel(const std::string &path);
	cv::Mat detect(const cv::Mat &I, std::vector<cv::Mat> &chns) const;

public:
	void setup(const std::string &modelPath);
	void edgeNms(cv::Mat &E, int r, int s, float m) const;
	cv::Mat detectEdges(const cv::Mat &img) const;
};

}
