#include "core/key_output.h"

#include <driver/gpio.h>

namespace key_output {

void init() {
    gpio_set_level(gpio_num_t(KEY_PIN), 0);  // level first, so the pin never drives high
    gpio_config_t cfg = {};
    cfg.pin_bit_mask = 1ULL << KEY_PIN;
    cfg.mode = GPIO_MODE_OUTPUT;
    cfg.pull_up_en = GPIO_PULLUP_DISABLE;
    cfg.pull_down_en = GPIO_PULLDOWN_ENABLE;
    cfg.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&cfg);
    gpio_set_level(gpio_num_t(KEY_PIN), 0);
}

void set(bool on) { gpio_set_level(gpio_num_t(KEY_PIN), on ? 1 : 0); }

}  // namespace key_output

// Safety rule 1: open the output before any other static constructor or setup().
__attribute__((constructor(101))) static void keyOffAtBoot() { key_output::init(); }
