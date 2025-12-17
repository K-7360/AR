/**
 * @file ui_manager.h
 * @brief Main UI Manager API
 */

#ifndef UI_MANAGER_H
#define UI_MANAGER_H

#include <stdint.h>
#include "display.h"
#include "ui_waveform.h"
#include "ui_indices.h"

typedef struct {
    float focus;
    float relaxation;
    float fatigue;
    float meditation;
} cognitive_indices_t;

typedef struct {
    framebuffer_t framebuffer;
    waveform_display_t ch1_waveform;
    waveform_display_t ch2_waveform;
    index_bar_t focus_bar;
    index_bar_t relax_bar;
    index_bar_t fatigue_bar;
    index_bar_t meditation_bar;
    char signal_status[16];
    uint8_t battery_percent;
    uint16_t sample_rate;
} ui_state_t;

void ui_init(ui_state_t *ui);
void ui_add_sample_ch1(ui_state_t *ui, float sample);
void ui_add_sample_ch2(ui_state_t *ui, float sample);
void ui_update_indices(ui_state_t *ui, cognitive_indices_t *indices);
void ui_set_signal_status(ui_state_t *ui, const char *status);
void ui_set_battery(ui_state_t *ui, uint8_t percent);
void ui_render(ui_state_t *ui);
void ui_flush(ui_state_t *ui);
void ui_clear(ui_state_t *ui);

#endif
