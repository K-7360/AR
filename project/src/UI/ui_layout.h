/**
 * @file ui_layout.h
 * @brief UI Layout Constants for JBD013 640x480 Display
 */

#ifndef UI_LAYOUT_H
#define UI_LAYOUT_H

#include "display.h"

// Waveform Section
#define WAVEFORM_AREA_X         0
#define WAVEFORM_AREA_Y         0
#define WAVEFORM_AREA_WIDTH     DISPLAY_WIDTH
#define WAVEFORM_AREA_HEIGHT    280

#define WAVEFORM_CH1_Y          0
#define WAVEFORM_CH1_HEIGHT     140
#define WAVEFORM_CH2_Y          140
#define WAVEFORM_CH2_HEIGHT     140

#define WAVEFORM_LABEL_X        10
#define WAVEFORM_LABEL_Y        8
#define WAVEFORM_BORDER_COLOR   GRAY_MID

// Indices Section
#define INDICES_AREA_X          0
#define INDICES_AREA_Y          280
#define INDICES_AREA_WIDTH      DISPLAY_WIDTH
#define INDICES_AREA_HEIGHT     170

// Status Bar Section
#define STATUS_AREA_X           0
#define STATUS_AREA_Y           450
#define STATUS_AREA_WIDTH       DISPLAY_WIDTH
#define STATUS_AREA_HEIGHT      30

// Waveform Parameters
#define WAVEFORM_SAMPLES_VISIBLE    640
#define WAVEFORM_COLOR_CH1      GRAY_WHITE
#define WAVEFORM_COLOR_CH2      GRAY_LIGHT
#define WAVEFORM_COLOR_GRID     GRAY_DARK
#define WAVEFORM_COLOR_BG       GRAY_BLACK

// Index Bar Parameters
#define INDEX_BAR_WIDTH         300
#define INDEX_BAR_HEIGHT        16
#define INDEX_CELL_WIDTH        310
#define INDEX_CELL_HEIGHT       80
#define INDEX_GRID_PADDING_X    10
#define INDEX_GRID_PADDING_Y    5
#define INDEX_GRID_SPACING_X    10
#define INDEX_GRID_SPACING_Y    5
#define INDEX_BAR_BORDER_COLOR  GRAY_MID

// Status Bar Parameters
#define STATUS_TEXT_Y           (STATUS_AREA_Y + 11)
#define STATUS_FONT_SIZE        FONT_SIZE_SMALL
#define STATUS_TEXT_COLOR       GRAY_WHITE
#define STATUS_SIGNAL_X         10
#define STATUS_BATTERY_X        180
#define STATUS_RATE_X           350

#endif
