/*
 *  By downloading, copying, installing or using the software you agree to this license.
 *  If you do not agree to this license, do not download, install,
 *  copy or use the software.
 *
 *
 *  License Agreement
 *  For Open Source Computer Vision Library
 *  (3 - clause BSD License)
 *
 *  Redistribution and use in source and binary forms, with or without modification,
 *  are permitted provided that the following conditions are met :
 *
 *  *Redistributions of source code must retain the above copyright notice,
 *  this list of conditions and the following disclaimer.
 *
 *  * Redistributions in binary form must reproduce the above copyright notice,
 *  this list of conditions and the following disclaimer in the documentation
 *  and / or other materials provided with the distribution.
 *
 *  * Neither the names of the copyright holders nor the names of the contributors
 *  may be used to endorse or promote products derived from this software
 *  without specific prior written permission.
 *
 *  This software is provided by the copyright holders and contributors "as is" and
 *  any express or implied warranties, including, but not limited to, the implied
 *  warranties of merchantability and fitness for a particular purpose are disclaimed.
 *  In no event shall copyright holders or contributors be liable for any direct,
 *  indirect, incidental, special, exemplary, or consequential damages
 *  (including, but not limited to, procurement of substitute goods or services;
 *  loss of use, data, or profits; or business interruption) however caused
 *  and on any theory of liability, whether in contract, strict liability,
 *  or tort(including negligence or otherwise) arising in any way out of
 *  the use of this software, even if advised of the possibility of such damage.
 */

#ifndef __OPENCV_EDGEFILTER_HPP__
#define __OPENCV_EDGEFILTER_HPP__
#ifdef __cplusplus

#include <opencv2/core.hpp>

namespace cv
{
namespace ximgproc
{

//! @addtogroup ximgproc_filters
//! @{

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

/** @brief Interface for implementations of Fast Global Smoother filter.

For more details about this filter see @cite Min2014 and @cite Farbman2008 .
*/
class CV_EXPORTS_W FastGlobalSmootherFilter : public Algorithm
{
public:
    /** @brief Apply smoothing operation to the source image.

    @param src source image for filtering with unsigned 8-bit or signed 16-bit or floating-point 32-bit depth and up to 4 channels.

    @param dst destination image.
    */
    CV_WRAP virtual void filter(InputArray src, OutputArray dst) = 0;
};

/** @brief Factory method, create instance of FastGlobalSmootherFilter and execute the initialization routines.

@param guide image serving as guide for filtering. It should have 8-bit depth and either 1 or 3 channels.

@param lambda parameter defining the amount of regularization

@param sigma_color parameter, that is similar to color space sigma in bilateralFilter.

@param lambda_attenuation internal parameter, defining how much lambda decreases after each iteration. Normally,
it should be 0.25. Setting it to 1.0 may lead to streaking artifacts.

@param num_iter number of iterations used for filtering, 3 is usually enough.

For more details about Fast Global Smoother parameters, see the original paper @cite Min2014. However, please note that
there are several differences. Lambda attenuation described in the paper is implemented a bit differently so do not
expect the results to be identical to those from the paper; sigma_color values from the paper should be multiplied by 255.0 to
achieve the same effect. Also, in case of image filtering where source and guide image are the same, authors
propose to dynamically update the guide image after each iteration. To maximize the performance this feature
was not implemented here.
*/
CV_EXPORTS_W Ptr<FastGlobalSmootherFilter> createFastGlobalSmootherFilter(InputArray guide, double lambda, double sigma_color, double lambda_attenuation=0.25, int num_iter=3);

/** @brief Simple one-line Fast Global Smoother filter call. If you have multiple images to filter with the same
guide then use FastGlobalSmootherFilter interface to avoid extra computations.

@param guide image serving as guide for filtering. It should have 8-bit depth and either 1 or 3 channels.

@param src source image for filtering with unsigned 8-bit or signed 16-bit or floating-point 32-bit depth and up to 4 channels.

@param dst destination image.

@param lambda parameter defining the amount of regularization

@param sigma_color parameter, that is similar to color space sigma in bilateralFilter.

@param lambda_attenuation internal parameter, defining how much lambda decreases after each iteration. Normally,
it should be 0.25. Setting it to 1.0 may lead to streaking artifacts.

@param num_iter number of iterations used for filtering, 3 is usually enough.
*/
CV_EXPORTS_W void fastGlobalSmootherFilter(InputArray guide, InputArray src, OutputArray dst, double lambda, double sigma_color, double lambda_attenuation=0.25, int num_iter=3);

//! @}
}
}
#endif
#endif
