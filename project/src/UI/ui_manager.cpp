/**
 * @file ui_manager.cpp
 * @brief Main UI Manager Implementation
 */

#include "ui_manager.h"
#include <string.h>
#include <stdio.h>

static void render_status_bar(ui_state_t *ui) {
    framebuffer_t *fb = &ui->framebuffer;

    fb_fill_rect(fb, STATUS_AREA_X, STATUS_AREA_Y, STATUS_AREA_WIDTH, STATUS_AREA_HEIGHT, GRAY_BLACK);
    fb_draw_line(fb, STATUS_AREA_X, STATUS_AREA_Y, STATUS_AREA_X + STATUS_AREA_WIDTH - 1, STATUS_AREA_Y, GRAY_DARK);

    fb_draw_string(fb, STATUS_SIGNAL_X, STATUS_TEXT_Y, ui->signal_status, STATUS_FONT_SIZE, STATUS_TEXT_COLOR);

    char battery_text[16];
    snprintf(battery_text, sizeof(battery_text), "Battery: %d%%", ui->battery_percent);
    fb_draw_string(fb, STATUS_BATTERY_X, STATUS_TEXT_Y, battery_text, STATUS_FONT_SIZE, STATUS_TEXT_COLOR);

    char rate_text[24];
    snprintf(rate_text, sizeof(rate_text), "%d Hz - 10s", ui->sample_rate);
    fb_draw_string(fb, STATUS_RATE_X, STATUS_TEXT_Y, rate_text, STATUS_FONT_SIZE, STATUS_TEXT_COLOR);
}

void ui_init(ui_state_t *ui) {
    fb_init(&ui->framebuffer);

    waveform_init(&ui->ch1_waveform, WAVEFORM_CH1_Y, 1);
    waveform_init(&ui->ch2_waveform, WAVEFORM_CH2_Y, 2);

    int16_t col0_x = INDEX_GRID_PADDING_X;
    int16_t col1_x = INDEX_GRID_PADDING_X + INDEX_CELL_WIDTH + INDEX_GRID_SPACING_X;
    int16_t row0_y = INDICES_AREA_Y + INDEX_GRID_PADDING_Y;
    int16_t row1_y = INDICES_AREA_Y + INDEX_GRID_PADDING_Y + INDEX_CELL_HEIGHT + INDEX_GRID_SPACING_Y;

    index_bar_init(&ui->focus_bar, "Focus", col0_x, row0_y);
    index_bar_init(&ui->relax_bar, "Relax", col1_x, row0_y);
    index_bar_init(&ui->fatigue_bar, "Fatigue", col0_x, row1_y);
    index_bar_init(&ui->meditation_bar, "Meditation", col1_x, row1_y);

    strncpy(ui->signal_status, "Signal: Good", sizeof(ui->signal_status));
    ui->battery_percent = 85;
    ui->sample_rate = 250;
}

void ui_add_sample_ch1(ui_state_t *ui, float sample) {
    waveform_add_sample(&ui->ch1_waveform, sample);
}

void ui_add_sample_ch2(ui_state_t *ui, float sample) {
    waveform_add_sample(&ui->ch2_waveform, sample);
}

void ui_update_indices(ui_state_t *ui, cognitive_indices_t *indices) {
    index_bar_update(&ui->focus_bar, indices->focus);
    index_bar_update(&ui->relax_bar, indices->relaxation);
    index_bar_update(&ui->fatigue_bar, indices->fatigue);
    index_bar_update(&ui->meditation_bar, indices->meditation);
}

void ui_set_signal_status(ui_state_t *ui, const char *status) {
    strncpy(ui->signal_status, status, sizeof(ui->signal_status) - 1);
    ui->signal_status[sizeof(ui->signal_status) - 1] = '\0';
}

void ui_set_battery(ui_state_t *ui, uint8_t percent) {
    if (percent > 100) percent = 100;
    ui->battery_percent = percent;
}

void ui_render(ui_state_t *ui) {
    framebuffer_t *fb = &ui->framebuffer;

    fb_clear(fb);
    waveform_render(&ui->ch1_waveform, fb);
    waveform_render(&ui->ch2_waveform, fb);

    fb_fill_rect(fb, INDICES_AREA_X, INDICES_AREA_Y, INDICES_AREA_WIDTH, INDICES_AREA_HEIGHT, GRAY_BLACK);

    index_bar_render(&ui->focus_bar, fb);
    index_bar_render(&ui->relax_bar, fb);
    index_bar_render(&ui->fatigue_bar, fb);
    index_bar_render(&ui->meditation_bar, fb);

    render_status_bar(ui);
}

void ui_flush(ui_state_t *ui) {
    fb_flush_to_display(&ui->framebuffer);
}

void ui_clear(ui_state_t *ui) {
    fb_clear(&ui->framebuffer);
    waveform_clear(&ui->ch1_waveform);
    waveform_clear(&ui->ch2_waveform);
}
