#include "../Viewer/GalleryLayout.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>

static void Require(bool condition, const char* text) {
    if (!condition) { std::fprintf(stderr, "FAIL: %s\n", text); std::exit(1); }
}
static bool Equal(q1view::GalleryRect a, q1view::GalleryRect b) {
    return std::abs(a.x-b.x) < .02f && std::abs(a.y-b.y) < .02f && std::abs(a.size-b.size) < .02f;
}
int main() {
    q1view::GalleryLayout layout;
    layout.Reset(10000, 5, 500, 800, 1);
    Require(layout.Visible(0).size() == 40, "only visible tiles enumerated");
    layout.ScrollTo(40000); auto before = layout.Visible(0);
    int selected = before[10]; auto selectedBefore = layout.Rect(selected, 0);
    layout.Retarget(2, 500, 800, 1, selected, 1, true);
    Require(Equal(selectedBefore, layout.Rect(selected, 1)), "zoom starts at exact displayed position");
    Require(std::abs(layout.Target(selected).y - selectedBefore.y) < .02, "selected tile vertical anchor preserved");
    auto halfway = layout.Rect(selected, 1.07);
    layout.Retarget(4, 500, 800, 1, selected, 1.07, true);
    Require(Equal(halfway, layout.Rect(selected, 1.07)), "mid-animation reversal is continuous");
    Require(!layout.Animating(2), "animation completes");
    for (int cols = 1; cols <= 5; ++cols) {
        for (float dpi : {1.0f, 1.25f, 1.5f, 2.0f}) {
            layout.Retarget(cols, 420*dpi, 900*dpi, dpi, selected, 3, false);
            layout.Reveal(selected);
            auto r = layout.Rect(selected, 4);
            Require(layout.Hit(r.x+r.size/2, r.y+r.size/2, 4) == selected, "DPI layout/hit test agreement");
            Require(r.y >= -.02 && r.y+r.size <= layout.height+.02, "selection revealed");
            layout.ScrollTo(1e9f);
            Require(layout.scroll == layout.MaxScroll(), "EOF scroll clamps");
            auto end = layout.Visible(4);
            Require(!end.empty() && end.back() == 9999, "last tile reachable");
        }
    }
    layout.Reset(10000, 5, 500, 800, 1);
    for (int j = 0; j < 1000; ++j) {
        double time = j * .007;
        auto oldVisible = layout.Visible(time);
        std::map<int,q1view::GalleryRect> old;
        for (int i : oldVisible) old[i] = layout.Rect(i, time);
        layout.Retarget(j%5+1, 500, 800, 1, -1, time, true);
        for (auto& p : old) Require(Equal(p.second, layout.Rect(p.first,time)), "rapid retarget has no discontinuities");
        Require(layout.Visible(time).size() < 200, "rapid input does not retain folder-sized animation state");
    }
    layout.Reset(0, 5, 0, 0, 2);
    layout.Retarget(1, 0, 0, 2, -1, 0, true);
    Require(layout.Visible(1).empty() && layout.Hit(0,0,1) == -1, "empty/zero-size safe");
    for (int count : {1000, 1000000}) {
        layout.Reset(count, 5, 800, 1800, 2);
        size_t work = 0;
        auto start = std::chrono::steady_clock::now();
        for (int frame = 0; frame < 10000; ++frame) {
            double time = frame / 60.0;
            if (frame%4 == 0) layout.Retarget(frame%5+1,800,1800,2,-1,time,true);
            for (int i : layout.Visible(time)) { work += layout.Rect(i,time).size > 0; }
        }
        double ms = std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()/10000;
        std::printf("%d entries: %.4f ms/layout frame (%zu tiles evaluated)\n",count,ms,work);
    }
    std::puts("Gallery layout tests passed");
}
