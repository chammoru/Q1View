
#include "filter.h"

/* convolve image with gaussian filter */
image<float> *smooth(image<float> *src, float sigma) {
  std::vector<float> mask = make_fgauss(sigma);
  normalize(mask);

  image<float> *tmp = new image<float>(src->height(), src->width(), false);
  image<float> *dst = new image<float>(src->width(), src->height(), false);
  convolve_even(src, tmp, mask);
  convolve_even(tmp, dst, mask);

  delete tmp;
  return dst;
}

image<float> *smooth(image<uchar> *src, float sigma) {
  image<float> *tmp = imageUCHARtoFLOAT(src);
  image<float> *dst = smooth(tmp, sigma);
  delete tmp;
  return dst;
}
