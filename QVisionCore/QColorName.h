#pragma once

#include <string>
#include <opencv2/core/core.hpp>

namespace q1 {

class ColorName
{
    /*  giving a image, return the type of the color, 11 types in total */
    public:
        ColorName(const std::string &model);   /* in: path of the color model*/
        cv::Mat getColorName(const cv::Mat &src) const;
		cv::Mat cvt2Actual(const cv::Mat &src);
		cv::Mat getSameCnPos(const cv::Mat &src, int cn) const;

    private:
        /*  no copy */
        ColorName(const ColorName & rhs);
        ColorName& operator=( const ColorName &rhs);

        /*
         * ===  FUNCTION  ======================================================================
         *         Name:  getMaxIndex
         *  Description:  fill the matrix mW2cMax
         * =====================================================================================
         */
        bool getMaxIndex();
        /* model file, word_2_color map */
        cv::Mat mW2c;
        /* stores the index of max value of the mW2c */
        std::vector<uchar> mW2cMax;
        /* each color's corresponding RGB value */
        std::vector<cv::Vec3b> mColors;
        /* ready to go ? */
        bool mModelValid;
};

}
