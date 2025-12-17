/**
 * @file ui_indices.cpp
 * @brief Cognitive Index Bar Implementation
 */

#include "ui_indices.h"
#include <stdio.h>

#define INDEX_ANIMATION_ALPHA  0.2f

void index_bar_init(index_bar_t *bar, const char *label, int16_t x, int16_t y) {
    bar->label = label;
    bar->value = 0.0f;
    bar->target_value = 0.0f;
    bar->x = x;
    bar->y = y;
}

void index_bar_update(index_bar_t *bar, float new_value) {
    if (new_value < 0.0f) new_value = 0.0f;
    if (new_value > 100.0f) new_value = 100.0f;
    bar->target_value = new_value;
    bar->value = bar->value * (1.0f - INDEX_ANIMATION_ALPHA) + bar->target_value * INDEX_ANIMATION_ALPHA;
}

void index_bar_render(index_bar_t *bar, framebuffer_t *fb) {
    // Draw label
    fb_draw_string(fb, bar->x, bar->y, bar->label, FONT_SIZE_NORMAL, GRAY_WHITE);

    // Draw value (right-aligned)
    char value_str[8];
    snprintf(value_str, sizeof(value_str), "%d", (int)bar->value);
    int16_t value_width = fb_get_string_width(value_str, FONT_SIZE_NORMAL);
    fb_draw_string(fb, bar->x + INDEX_BAR_WIDTH - value_width, bar->y, value_str, FONT_SIZE_NORMAL, GRAY_WHITE);

    // Draw bar
    int16_t bar_y = bar->y + FONT_HEIGHT_NORMAL + 8;
    fb_fill_rect(fb, bar->x, bar_y, INDEX_BAR_WIDTH, INDEX_BAR_HEIGHT, GRAY_BLACK);
    fb_draw_rect(fb, bar->x, bar_y, INDEX_BAR_WIDTH, INDEX_BAR_HEIGHT, INDEX_BAR_BORDER_COLOR);

    // Draw filled portion
    int16_t fill_width = (int16_t)((INDEX_BAR_WIDTH - 2) * bar->value / 100.0f);
    if (fill_width > 0) {
        fb_fill_rect(fb, bar->x + 1, bar_y + 1, fill_width, INDEX_BAR_HEIGHT - 2, GRAY_WHITE);
    }
}

void index_bar_set_value(index_bar_t *bar, float value) {
    if (value < 0.0f) value = 0.0f;
    if (value > 100.0f) value = 100.0f;
    bar->value = value;
    bar->target_value = value;
}
