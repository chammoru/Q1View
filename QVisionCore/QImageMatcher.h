// Copyright (C) 2014 Kyuwon Kim <chammoru@gmail.com>

/*------------------------------------------------------------------------------------------*\
   This file contains material supporting chapter 9 of the cookbook:  
   Computer Vision Programming using the OpenCV Library. 
   by Robert Laganiere, Packt Publishing, 2011.

   This program is free software; permission is hereby granted to use, copy, modify, 
   and distribute this source code, or portions thereof, for any purpose, without fee, 
   subject to the restriction that the copyright notice may not be removed 
   or altered from any source or altered source distribution. 
   The software is released on an as-is basis and without any warranties of any kind. 
   In particular, the software is not guaranteed to be fault-tolerant or free from failure. 
   The author disclaims all warranties with regard to this software, any use, 
   and any consequent failure, is purely the responsibility of the user.
 
   Copyright (C) 2010-2011 Robert Laganiere, www.laganiere.name
   Copyright (C) 2014 Kyuwon Kim <chammoru@gmail.com>
\*------------------------------------------------------------------------------------------*/

#ifndef __QIMAGE_MATCHER__
#define __QIMAGE_MATCHER__

#include <vector>
#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/features2d/features2d.hpp>
#include <opencv2/calib3d/calib3d.hpp>

#if CV_MAJOR_VERSION <= 2
#include <opencv2/nonfree/features2d.hpp>
#endif

class RobustMatcher {
private:
	// pointer to the feature point detector object
	cv::Ptr<cv::FeatureDetector> mDetector;
	// pointer to the feature descriptor extractor object
	cv::Ptr<cv::DescriptorExtractor> mExtractor;
	float mRatio; // max ratio between 1st and 2nd NN (1st/2nd)
	bool mRefineF; // if true will refine the F matrix
	double mDistance; // min distance to epipolar
	double mConfidence; // confidence level (probability)

public:
	RobustMatcher() : mRatio(0.65f), mRefineF(true), mDistance(3.0), mConfidence(0.99) {
		// SURF is the default feature
#if CV_MAJOR_VERSION <= 2
		mDetector = new cv::SurfFeatureDetector();
		mExtractor = new cv::SurfDescriptorExtractor();
#endif
	}

	// Set the feature detector
	void setFeatureDetector(cv::Ptr<cv::FeatureDetector>& detect) { mDetector = detect; }
	// Set descriptor extractor
	void setDescriptorExtractor(cv::Ptr<cv::DescriptorExtractor>& desc) { mExtractor = desc; }
	// Set the minimum distance to epipolar in RANSAC
	void setMinDistanceToEpipolar(double d) { mDistance = d; }
	// Set confidence level in RANSAC
	void setConfidenceLevel(double c) { mConfidence = c; }
	// Set the NN ratio
	void setRatio(float r) { mRatio = r; }
	// if you want the F matrix to be recalculated
	void refineFundamental(bool flag) { mRefineF = flag; }

	// Clear matches for which NN ratio is > than threshold
	// return the number of removed points 
	// (corresponding entries being cleared, i.e. size will be 0)
	int ratioTest(std::vector<std::vector<cv::DMatch> >& matches);

	// Insert symmetrical matches in symMatches std::vector
	void symmetryTest(const std::vector<std::vector<cv::DMatch> >& matches1,
			const std::vector<std::vector<cv::DMatch> >& matches2,
			std::vector<cv::DMatch>& symMatches);

	// Identify good matches using RANSAC
	// Return fundemental matrix
	cv::Mat ransacTest(const std::vector<cv::DMatch>& matches,
			const std::vector<cv::KeyPoint>& keypoints1,
			const std::vector<cv::KeyPoint>& keypoints2,
			std::vector<cv::DMatch>& outMatches);

	// Match feature points using symmetry test and RANSAC
	// returns fundemental matrix
	cv::Mat match(cv::Mat& image1, cv::Mat& image2); // input images
};

#endif
