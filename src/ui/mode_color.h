#pragma once

#include <stdint.h>

#include "command.h"

namespace ui {

struct Rgb {
    uint8_t r, g, b;
};

inline Rgb modeColor(proto::Mode m) {
    switch (m) {
        case proto::Mode::Ble: return {0, 0, 255};     // blue
        case proto::Mode::Http: return {0, 255, 0};    // green
        case proto::Mode::Cwd: return {255, 180, 0};   // yellow
    }
    return {0, 0, 0};
}

}  // namespace ui
