#pragma once

#include <algorithm>
#include <set>

namespace q1view {
// Shared selection semantics for native lists and the owner-drawn gallery.
struct BrowserSelection {
    std::set<int> selected;
    int focus = -1;
    int anchor = -1;

    void clear() { selected.clear(); focus = anchor = -1; }
    bool contains(int index) const { return selected.count(index) != 0; }

    template<class IsMedia>
    void click(int index, int count, bool control, bool shift, IsMedia isMedia) {
        if (index < 0 || index >= count) {
            if (!control && !shift) clear();
            return;
        }
        focus = index;
        if (shift) {
            if (anchor < 0 || anchor >= count) anchor = index;
            if (!control) selected.clear();
            for (int i = (std::min)(anchor, index); i <= (std::max)(anchor, index); ++i)
                if (isMedia(i)) selected.insert(i);
        } else if (control) {
            if (!isMedia(index)) return;
            for (auto it = selected.begin(); it != selected.end(); )
                if (!isMedia(*it)) it = selected.erase(it); else ++it;
            if (!selected.erase(index)) selected.insert(index);
            anchor = index;
        } else {
            selected = {index};
            anchor = index;
        }
    }

    template<class IsMedia>
    void all(int count, IsMedia isMedia) {
        selected.clear();
        for (int i = 0; i < count; ++i) if (isMedia(i)) selected.insert(i);
        if (!selected.empty() && !contains(focus)) focus = *selected.begin();
        anchor = focus;
    }
};
}
