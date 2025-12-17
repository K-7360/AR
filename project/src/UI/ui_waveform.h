/**
 * @file ui_waveform.h
 * @brief Waveform Display API
 */

#ifndef UI_WAVEFORM_H
#define UI_WAVEFORM_H

#include <stdint.h>
#include "ui_layout.h"
#include "display.h"

#define WAVEFORM_BUFFER_SIZE    2560  // 10.24 seconds @ 250 Hz (640 pixels × 4 samples/pixel)

typedef struct {
    float samples[WAVEFORM_BUFFER_SIZE];
    uint16_t write_index;
    uint16_t display_width;
    uint16_t display_height;
    int16_t y_offset;
    uint8_t channel_num;
    uint8_t waveform_color;
} waveform_display_t;

void waveform_init(waveform_display_t *wf, int16_t y_offset, uint8_t channel);
void waveform_add_sample(waveform_display_t *wf, float sample);
void waveform_render(waveform_display_t *wf, framebuffer_t *fb);
void waveform_clear(waveform_display_t *wf);

#endif
