#include "HotSpotTracker.hpp"
#include <cmath>
#include <algorithm>

void HotSpotTracker::update(HotSpotResult &res, const P2ProFrame &frame) {
    if (!res.found) {
        lostFrames++;
        // Persistence: if we recently had a hot spot, keep it for up to 3 frames
        if (lostFrames <= 3 && !history.empty()) {
            res.found = true;
            applyHistory(res);
        } else {
            res.found = false;
            if (lostFrames > 10) history.clear();
        }
        return;
    }

    lostFrames = 0;
    Sample current = {(double) res.x, (double) res.y, res.tempC, res.r, res.g, res.b};

    if (!history.empty()) {
        double lastX = history.back().x;
        double lastY = history.back().y;
        double distSq = std::pow(current.x - lastX, 2) + std::pow(current.y - lastY, 2);

        if (distSq > 20.0 * 20.0) {
            // Threshold for "moving significantly"
            history.clear();
        }
    }

    history.push_back(current);
    if (history.size() > 8) {
        history.pop_front();
    }

    applyHistory(res);
}

void HotSpotTracker::applyHistory(HotSpotResult &res) {
    if (history.empty()) return;

    double avgX = 0, avgY = 0;
    double maxTemp = -1000.0;
    Sample bestSample = history.back();

    for (const auto &s: history) {
        avgX += s.x;
        avgY += s.y;
        if (s.temp > maxTemp) {
            maxTemp = s.temp;
            bestSample = s; // Use color and temp from the hottest sample
        }
    }

    res.x = (int) std::round(avgX / history.size());
    res.y = (int) std::round(avgY / history.size());
    // Use max temperature from buffer as requested: "if any modification is done to temperature, it must use max, not average"
    res.tempC = maxTemp;

    // Use color from the hottest sample for better contrast consistency
    res.r = bestSample.r;
    res.g = bestSample.g;
    res.b = bestSample.b;
}
