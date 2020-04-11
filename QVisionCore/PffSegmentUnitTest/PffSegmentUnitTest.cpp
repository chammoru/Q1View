/*
Copyright (C) 2006 Pedro Felzenszwalb

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
*/

#include <cstdio>
#include <cstdlib>
#include <segment-image.h>

#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <iostream>

using namespace std;
using namespace cv;

// random color
Vec3b random_rgb(){ 
  Vec3b c;
  
  c[0] = uchar(rand() % 256);
  c[1] = uchar(rand() % 256);
  c[2] = uchar(rand() % 256);

  return c;
}

// argument example: 0.5 500 20 ../../DataSet/01_Edge/peppers.png ./out.png
int main(int argc, char **argv) {
  if (argc != 6) {
    fprintf(stderr, "usage: %s sigma k min input(ppm) output(ppm)\n", argv[0]);
    return 1;
  }
  
  float sigma = (float)atof(argv[1]);
  float k = (float)atof(argv[2]);
  int min_size = atoi(argv[3]);
	
  printf("loading input image.\n");
  Mat input = imread(argv[4], CV_LOAD_IMAGE_COLOR);
	
  printf("processing\n");
  int num_ccs;
  Mat seg = segment_image(input, sigma, k, min_size, &num_ccs);

    // pick random colors for each component
  Mat colors(seg.size(), CV_8UC3);
  for (int i = 0; i < seg.rows * seg.cols; i++)
    colors.at<Vec3b>(i) = random_rgb();

  Mat randColorImg(seg.size(), CV_8UC3);
  for (int i = 0; i < seg.rows; i++) {
	  for (int j = 0; j < seg.cols; j++)
		  randColorImg.at<Vec3b>(i, j) = colors.at<Vec3b>(seg.at<int>(i, j));
  }

  imwrite(argv[5], randColorImg);

  printf("got %d components\n", num_ccs);
  printf("done! uff...thats hard work.\n");

  return 0;
}
