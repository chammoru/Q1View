#pragma once

#include "qimage_cs.h"
#include "qimage_metrics.h"
#include "QImageDiff.h"

class FrmCmpInfo
{
	inline void init() { memset(mMetrics, 0, sizeof(mMetrics)); }

public:
	bool mParseDone;
	double mMetrics[METRIC_COUNT][QPLANES];

	list<RLC> diffRLC[QPLANES];

	inline FrmCmpInfo() : mParseDone(false) {}
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
