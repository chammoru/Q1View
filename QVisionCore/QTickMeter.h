#ifndef __QTICK_METER__
#define __QTICK_METER__

#include <opencv2/core/core.hpp>

class QTickMeter
{
public:
	QTickMeter() { reset(); }
	void start(){ startTime = cv::getTickCount(); }
	void stop()
	{
		int64 time = cv::getTickCount();
		if ( startTime == 0 )
			return;
		++counter;
		sumTime += ( time - startTime );
		startTime = 0;
	}
	int64 getTimeTicks() const { return sumTime; }
	double getTimeMicro() const { return (double)getTimeTicks()/(cv::getTickFrequency()*1e-6); }
	double getTimeMilli() const { return getTimeMicro()*1e-3; }
	double getTimeSec() const { return getTimeMilli()*1e-3; }
	int64 getCounter() const { return counter; }

	void reset() {startTime = 0; sumTime = 0; counter = 0; }

private:
	int64 counter;
	int64 sumTime;
	int64 startTime;
};

#endif /* __QTICK_METER__ */
