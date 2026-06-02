#pragma once

#include "qimage_cs.h"
#include "qimage_metrics.h"
#include "QImageDiff.h"

class FrmCmpInfo
{
public:
	inline void init() {
		memset(mMetrics, 0, sizeof(mMetrics));
		memset(mMetricDone, 0, sizeof(mMetricDone));
	}

	bool mParseDone;
	double mMetrics[METRIC_COUNT][QPLANES];
	bool mMetricDone[METRIC_COUNT];

	list<RLC> diffRLC[QPLANES];

	inline FrmCmpInfo() : mParseDone(false) { init(); }
	inline ~FrmCmpInfo()
	{
		deinit();
	}

	inline void deinit()
	{
		for (int i = 0; i < QPLANES; i++)
			diffRLC[i].clear();
	}

	friend class FileScanThread;
};
