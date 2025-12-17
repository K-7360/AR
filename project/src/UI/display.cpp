/**
 * @file display.cpp
 * @brief 4-bit Grayscale Frame Buffer Implementation
 */

#include "display.h"
#include "fonts.h"
#include "jbd013_api.h"

void fb_init(framebuffer_t *fb) {
    fb->buffer = (uint8_t*) heap_caps_malloc(FRAMEBUFFER_SIZE, MALLOC_CAP_SPIRAM);
    memset(fb->buffer, 0x00, FRAMEBUFFER_SIZE);
}

void fb_clear(framebuffer_t *fb) {
    memset(fb->buffer, 0x00, FRAMEBUFFER_SIZE);
}

void fb_set_pixel(framebuffer_t *fb, int16_t x, int16_t y, uint8_t gray) {
    if (x < 0 || x >= DISPLAY_WIDTH || y < 0 || y >= DISPLAY_HEIGHT) return;
    if (gray > 15) gray = 15;

    uint32_t pixel_index = y * DISPLAY_WIDTH + x;
    uint32_t byte_index = pixel_index / 2;

    if (pixel_index & 1) {
        fb->buffer[byte_index] = (fb->buffer[byte_index] & 0xF0) | (gray & 0x0F);
    } else {
        fb->buffer[byte_index] = (fb->buffer[byte_index] & 0x0F) | ((gray & 0x0F) << 4);
    }
}

// Fast horizontal line
static void fb_hline(framebuffer_t *fb, int16_t x0, int16_t x1, int16_t y, uint8_t gray) {
    if (x0 > x1) { int16_t t = x0; x0 = x1; x1 = t; }
    for (int16_t x = x0; x <= x1; x++) {
        fb_set_pixel(fb, x, y, gray);
    }
}

// Fast vertical line
static void fb_vline(framebuffer_t *fb, int16_t x, int16_t y0, int16_t y1, uint8_t gray) {
    if (y0 > y1) { int16_t t = y0; y0 = y1; y1 = t; }
    for (int16_t y = y0; y <= y1; y++) {
        fb_set_pixel(fb, x, y, gray);
    }
}

void fb_draw_line(framebuffer_t *fb, int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t gray) {
    // Fast path for horizontal lines
    if (y0 == y1) {
        fb_hline(fb, x0, x1, y0, gray);
        return;
    }
    // Fast path for vertical lines
    if (x0 == x1) {
        fb_vline(fb, x0, y0, y1, gray);
        return;
    }

    // Bresenham for diagonal lines
    int16_t dx = x1 - x0;
    int16_t dy = y1 - y0;
    int16_t dx_abs = dx < 0 ? -dx : dx;
    int16_t dy_abs = dy < 0 ? -dy : dy;
    int16_t sx = dx < 0 ? -1 : 1;
    int16_t sy = dy < 0 ? -1 : 1;
    int16_t x = x0, y = y0;

    if (dx_abs > dy_abs) {
        int16_t err = dx_abs / 2;
        while (x != x1) {
            fb_set_pixel(fb, x, y, gray);
            err -= dy_abs;
            if (err < 0) { y += sy; err += dx_abs; }
            x += sx;
        }
    } else {
        int16_t err = dy_abs / 2;
        while (y != y1) {
            fb_set_pixel(fb, x, y, gray);
            err -= dx_abs;
            if (err < 0) { x += sx; err += dy_abs; }
            y += sy;
        }
    }
    fb_set_pixel(fb, x1, y1, gray);
}

void fb_draw_rect(framebuffer_t *fb, int16_t x, int16_t y, int16_t w, int16_t h, uint8_t gray) {
    fb_draw_line(fb, x, y, x + w - 1, y, gray);
    fb_draw_line(fb, x, y + h - 1, x + w - 1, y + h - 1, gray);
    fb_draw_line(fb, x, y, x, y + h - 1, gray);
    fb_draw_line(fb, x + w - 1, y, x + w - 1, y + h - 1, gray);
}

void fb_fill_rect(framebuffer_t *fb, int16_t x, int16_t y, int16_t w, int16_t h, uint8_t gray) {
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > DISPLAY_WIDTH) w = DISPLAY_WIDTH - x;
    if (y + h > DISPLAY_HEIGHT) h = DISPLAY_HEIGHT - y;
    if (w <= 0 || h <= 0) return;

    // Fast path: full-width aligned fill using memset
    if (x == 0 && w == DISPLAY_WIDTH) {
        uint8_t fill_byte = (gray << 4) | gray;  // Same gray in both nibbles
        uint32_t row_bytes = DISPLAY_WIDTH / 2;
        uint8_t *row_ptr = fb->buffer + (y * row_bytes);
        memset(row_ptr, fill_byte, row_bytes * h);
        return;
    }

    // Slow path: pixel by pixel
    for (int16_t row = y; row < y + h; row++) {
        for (int16_t col = x; col < x + w; col++) {
            fb_set_pixel(fb, col, row, gray);
        }
    }
}

void fb_draw_char(framebuffer_t *fb, int16_t x, int16_t y, char c, uint8_t font_size, uint8_t gray) {
    int16_t char_width = (font_size == FONT_SIZE_SMALL) ? FONT_WIDTH_SMALL : FONT_WIDTH_NORMAL;
    int16_t char_height = (font_size == FONT_SIZE_SMALL) ? FONT_HEIGHT_SMALL : FONT_HEIGHT_NORMAL;

    uint8_t char_index = (uint8_t)c;
    if (char_index < 32 || char_index > 126) char_index = 32;
    char_index -= 32;

    const uint8_t *char_data = (font_size == FONT_SIZE_SMALL) ? font_small[char_index] : font_normal[char_index];

    for (int16_t col = 0; col < char_width; col++) {
        uint8_t column_data = char_data[col];
        for (int16_t row = 0; row < char_height; row++) {
            if (column_data & (1 << row)) {
                fb_set_pixel(fb, x + col, y + row, gray);
            }
        }
    }
}

void fb_draw_string(framebuffer_t *fb, int16_t x, int16_t y, const char *str, uint8_t font_size, uint8_t gray) {
    int16_t char_width = (font_size == FONT_SIZE_SMALL) ? FONT_WIDTH_SMALL : FONT_WIDTH_NORMAL;
    int16_t cursor_x = x;
    while (*str) {
        fb_draw_char(fb, cursor_x, y, *str++, font_size, gray);
        cursor_x += char_width;
    }
}

int16_t fb_get_string_width(const char *str, uint8_t font_size) {
    int16_t char_width = (font_size == FONT_SIZE_SMALL) ? FONT_WIDTH_SMALL : FONT_WIDTH_NORMAL;
    int16_t len = 0;
    while (*str++) len++;
    return len * char_width;
}

void fb_flush_to_display(framebuffer_t *fb) {
    display_image(fb->buffer, FRAMEBUFFER_SIZE, 0, 0);
}
