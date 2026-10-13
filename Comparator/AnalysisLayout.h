#pragma once

#include <algorithm>

struct AnalysisHeights {
	int source = 0, footer = 0, graph = 0;
};

// Preserve the preferred layout when it fits. In small/high-DPI windows first
// shrink the scrollable graph, then the scrollable footer, rather than letting
// their ideal heights consume the image canvas. All inputs/results are pixels.
inline AnalysisHeights ResolveAnalysisHeights(int content, int graph, int footer,
	int minimumSource, int minimumGraph, int minimumFooter, int dividers)
{
	const int available=(std::max)(0,content-(std::max)(0,dividers));
	AnalysisHeights result;
	result.graph=(std::min)((std::max)(0,graph),available);
	result.footer=(std::min)((std::max)(0,footer),available-result.graph);
	int deficit=(std::max)(0,(std::min)((std::max)(0,minimumSource),available)
		-(available-result.graph-result.footer));
	auto shrink=[&](int& height,int floor) {
		const int reduction=(std::min)(deficit,(std::max)(0,height-(std::max)(0,floor)));
		height-=reduction; deficit-=reduction;
	};
	shrink(result.graph,minimumGraph);
	shrink(result.footer,minimumFooter);
	// Extremely small windows cannot fit every minimum. Keep sizes nonnegative
	// and prioritize the image; normal windows retain readable panel minima.
	shrink(result.graph,0);
	shrink(result.footer,0);
	result.source=available-result.graph-result.footer;
	return result;
}
