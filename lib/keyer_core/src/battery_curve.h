// Single-cell LiPo voltage to state of charge. Pure C++.
#pragma once

#include <stdint.h>

namespace keyer {

// Open-circuit-ish discharge curve of a small LiPo cell, linear between points.
inline uint8_t batteryPercent(uint32_t mv) {
    static const struct {
        uint16_t mv;
        uint8_t pct;
    } CURVE[] = {
        {4200, 100}, {4150, 95}, {4110, 90}, {4020, 80}, {3950, 70}, {3870, 60}, {3830, 50},
        {3790, 40},  {3750, 30}, {3700, 20}, {3600, 10}, {3500, 5},  {3300, 0},
    };
    const unsigned n = sizeof(CURVE) / sizeof(CURVE[0]);
    if (mv >= CURVE[0].mv) return 100;
    if (mv <= CURVE[n - 1].mv) return 0;
    for (unsigned i = 1; i < n; i++) {
        if (mv >= CURVE[i].mv) {
            uint32_t hiMv = CURVE[i - 1].mv, loMv = CURVE[i].mv;
            uint32_t hiP = CURVE[i - 1].pct, loP = CURVE[i].pct;
            return uint8_t(loP + (mv - loMv) * (hiP - loP) / (hiMv - loMv));
        }
    }
    return 0;
}

}  // namespace keyer
