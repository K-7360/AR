/**
 * @file ui_waveform.cpp
 * @brief Waveform Display Implementation
 */

#include "ui_waveform.h"
#include <string.h>

void waveform_init(waveform_display_t *wf, int16_t y_offset, uint8_t channel) {
    memset(wf->samples, 0, sizeof(wf->samples));
    wf->write_index = 0;
    wf->display_width = WAVEFORM_AREA_WIDTH;
    wf->display_height = (channel == 1) ? WAVEFORM_CH1_HEIGHT : WAVEFORM_CH2_HEIGHT;
    wf->y_offset = y_offset;
    wf->channel_num = channel;
    wf->waveform_color = (channel == 1) ? WAVEFORM_COLOR_CH1 : WAVEFORM_COLOR_CH2;
}

void waveform_add_sample(waveform_display_t *wf, float sample) {
    wf->samples[wf->write_index] = sample;
    wf->write_index = (wf->write_index + 1) % WAVEFORM_BUFFER_SIZE;
}

void waveform_render(waveform_display_t *wf, framebuffer_t *fb) {
    // Clear waveform area
    fb_fill_rect(fb, WAVEFORM_AREA_X, wf->y_offset, wf->display_width, wf->display_height, WAVEFORM_COLOR_BG);

    // Draw border
    fb_draw_rect(fb, WAVEFORM_AREA_X, wf->y_offset, wf->display_width, wf->display_height, WAVEFORM_BORDER_COLOR);

    // Draw horizontal grid lines
    for (int i = 0; i < 5; i++) {
        int16_t y = wf->y_offset + (i * wf->display_height / 4);
        fb_draw_line(fb, WAVEFORM_AREA_X, y, WAVEFORM_AREA_X + wf->display_width - 1, y, WAVEFORM_COLOR_GRID);
    }

    // Draw vertical grid lines
    for (int i = 0; i <= 8; i++) {
        int16_t x = WAVEFORM_AREA_X + (i * wf->display_width / 8);
        fb_draw_line(fb, x, wf->y_offset, x, wf->y_offset + wf->display_height - 1, WAVEFORM_COLOR_GRID);
    }

    // Draw channel label
    const char *label = (wf->channel_num == 1) ? "# Channel 1" : "# Channel 2";
    fb_draw_string(fb, WAVEFORM_AREA_X + WAVEFORM_LABEL_X, wf->y_offset + WAVEFORM_LABEL_Y, label, FONT_SIZE_SMALL, GRAY_WHITE);

    // Draw waveform
    uint16_t samples_per_pixel = WAVEFORM_BUFFER_SIZE / wf->display_width;
    if (samples_per_pixel < 1) samples_per_pixel = 1;

    int16_t center_y = wf->y_offset + wf->display_height / 2;
    int16_t y_min = wf->y_offset + 2;
    int16_t y_max = wf->y_offset + wf->display_height - 3;
    float scale = (wf->display_height / 4) / 100.0f;

    uint16_t samples_to_display = wf->display_width * samples_per_pixel;
    uint16_t start_idx = (wf->write_index + WAVEFORM_BUFFER_SIZE - samples_to_display) % WAVEFORM_BUFFER_SIZE;

    int16_t prev_y = center_y;

    for (int x = 0; x < wf->display_width; x++) {
        uint16_t sample_idx = (start_idx + x * samples_per_pixel) % WAVEFORM_BUFFER_SIZE;
        float value = wf->samples[sample_idx];
        int16_t y = center_y - (int16_t)(value * scale);

        if (y < y_min) y = y_min;
        if (y > y_max) y = y_max;

        // Draw vertical line segment connecting prev_y to y
        int16_t px = WAVEFORM_AREA_X + x;
        if (y == prev_y) {
            fb_set_pixel(fb, px, y, wf->waveform_color);
        } else if (y > prev_y) {
            for (int16_t py = prev_y; py <= y; py++) {
                fb_set_pixel(fb, px, py, wf->waveform_color);
            }
        } else {
            for (int16_t py = y; py <= prev_y; py++) {
                fb_set_pixel(fb, px, py, wf->waveform_color);
            }
        }
        prev_y = y;
    }
}

void waveform_clear(waveform_display_t *wf) {
    memset(wf->samples, 0, sizeof(wf->samples));
    wf->write_index = 0;
}
