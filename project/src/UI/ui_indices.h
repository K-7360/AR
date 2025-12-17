/**
 * @file ui_indices.h
 * @brief Cognitive Index Bar API
 */

#ifndef UI_INDICES_H
#define UI_INDICES_H

#include <stdint.h>
#include "ui_layout.h"
#include "display.h"

typedef struct {
    const char *label;
    float value;
    float target_value;
    int16_t x, y;
} index_bar_t;

void index_bar_init(index_bar_t *bar, const char *label, int16_t x, int16_t y);
void index_bar_update(index_bar_t *bar, float new_value);
void index_bar_render(index_bar_t *bar, framebuffer_t *fb);
void index_bar_set_value(index_bar_t *bar, float value);

#endif
