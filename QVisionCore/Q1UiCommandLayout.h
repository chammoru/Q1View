#pragma once
#include <algorithm>
#include <vector>

namespace q1view {
struct WindowsUiCommandLayoutItem { int width, order, priority; };
struct WindowsUiCommandLayoutPlan { std::vector<int> visible, overflow; };

// Geometry-only policy; widths have already been measured using the actual
// font, DPI and accessibility scale. priority >= 100 is always in More.
inline WindowsUiCommandLayoutPlan PlanWindowsUiCommandRow(
	const std::vector<WindowsUiCommandLayoutItem>& items, int more, int available)
{
	WindowsUiCommandLayoutPlan plan;
	int total = more >= 0 ? items[more].width : 0;
	for (int i = 0; i < int(items.size()); ++i) {
		if (i == more) continue;
		if (items[i].priority >= 100) plan.overflow.push_back(i);
		else { plan.visible.push_back(i); total += items[i].width; }
	}
	while (total > available && !plan.visible.empty()) {
		auto least = std::max_element(plan.visible.begin(), plan.visible.end(), [&](int a, int b) {
			return items[a].priority != items[b].priority ? items[a].priority < items[b].priority : items[a].order < items[b].order;
		});
		total -= items[*least].width; plan.overflow.push_back(*least); plan.visible.erase(least);
	}
	auto order = [&](int a, int b) { return items[a].order < items[b].order; };
	std::stable_sort(plan.visible.begin(), plan.visible.end(), order);
	std::stable_sort(plan.overflow.begin(), plan.overflow.end(), order);
	if (more >= 0 && !plan.overflow.empty()) plan.visible.push_back(more);
	return plan;
}
} // namespace q1view
