#pragma once

#include "qimage_cs.h"

namespace q1 {

int image_parse_num(const char* str, int* num);

int image_parse_w_h(const char* str, int* w, int* h);
int image_parse_arg(char* str, int* num, const char* key);
int image_parse_arg(char* str, double* num, const char* key);

const struct qcsc_info* const image_find_cs(const struct qcsc_info* ci,
	const char* name);
void image_sort_cs(struct qcsc_info* ci);
const char* image_find_cs_name(struct qcsc_info* ci, int cs);

int image_resolution_idx(int w_ref, int h_ref);

} // namespace q1
