#ifndef HOTSPOTTRACKER_HPP
#define HOTSPOTTRACKER_HPP

#include "P2Pro.hpp"
#include <deque>
#include <cstdint>

class HotSpotTracker {
public:
    struct Sample {
        double x, y, temp;
        uint8_t r, g, b;
    };

    void update(HotSpotResult &res, const P2ProFrame &frame);

private:
    std::deque<Sample> history;
    int lostFrames = 0;

    void applyHistory(HotSpotResult &res);
};

#endif // HOTSPOTTRACKER_HPP
