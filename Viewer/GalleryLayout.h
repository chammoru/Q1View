#pragma once

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <vector>

// MFC's Windows headers may define function-like min/max macros.
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

namespace q1view {
struct GalleryRect {
    float x = 0, y = 0, size = 0;
    bool Contains(float px, float py) const {
        return px >= x && py >= y && px < x + size && py < y + size;
    }
};

// Platform-independent, virtualized layout. Only tiles involved in the visible
// transition have stored rectangles; folder size does not affect frame cost.
class GalleryLayout {
public:
    int count = 0, columns = 1;
    float width = 1, height = 1, gutter = 1, scroll = 0;
    static constexpr double Duration = 0.190;

    float Pitch() const { return std::max(1.0f, width / columns); }
    float MaxScroll() const { return std::max(0.0f, std::ceil(float(count) / columns) * Pitch() - height); }
    bool Animating(double now) const { return !mFrom.empty() && now < mStart + Duration; }
    GalleryRect Target(int index) const {
        return { (index % columns) * Pitch(), (index / columns) * Pitch() - scroll,
            std::max(1.0f, Pitch() - gutter) };
    }
    GalleryRect Rect(int index, double now) const {
        const auto target = Target(index);
        auto it = mFrom.find(index);
        if (it == mFrom.end() || !Animating(now)) return target;
        float t = float(std::max(0.0, std::min(1.0, (now - mStart) / Duration)));
        t = t * t * (3 - 2 * t);
        const auto& from = it->second;
        return { from.x + (target.x - from.x) * t, from.y + (target.y - from.y) * t,
            from.size + (target.size - from.size) * t };
    }
    std::vector<int> Visible(double now, int extraRows = 0) const {
        std::set<int> indices;
        const int first = std::max(0, int(std::floor(scroll / Pitch())) - extraRows);
        const int last = int(std::ceil((scroll + height) / Pitch())) + extraRows;
        for (int i = first * columns; i < std::min(count, last * columns); ++i) indices.insert(i);
        if (Animating(now)) {
            for (const auto& p : mFrom) {
                const auto r = Rect(p.first, now);
                if (r.y + r.size >= -extraRows * Pitch() && r.y <= height + extraRows * Pitch())
                    indices.insert(p.first);
            }
        }
        return {indices.begin(), indices.end()};
    }
    void Reset(int items, int cols, float w, float h, float gap) {
        count = std::max(0, items); columns = std::max(1, cols);
        width = std::max(1.0f, w); height = std::max(1.0f, h); gutter = gap;
        scroll = 0; mFrom.clear();
    }
    void Retarget(int cols, float w, float h, float gap, int selected, double now, bool animate) {
        GalleryLayout previous = *this;
        int anchor = selected;
        auto visible = Visible(now);
        if (anchor < 0 || anchor >= count || Rect(anchor, now).y < 0 ||
            Rect(anchor, now).y + Rect(anchor, now).size > height)
            anchor = visible.empty() ? -1 : visible.front();
        float anchorY = anchor >= 0 ? Rect(anchor, now).y : 0;
        columns = std::max(1, cols); width = std::max(1.0f, w);
        height = std::max(1.0f, h); gutter = gap;
        scroll = anchor >= 0 ? (anchor / columns) * Pitch() - anchorY : 0;
        scroll = std::max(0.0f, std::min(MaxScroll(), scroll));
        mFrom.clear();
        if (animate) {
            auto targetVisible = Visible(now);
            visible.insert(visible.end(), targetVisible.begin(), targetVisible.end());
            for (int i : visible) mFrom[i] = previous.Rect(i, now);
        }
        mStart = now;
    }
    void ScrollTo(float value) {
        value = std::max(0.0f, std::min(MaxScroll(), value));
        float delta = value - scroll;
        for (auto& p : mFrom) p.second.y -= delta;
        scroll = value;
    }
    void Reveal(int index) {
        if (index < 0 || index >= count) return;
        const auto r = Target(index);
        if (r.y < 0) ScrollTo(scroll + r.y);
        else if (r.y + r.size > height) ScrollTo(scroll + r.y + r.size - height);
    }
    int Hit(float x, float y, double now) const {
        auto visible = Visible(now);
        // Last drawn tile wins where animated tiles overlap.
        for (auto i = visible.rbegin(); i != visible.rend(); ++i)
            if (Rect(*i, now).Contains(x, y)) return *i;
        return -1;
    }
private:
    std::map<int, GalleryRect> mFrom;
    double mStart = 0;
};
}
