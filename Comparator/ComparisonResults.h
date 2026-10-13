#pragma once

#include "ComparatorPane.h"
#include "FrmCmpInfo.h"
#include <memory>

struct IFrmCmpStrategy;

// Stable source slots: A = 0, B = 1, C = 2, D = 3. Closing B does not rename C.
struct PairCurrentResult
{
	unsigned long long request = 0;
	CString full, channels, roi, state;
};

struct PairSequenceSummary
{
	int eligible = 0, parsed = 0;
	int measured[METRIC_COUNT] = {};
	double sum[METRIC_COUNT][QPLANES] = {};
	double minimum[METRIC_COUNT][QPLANES] = {};
	double maximum[METRIC_COUNT][QPLANES] = {};
	int finite[METRIC_COUNT][QPLANES] = {};
	int infinite[METRIC_COUNT][QPLANES] = {};
	CString error, metricError[METRIC_COUNT];
};

// UI buffers are copied once per affected source. Workers never borrow a buffer
// that playback, seeking, or source replacement can mutate.
struct CurrentComparisonRequest
{
	unsigned long long id = 0;
	unsigned targets = 0; // Candidate source-slot bits: B=2, C=4, D=8.
	int metric = 0, width = 0, height = 0;
	bool hasRoi = false;
	int l = 0, t = 0, r = 0, b = 0;
	std::shared_ptr<ComparatorPane> sources[4];
	IFrmCmpStrategy* strategies[3] = {};
};
