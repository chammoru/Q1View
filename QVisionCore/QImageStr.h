#pragma once

#include "qimage_cs.h"

int qimage_parse_num(const char *str, int *num);

int qimage_parse_w_h(const char *str, int *w, int *h);
int qimage_parse_arg(char *str, int *num, const char *key);
int qimage_parse_arg(char* str, double* num, const char* key);

const struct qcsc_info * const qimage_find_cs(struct qcsc_info *ci,
											  const char *name);
void qimage_sort_cs(struct qcsc_info *ci);
const char *qimage_find_cs_name(struct qcsc_info *ci, int cs);

int qimage_resolution_idx(int w_ref, int h_ref);
