/**
 * @file display.h
 * @brief 4-bit Grayscale Frame Buffer with PSRAM Support
 */

#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdint.h>
#include <string.h>
#include <esp_heap_caps.h>

#define DISPLAY_WIDTH   640
#define DISPLAY_HEIGHT  480
#define FRAMEBUFFER_SIZE  (DISPLAY_WIDTH * DISPLAY_HEIGHT / 2)  // 153,600 bytes

// Gray Levels (0-15)
#define GRAY_BLACK      0
#define GRAY_DARK       3
#define GRAY_MID_DARK   7
#define GRAY_MID        11
#define GRAY_LIGHT      14
#define GRAY_WHITE      15

// Font Sizes
#define FONT_SIZE_SMALL  0
#define FONT_SIZE_NORMAL 1
#define FONT_WIDTH_SMALL   6
#define FONT_HEIGHT_SMALL  8
#define FONT_WIDTH_NORMAL  8
#define FONT_HEIGHT_NORMAL 12

typedef struct {
    uint8_t *buffer;
} framebuffer_t;

void fb_init(framebuffer_t *fb);
void fb_clear(framebuffer_t *fb);
void fb_set_pixel(framebuffer_t *fb, int16_t x, int16_t y, uint8_t gray);
void fb_draw_line(framebuffer_t *fb, int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t gray);
void fb_draw_rect(framebuffer_t *fb, int16_t x, int16_t y, int16_t w, int16_t h, uint8_t gray);
void fb_fill_rect(framebuffer_t *fb, int16_t x, int16_t y, int16_t w, int16_t h, uint8_t gray);
void fb_draw_char(framebuffer_t *fb, int16_t x, int16_t y, char c, uint8_t font_size, uint8_t gray);
void fb_draw_string(framebuffer_t *fb, int16_t x, int16_t y, const char *str, uint8_t font_size, uint8_t gray);
int16_t fb_get_string_width(const char *str, uint8_t font_size);
void fb_flush_to_display(framebuffer_t *fb);

#endif
