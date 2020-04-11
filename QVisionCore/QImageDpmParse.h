// Copyright (C) 2014 Kyuwon Kim <chammoru@gmail.com>

#ifndef __QIMAGE_DPM_PARSE_H__
#define __QIMAGE_DPM_PARSE_H__

int QloadModel(const char *modelPath,
              LsvmFilter ** &filters,
              int *kFilters,
              int *kComponents,
              int **kPartFilters,
              float * &b,
              float *scoreThreshold,
              float *ratioThreshold);

#endif /* __QIMAGE_DPM_PARSE_H__ */
